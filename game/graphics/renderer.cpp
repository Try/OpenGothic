#include "renderer.h"

#include <Tempest/Color>
#include <Tempest/Fence>
#include <Tempest/Log>
#include <Tempest/StorageImage>

#include "ui/inventorymenu.h"
#include "camera.h"
#include "gothic.h"
#include "ui/videowidget.h"
#include "utils/string_frm.h"

#include <ui/videowidget.h>

using namespace Tempest;

static const bool skyPathTrace = false;

static uint32_t nextPot(uint32_t x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x++;
  return x;
  }

static float smoothstep(float edge0, float edge1, float x) {
  float t = std::min(std::max((x - edge0) / (edge1 - edge0), 0.f), 1.f);
  return t * t * (3.f - 2.f * t);
  };

static float linearstep(float edge0, float edge1, float x) {
  float t = std::min(std::max((x - edge0) / (edge1 - edge0), 0.f), 1.f);
  return t;
  };

static Size tileCount(Size sz, int s) {
  sz.w = (sz.w+s-1)/s;
  sz.h = (sz.h+s-1)/s;
  return sz;
  }

Renderer::Renderer(Tempest::Swapchain& swapchain)
  : swapchain(swapchain) {
  auto& device = Resources::device();

  static const TextureFormat sfrm[] = {
    TextureFormat::Depth16,
    TextureFormat::Depth24x8,
    TextureFormat::Depth32F,
    };

  for(auto& i:sfrm) {
    if(device.properties().hasDepthFormat(i) && device.properties().hasSamplerFormat(i)) {
      shadowFormat = i;
      break;
      }
    }

  static const TextureFormat zfrm[] = {
    TextureFormat::Depth32F,
    TextureFormat::Depth24x8,
    TextureFormat::Depth16,
    };
  for(auto& i:zfrm) {
    if(device.properties().hasDepthFormat(i) && device.properties().hasSamplerFormat(i)){
      zBufferFormat = i;
      break;
      }
    }

  // crappy rasbery-pi like hardware
  if(!device.properties().hasStorageFormat(sky.lutRGBAFormat))
    sky.lutRGBAFormat = Tempest::TextureFormat::RGBA8;
  if(!device.properties().hasStorageFormat(sky.lutRGBFormat))
    sky.lutRGBFormat = Tempest::TextureFormat::RGBA8;

  Log::i("GPU = ",device.properties().name);
  Log::i("Depth format = ", Tempest::formatName(zBufferFormat), " Shadow format = ", Tempest::formatName(shadowFormat));

  Gothic::inst().onSettingsChanged.bind(this,&Renderer::setupSettings);
  Gothic::inst().toggleGi  .bind(this, &Renderer::toggleGi);
  Gothic::inst().toggleVsm .bind(this, &Renderer::toggleVsm);
  Gothic::inst().toggleRtsm.bind(this, &Renderer::toggleRtsm);

  settings.giEnabled   = Gothic::options().doRtGi;
  settings.vsmEnabled  = Gothic::options().doVirtualShadow;
  settings.rtsmEnabled = Gothic::options().doSoftwareShadow;
  settings.swrEnabled  = Gothic::options().swRenderingPreset>0;

  sky.cloudsLut     = device.image2d   (sky.lutRGBAFormat,  2,  1);
  sky.transLut      = device.attachment(sky.lutRGBFormat, 256, 64);
  sky.multiScatLut  = device.attachment(sky.lutRGBFormat,  32, 32);
  sky.viewLut       = device.attachment(Tempest::TextureFormat::RGBA32F, 128, 64);
  sky.viewCldLut    = device.attachment(Tempest::TextureFormat::RGBA32F, 512, 256);
  sky.irradianceLut = device.image2d(TextureFormat::RGBA32F, 3,2);

  setupSettings();
  }

Renderer::~Renderer() {
  Gothic::inst().onSettingsChanged.ubind(this,&Renderer::setupSettings);
  }

void Renderer::resetSwapchain() {
  auto& device = Resources::device();
  device.waitIdle();

  const auto     res    = internalResolution();
  const uint32_t w      = uint32_t(res.w);
  const uint32_t h      = uint32_t(res.h);
  const uint32_t smSize = settings.shadowResolution;

  sceneLinear = device.attachment(TextureFormat::R11G11B10UF,w,h);

  if(settings.aaEnabled) {
    cmaa2.workingEdges               = device.image2d(TextureFormat::R8, (w + 1) / 2, h);
    cmaa2.shapeCandidates            = device.ssbo(Tempest::Uninitialized, w * h / 4 * sizeof(uint32_t));
    cmaa2.deferredBlendLocationList  = device.ssbo(Tempest::Uninitialized, (w * h + 3) / 6 * sizeof(uint32_t));
    cmaa2.deferredBlendItemList      = device.ssbo(Tempest::Uninitialized, w * h * sizeof(uint32_t));
    cmaa2.deferredBlendItemListHeads = device.image2d(TextureFormat::R32U, (w + 1) / 2, (h + 1) / 2);
    cmaa2.controlBuffer              = device.ssbo(nullptr, 5 * sizeof(uint32_t));
    cmaa2.indirectBuffer             = device.ssbo(nullptr, sizeof(DispatchIndirectCommand) + sizeof(DrawIndirectCommand));
    }

  zbuffer        = device.zbuffer(zBufferFormat,w,h);
  if(w!=swapchain.w() || h!=swapchain.h())
    zbufferUi = device.zbuffer(zBufferFormat, swapchain.w(), swapchain.h()); else
    zbufferUi = ZBuffer();

  uint32_t pw = nextPot(w);
  uint32_t ph = nextPot(h);

  uint32_t hw = pw;
  uint32_t hh = ph;
  while(hw>64 || hh>64) {
    hw = std::max(1u, (hw+1)/2u);
    hh = std::max(1u, (hh+1)/2u);
    }

  hiz.atomicImg = device.properties().hasAtomicFormat(TextureFormat::R32U);
  hiz.hiZ       = device.image2d(TextureFormat::R16,  hw, hh, true);
  if(hiz.atomicImg) {
    hiz.counter = device.image2d(TextureFormat::R32U, std::max(hw/4, 1u), std::max(hh/4, 1u), false);
    } else {
    hiz.counterBuf = device.ssbo(Tempest::Uninitialized, std::max(hw/4, 1u)*std::max(hh/4, 1u)*sizeof(uint32_t));
    }

  if(smSize>0) {
    for(int i=0; i<Resources::ShadowLayers; ++i) {
      if(settings.vsmEnabled && !(settings.giEnabled && i==1) && !(sky.quality==PathTrace && i==1))
        continue; //TODO: support vsm in gi code
      if(settings.rtsmEnabled && !(settings.giEnabled && i==1) && !(sky.quality!=None && i==1))
        continue; //TODO: support vsm in gi code
      shadowMap[i] = device.zbuffer(shadowFormat,smSize,smSize);
      }
    }

  sceneOpaque = device.attachment(TextureFormat::R11G11B10UF,w,h);
  sceneDepth  = device.attachment(TextureFormat::R32F,       w,h);

  gbufDiffuse = device.attachment(TextureFormat::RGBA8,w,h);
  gbufNormal  = device.attachment(TextureFormat::R32U, w,h);

  if(settings.rtsmEnabled)
    shadow.directLightPso = &shaders.rtsmDirectLight;
  else if(settings.vsmEnabled)
    shadow.directLightPso = &shaders.vsmDirectLight; //TODO: naming
  else if(Gothic::options().doRayQuery && Resources::device().properties().descriptors.nonUniformIndexing &&
     settings.shadowResolution>0)
    shadow.directLightPso = &shaders.directLightRq;
  else if(settings.shadowResolution>0)
    shadow.directLightPso = &shaders.directLightSh;
  else
    shadow.directLightPso = &shaders.directLight;

  if(settings.vsmEnabled)
    lights.directLightPso = &shaders.lightsVsm;
  else if(Gothic::options().doRayQuery && Resources::device().properties().descriptors.nonUniformIndexing)
    lights.directLightPso = &shaders.lightsRq;
  else
    lights.directLightPso = &shaders.lights;

  ssao.ssaoBuf  = device.image2d(ssao.aoFormat, w, h);
  ssao.ssaoBlur = device.image2d(ssao.aoFormat, w, h);

  if(settings.vsmEnabled) {
    vsm.pageTbl  = device.image3d(TextureFormat::R32U, 32, 32, 16);
    vsm.pageHiZ  = device.image3d(TextureFormat::R32U, 32, 32, 16);
    vsm.pageData = device.zbuffer(shadowFormat, 8192, 8192);
    vsm.vsmDbg   = device.image2d(TextureFormat::R32U, uint32_t(zbuffer.w()), uint32_t(zbuffer.h()));

    // vsm.ssTrace  = device.image2d(TextureFormat::RGBA8, w, h);
    vsm.ssTrace  = device.image2d(TextureFormat::R32U, w, h);
    vsm.epTrace  = device.image2d(TextureFormat::R16,  1024, 2*1024);
    vsm.fogDbg   = device.image2d(sky.lutRGBFormat,    1024, 2*1024);
    vsm.epipoles = device.ssbo(nullptr, shaders.vsmFogEpipolar.sizeofBuffer(3, size_t(vsm.epTrace.h())));

    auto pageCount    = uint32_t(vsm.pageData.w()/VSM_PAGE_SIZE) * uint32_t(vsm.pageData.h()/VSM_PAGE_SIZE);
    auto pageSsboSize = shaders.vsmClear.sizeofBuffer(0, pageCount);

    vsm.pageList      = device.ssbo(nullptr, pageSsboSize);
    vsm.pageListTmp   = device.ssbo(nullptr, shaders.vsmAllocPages.sizeofBuffer(3, pageCount));
    }

  if(settings.swrEnabled) {
    //swr.outputImage = device.image2d(Tempest::RGBA8, w, h);
    swr.outputImage = device.image2d(Tempest::R32U, w, h);
    }

  if(settings.rtsmEnabled) {
    // rtsm.outputImage = device.image2d(Tempest::RGBA8, w, h);
    rtsm.outputImage = device.image2d(TextureFormat::R8, w, h);
    }

  resetSkyFog();
  initGiData();
  prepareUniforms();
  }

void Renderer::setupSettings() {
  settings.zEnvMappingEnabled = Gothic::settingsGetI("ENGINE","zEnvMappingEnabled")!=0;
  settings.zCloudShadowScale  = Gothic::settingsGetI("ENGINE","zCloudShadowScale") !=0;
  settings.zFogRadial         = Gothic::settingsGetI("RENDERER_D3D","zFogRadial")!=0;

  settings.zVidBrightness     = Gothic::settingsGetF("VIDEO","zVidBrightness");
  settings.zVidContrast       = Gothic::settingsGetF("VIDEO","zVidContrast");
  settings.zVidGamma          = Gothic::settingsGetF("VIDEO","zVidGamma");

  settings.sunSize            = Gothic::settingsGetF("SKY_OUTDOOR","zSunSize");
  settings.moonSize           = Gothic::settingsGetF("SKY_OUTDOOR","zMoonSize");
  if(settings.sunSize<=1)
    settings.sunSize = 200;
  if(settings.moonSize<=1)
    settings.moonSize = 400;

  auto prevVidResIndex = settings.vidResIndex;
  settings.vidResIndex = Gothic::inst().settingsGetF("INTERNAL","vidResIndex");
  settings.aaEnabled = (Gothic::options().aaPreset>0) && (settings.vidResIndex==0);

  {
    auto q = Quality::VolumetricLQ;
    if(!settings.zFogRadial) {
      q = Quality::VolumetricLQ;
      } else {
      q = Quality::VolumetricHQ;
      // q = Quality::Epipolar;
      }

    if(skyPathTrace)
      q = PathTrace;

    if(sky.quality!=q) {
      sky.quality = q;
      resetSkyFog();
      }
  }

  if(prevVidResIndex!=settings.vidResIndex) {
    resetSwapchain();
    }
  }

void Renderer::toggleGi() {
  auto& device = Resources::device();
  if(!Gothic::options().doRayQuery)
    return;

  auto prop = device.properties();
  if(prop.tex2d.maxSize<4096 || !prop.hasStorageFormat(R11G11B10UF) || !prop.hasStorageFormat(R16))
    return;

  settings.giEnabled = !settings.giEnabled;
  gi.fisrtFrame = true;

  device.waitIdle();
  if(auto wview  = Gothic::inst().worldView()) {
    wview->resetRendering();
    }
  if(settings.giEnabled && settings.vsmEnabled) {
    // need a projective shadow, for gi to
    resetSwapchain();
    }
  initGiData();
  prepareUniforms();
  }

void Renderer::toggleVsm() {
  if(!Shaders::isVsmSupported())
    return;

  settings.vsmEnabled = !settings.vsmEnabled;

  auto& device = Resources::device();
  device.waitIdle();

  for(int i=0; i<Resources::ShadowLayers; ++i)
    shadowMap[i] = Tempest::ZBuffer();

  if(auto wview  = Gothic::inst().worldView()) {
    wview->resetRendering();
    }
  resetSwapchain();
  }

void Renderer::toggleRtsm() {
  if(!Shaders::isRtsmSupported())
    return;

  settings.rtsmEnabled = !settings.rtsmEnabled;

  auto& device = Resources::device();
  device.waitIdle();

  for(int i=0; i<Resources::ShadowLayers; ++i)
    shadowMap[i] = Tempest::ZBuffer();

  if(auto wview  = Gothic::inst().worldView()) {
    wview->resetRendering();
    }
  resetSwapchain();
  }

void Renderer::onWorldChanged() {
  gi.fisrtFrame = true;
  resetSkyFog();
  prepareUniforms();
  }

void Renderer::updateCamera(const Camera& camera) {
  proj        = camera.projective();
  viewProj    = camera.viewProj();
  viewProjLwc = camera.viewProjLwc();

  if(auto wview=Gothic::inst().worldView()) {
    for(size_t i=0; i<Resources::ShadowLayers; ++i)
      shadowMatrix[i] = camera.viewShadow(wview->mainLight().dir(),i);
    shadowMatrixVsm = camera.viewShadowVsm(wview->mainLight().dir());
    }

  auto zNear = camera.zNear();
  auto zFar  = camera.zFar();
  clipInfo.x  = zNear*zFar;
  clipInfo.y  = zNear-zFar;
  clipInfo.z  = zFar;
  }

void Renderer::prepareUniforms() {
  auto wview = Gothic::inst().worldView();
  if(wview==nullptr)
    return;

  const Texture2d* sh[Resources::ShadowLayers] = {};
  for(size_t i=0; i<Resources::ShadowLayers; ++i)
    if(!shadowMap[i].isEmpty()) {
      sh[i] = &textureCast<const Texture2d&>(shadowMap[i]);
      }
  wview->setShadowMaps(sh);
  wview->setVirtualShadowMap(vsm.pageData, vsm.pageTbl, vsm.pageHiZ, vsm.pageList);

  wview->setHiZ(textureCast<const Texture2d&>(hiz.hiZ));
  wview->setGbuffer(textureCast<const Texture2d&>(gbufDiffuse), textureCast<const Texture2d&>(gbufNormal));
  wview->setSceneImages(textureCast<const Texture2d&>(sceneOpaque), textureCast<const Texture2d&>(sceneDepth), zbuffer);
  }

void Renderer::resetSkyFog() {
  auto& device = Resources::device();

  const auto     res = internalResolution();
  const uint32_t w   = uint32_t(res.w);
  const uint32_t h   = uint32_t(res.h);

  Resources::recycle(std::move(sky.fogLut3D));
  Resources::recycle(std::move(sky.fogLut3DMs));
  Resources::recycle(std::move(sky.occlusionLut));

  sky.lutIsInitialized = false;

  switch(sky.quality) {
    case None:
    case VolumetricLQ:
      sky.fogLut3D     = device.image3d(sky.lutRGBAFormat, 160, 90, 64);
      sky.occlusionLut = StorageImage();
      break;
    case VolumetricHQ:
      // fogLut and oclussion are decupled
      sky.fogLut3D      = device.image3d(sky.lutRGBFormat,  128,64,32);
      sky.fogLut3DMs    = device.image3d(sky.lutRGBAFormat, 128,64,32);
      sky.occlusionLut  = device.image2d(TextureFormat::R32U, w, h);
      break;
    case Epipolar:
      sky.fogLut3D      = device.image3d(sky.lutRGBFormat,  128,64,32);
      sky.fogLut3DMs    = device.image3d(sky.lutRGBAFormat, 128,64,32);
      sky.occlusionLut  = device.image2d(TextureFormat::R32U, w, h); //TODO: remove
      break;
    case PathTrace:
      break;
    }
  }

void Renderer::prepareSky(Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview) {
  auto& scene   = wview.sceneGlobals();

  cmd.setDebugMarker("Sky LUT");
  if(!sky.lutIsInitialized) {
    sky.lutIsInitialized = true;

    cmd.setFramebuffer({});
    cmd.setBinding(0, sky.cloudsLut);
    cmd.setBinding(5, *wview.sky().cloudsDay()  .lay[0], Sampler::trillinear());
    cmd.setBinding(6, *wview.sky().cloudsDay()  .lay[1], Sampler::trillinear());
    cmd.setBinding(7, *wview.sky().cloudsNight().lay[0], Sampler::trillinear());
    cmd.setBinding(8, *wview.sky().cloudsNight().lay[1], Sampler::trillinear());
    cmd.setPipeline(shaders.cloudsLut);
    cmd.dispatchThreads(size_t(sky.cloudsLut.w()), size_t(sky.cloudsLut.h()));

    auto sz = Vec2(float(sky.transLut.w()), float(sky.transLut.h()));
    cmd.setFramebuffer({{sky.transLut, Tempest::Discard, Tempest::Preserve}});
    cmd.setBinding(5, *wview.sky().cloudsDay()  .lay[0], Sampler::trillinear());
    cmd.setBinding(6, *wview.sky().cloudsDay()  .lay[1], Sampler::trillinear());
    cmd.setBinding(7, *wview.sky().cloudsNight().lay[0], Sampler::trillinear());
    cmd.setBinding(8, *wview.sky().cloudsNight().lay[1], Sampler::trillinear());
    cmd.setPushData(&sz, sizeof(sz));
    cmd.setPipeline(shaders.skyTransmittance);
    cmd.draw(Resources::fsqVbo());

    sz = Vec2(float(sky.multiScatLut.w()), float(sky.multiScatLut.h()));
    cmd.setFramebuffer({{sky.multiScatLut, Tempest::Discard, Tempest::Preserve}});
    cmd.setBinding(0, sky.transLut, Sampler::bilinear(ClampMode::ClampToEdge));
    cmd.setPushData(&sz, sizeof(sz));
    cmd.setPipeline(shaders.skyMultiScattering);
    cmd.draw(Resources::fsqVbo());
    }

  auto sz = Vec2(float(sky.viewLut.w()), float(sky.viewLut.h()));
  cmd.setFramebuffer({{sky.viewLut, Tempest::Discard, Tempest::Preserve}});
  cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, sky.transLut,     Sampler::bilinear(ClampMode::ClampToEdge));
  cmd.setBinding(2, sky.multiScatLut, Sampler::bilinear(ClampMode::ClampToEdge));
  cmd.setBinding(3, sky.cloudsLut,    Sampler::bilinear(ClampMode::ClampToEdge));
  cmd.setPushData(&sz, sizeof(sz));
  cmd.setPipeline(shaders.skyViewLut);
  cmd.draw(Resources::fsqVbo());

  sz = Vec2(float(sky.viewCldLut.w()), float(sky.viewCldLut.h()));
  cmd.setFramebuffer({{sky.viewCldLut, Tempest::Discard, Tempest::Preserve}});
  cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, sky.viewLut);
  cmd.setBinding(2, *wview.sky().cloudsDay()  .lay[0], Sampler::trillinear());
  cmd.setBinding(3, *wview.sky().cloudsDay()  .lay[1], Sampler::trillinear());
  cmd.setBinding(4, *wview.sky().cloudsNight().lay[0], Sampler::trillinear());
  cmd.setBinding(5, *wview.sky().cloudsNight().lay[1], Sampler::trillinear());
  cmd.setPushData(&sz, sizeof(sz));
  cmd.setPipeline(shaders.skyViewCldLut);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::draw(Encoder<CommandBuffer>& cmd, uint8_t cmdId, size_t imgId,
                    VectorImage::Mesh& uiLayer, VectorImage::Mesh& numOverlay,
                    InventoryMenu& inventory, VideoWidget& video) {
  auto& result = swapchain[imgId];

  if(!video.isActive()) {
    draw(result, cmd, cmdId);
    } else {
    cmd.setFramebuffer({{result, Vec4(), Tempest::Preserve}});
    }
  cmd.setFramebuffer({{result, Tempest::Preserve, Tempest::Preserve}});
  cmd.setDebugMarker("UI");
  uiLayer.draw(cmd);

  if(inventory.isOpen()!=InventoryMenu::State::Closed) {
    auto& zb = (zbufferUi.isEmpty() ? zbuffer : zbufferUi);
    cmd.setFramebuffer({{result, Tempest::Preserve, Tempest::Preserve}},{zb, 1.f, Tempest::Preserve});
    cmd.setDebugMarker("Inventory");
    inventory.draw(cmd);

    cmd.setFramebuffer({{result, Tempest::Preserve, Tempest::Preserve}});
    cmd.setDebugMarker("Inventory-counters");
    numOverlay.draw(cmd);
    }
  }

void Renderer::dbgDraw(Tempest::Painter& p) {
  static bool dbg = false;
  if(!dbg)
    return;

  std::vector<const Texture2d*> tex;
  //tex.push_back(&textureCast(swr.outputImage));
  //tex.push_back(&textureCast(hiz.hiZ));
  //tex.push_back(&textureCast(hiz.smProj));
  //tex.push_back(&textureCast(hiz.hiZSm1));
  //tex.push_back(&textureCast(shadowMap[1]));
  //tex.push_back(&textureCast<const Texture2d&>(shadowMap[0]));
  //tex.push_back(&textureCast<const Texture2d&>(vsm.pageData));
  tex.push_back(&textureCast<const Texture2d&>(vsm.ssTrace));

  static int size = 400;
  int left = 10;
  for(auto& t:tex) {
    p.setBrush(Brush(*t,Painter::NoBlend,ClampMode::ClampToBorder));
    auto sz = Size(p.brush().w(),p.brush().h());
    if(sz.isEmpty())
      continue;
    while(sz.w<size && sz.h<size) {
      sz.w *= 2;
      sz.h *= 2;
      }
    while(sz.w>size*2 || sz.h>size*2) {
      sz.w = (sz.w+1)/2;
      sz.h = (sz.h+1)/2;
      }
    p.drawRect(left,50,sz.w,sz.h,
               0,0,p.brush().w(),p.brush().h());
    left += (sz.w+10);
    }
  }

void Renderer::draw(Tempest::Attachment& result, Encoder<CommandBuffer>& cmd, uint8_t fId) {
  auto wview  = Gothic::inst().worldView();
  auto camera = Gothic::inst().camera();
  if(wview==nullptr || camera==nullptr) {
    cmd.setFramebuffer({{result, Vec4(), Tempest::Preserve}});
    return;
    }

  wview->updateRtScene();
  wview->updateLights();

  updateCamera(*camera);

  static bool updFr = true;
  if(updFr){
    if(wview->mainLight().dir().y>Camera::minShadowY) {
      frustrum[SceneGlobals::V_Shadow0].make(shadowMatrix[0],shadowMap[0].w(),shadowMap[0].h());
      frustrum[SceneGlobals::V_Shadow1].make(shadowMatrix[1],shadowMap[1].w(),shadowMap[1].h());
      } else {
      frustrum[SceneGlobals::V_Shadow0].clear();
      frustrum[SceneGlobals::V_Shadow1].clear();
      }
    frustrum[SceneGlobals::V_Main].make(viewProj,zbuffer.w(),zbuffer.h());
    frustrum[SceneGlobals::V_HiZ] = frustrum[SceneGlobals::V_Main];
    frustrum[SceneGlobals::V_Vsm] = frustrum[SceneGlobals::V_Shadow1]; //TODO: remove
    wview->updateFrustrum(frustrum);
    }

  wview->preFrameUpdate(*camera,Gothic::inst().world()->tickCount(),fId);
  wview->prepareGlobals(cmd,fId);

  wview->visibilityPass(cmd, 0);
  prepareSky(cmd,*wview);

  drawHiZ (cmd, *wview);
  buildHiZ(cmd);

  wview->visibilityPass(cmd, 1);
  drawGBuffer(cmd,fId,*wview);

  drawShadowMap(cmd,fId,*wview);
  drawVsm(cmd, *wview);
  drawSwr(cmd, *wview);
  drawRtsm(cmd, *wview);

  prepareIrradiance(cmd,*wview);
  prepareExposure(cmd,*wview);
  prepareSSAO(cmd,*wview);
  prepareFog (cmd,*wview);
  prepareGi  (cmd,*wview);

  cmd.setFramebuffer({{sceneLinear, Tempest::Discard, Tempest::Preserve}}, {zbuffer, Tempest::Readonly});
  drawShadowResolve(cmd,*wview);
  drawAmbient(cmd,*wview);
  drawLights(cmd,*wview);
  drawSky(cmd,*wview);

  stashSceneAux(cmd);

  drawGWater(cmd, *wview);

  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}}, {zbuffer, Tempest::Preserve, Tempest::Preserve});
  cmd.setDebugMarker("Sun&Moon");
  drawSunMoon(cmd, *wview);
  cmd.setDebugMarker("Translucent");
  wview->drawTranslucent(cmd, fId);

  drawProbesDbg(cmd, *wview);
  drawProbesHitDbg(cmd);
  drawVsmDbg(cmd, *wview);
  drawSwrDbg(cmd, *wview);
  drawRtsmDbg(cmd, *wview);

  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}});
  drawReflections(cmd, *wview);
  if(camera->isInWater()) {
    cmd.setDebugMarker("Underwater");
    drawUnderwater(cmd, *wview);
    } else {
    cmd.setDebugMarker("Fog");
    drawFog(cmd, *wview);
    }

  if(settings.aaEnabled) {
    cmd.setDebugMarker("CMAA2 & Tonemapping");
    drawCMAA2(result, cmd, *wview);
    } else {
    cmd.setDebugMarker("Tonemapping");
    drawTonemapping(result, cmd, *wview);
    }

  wview->postFrameupdate();
  }

void Renderer::drawTonemapping(Attachment& result, Encoder<CommandBuffer>& cmd, const WorldView& wview) {
  struct Push {
    float brightness = 0;
    float contrast   = 1;
    float gamma      = 1.f/2.2f;
    float mul        = 1;
    };

  Push p;
  p.brightness = (settings.zVidBrightness - 0.5f)*0.1f;
  p.contrast   = std::max(1.5f - settings.zVidContrast, 0.01f);
  p.gamma      = p.gamma/std::max(2.0f*settings.zVidGamma,  0.01f);

  static float mul = 0.f;
  if(mul>0)
    p.mul = mul;

  auto& pso = (settings.vidResIndex==0) ? shaders.tonemapping : shaders.tonemappingUpscale;
  cmd.setFramebuffer({ {result, Tempest::Discard, Tempest::Preserve} });
  cmd.setBinding(0, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, sceneLinear, Sampler::nearest(ClampMode::ClampToEdge)); // Lanczos upscale requires nearest sampling
  cmd.setPushData(p);
  cmd.setPipeline(pso);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawCMAA2(Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview) {
  auto&          pso             = shaders.cmaa2EdgeColor2x2Presets[Gothic::options().aaPreset];
  const IVec3    inputGroupSize  = pso.workGroupSize();
  const IVec3    outputGroupSize = inputGroupSize - IVec3(2, 2, 0);
  const uint32_t groupCountX     = uint32_t((sceneLinear.w() + outputGroupSize.x * 2 - 1) / (outputGroupSize.x * 2));
  const uint32_t groupCountY     = uint32_t((sceneLinear.h() + outputGroupSize.y * 2 - 1) / (outputGroupSize.y * 2));

  cmd.setFramebuffer({});
  cmd.setBinding(0, sceneLinear, Sampler::bilinear(ClampMode::ClampToEdge));
  cmd.setBinding(1, cmaa2.workingEdges);
  cmd.setBinding(2, cmaa2.shapeCandidates);
  cmd.setBinding(3, cmaa2.deferredBlendLocationList);
  cmd.setBinding(4, cmaa2.deferredBlendItemList);
  cmd.setBinding(5, cmaa2.deferredBlendItemListHeads);
  cmd.setBinding(6, cmaa2.controlBuffer);
  cmd.setBinding(7, cmaa2.indirectBuffer);

  // detect edges
  cmd.setPipeline(pso);
  cmd.dispatch(groupCountX, groupCountY, 1);

  // process candidates pass
  cmd.setPipeline(shaders.cmaa2ProcessCandidates);
  cmd.dispatchIndirect(cmaa2.indirectBuffer, 0);

  // deferred color apply
  struct Push {
    float brightness = 0;
    float contrast   = 1;
    float gamma      = 1.f/2.2f;
    float mul        = 1;
    };

  Push p;
  p.brightness = (settings.zVidBrightness - 0.5f)*0.1f;
  p.contrast   = std::max(1.5f - settings.zVidContrast, 0.01f);
  p.gamma      = p.gamma/std::max(2.0f*settings.zVidGamma,  0.01f);

  static float mul = 0.f;
  if(mul>0)
    p.mul = mul;

  auto& psoTone = (settings.vidResIndex==0) ? shaders.tonemapping : shaders.tonemappingUpscale;
  cmd.setFramebuffer({{result, Tempest::Discard, Tempest::Preserve}});
  cmd.setBinding(0, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, sceneLinear, Sampler::nearest());
  cmd.setPushData(&p, sizeof(p));
  cmd.setPipeline(psoTone);
  cmd.draw(Resources::fsqVbo());

  cmd.setBinding(0, sceneLinear);
  cmd.setBinding(1, cmaa2.workingEdges);
  cmd.setBinding(2, cmaa2.shapeCandidates);
  cmd.setBinding(3, cmaa2.deferredBlendLocationList);
  cmd.setBinding(4, cmaa2.deferredBlendItemList);
  cmd.setBinding(5, cmaa2.deferredBlendItemListHeads);
  cmd.setBinding(6, cmaa2.controlBuffer);
  cmd.setPushData(&p, sizeof(p));
  cmd.setPipeline(shaders.cmaa2DeferredColorApply2x2);
  cmd.drawIndirect(cmaa2.indirectBuffer, 3*sizeof(uint32_t));
  }

void Renderer::drawFog(Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview) {
  auto& scene = wview.sceneGlobals();

  switch(sky.quality) {
    case None:
    case VolumetricLQ: {
      cmd.setBinding(0, sky.fogLut3D, Sampler::bilinear(ClampMode::ClampToEdge));
      cmd.setBinding(1, sky.fogLut3D, Sampler::bilinear(ClampMode::ClampToEdge));
      cmd.setBinding(2, zbuffer, Sampler::nearest()); // NOTE: wanna here depthFetch from gles2
      cmd.setBinding(3, scene.uboGlobal[SceneGlobals::V_Main]);
      cmd.setPipeline(shaders.fog);
      break;
      }
    case VolumetricHQ: {
      cmd.setBinding(0, sky.fogLut3D,   Sampler::bilinear(ClampMode::ClampToEdge));
      cmd.setBinding(1, sky.fogLut3DMs, Sampler::bilinear(ClampMode::ClampToEdge));
      cmd.setBinding(2, zbuffer,        Sampler::nearest());
      cmd.setBinding(3, scene.uboGlobal[SceneGlobals::V_Main]);
      cmd.setBinding(4, sky.occlusionLut);
      cmd.setPipeline(shaders.fog3dHQ);
      break;
      }
    case Epipolar: {
      //cmd.setBinding(0, sky.fogLut3D,   Sampler::bilinear(ClampMode::ClampToEdge));
      cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
      cmd.setBinding(1, zbuffer,        Sampler::nearest());
      cmd.setBinding(2, vsm.fogDbg,     Sampler::bilinear(ClampMode::ClampToEdge));
      cmd.setBinding(3, vsm.epipoles);
      cmd.setBinding(4, sky.fogLut3DMs, Sampler::bilinear(ClampMode::ClampToEdge));
      cmd.setPipeline(shaders.vsmFog);
      break;
      }
    case PathTrace:
      return;
    }
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawSunMoon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview) {
  drawSunMoon(cmd, wview, false);
  drawSunMoon(cmd, wview, true);
  }

void Renderer::drawSunMoon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview, bool isSun) {
  auto& scene = wview.sceneGlobals();
  auto& sun   = wview.sky().sunLight();
  auto  m     = scene.viewProject();
  auto  d     = sun.dir();

  if(!isSun) {
    // fixed pos for now
    d = Vec3::normalize({-1,1,0});
    }

  auto  dx = d;
  float w  = 0;
  m.project(dx.x, dx.y, dx.z, w);

  if(dx.z<0)
    return;

  struct Push {
    Tempest::Vec2      pos;
    Tempest::Vec2      size;
    Tempest::Vec3      sunDir;
    float              GSunIntensity = 0;
    Tempest::Matrix4x4 viewProjectInv;
    uint32_t           isSun = 0;
    } push;
  push.pos     = Vec2(dx.x,dx.y)/dx.z;
  push.size.x  = 2.f/float(zbuffer.w());
  push.size.y  = 2.f/float(zbuffer.h());

  const float intencity      = 0.07f;
  const float GSunIntensity  = wview.sky().sunIntensity();
  const float GMoonIntensity = wview.sky().moonIntensity();

  const float scale          = internalResolutionScale();
  const float sunSize        = settings.sunSize  * scale;
  const float moonSize       = settings.moonSize * scale;

  push.size          *= isSun ? sunSize : (moonSize*0.25f);
  push.GSunIntensity  = isSun ? (GSunIntensity*intencity) : (GMoonIntensity*intencity);
  push.isSun          = isSun ? 1 : 0;
  push.sunDir         = d;
  push.viewProjectInv = scene.viewProjectLwcInv();

  // HACK
  if(isSun) {
    float day = sun.dir().y;
    float stp = linearstep(-0.07f, 0.03f, day);
    push.GSunIntensity *= stp*stp*4.f;
    } else {
    push.GSunIntensity *= wview.sky().isNight();
    }
  // push.GSunIntensity *= exposure;

  cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, isSun ? wview.sky().sunImage() : wview.sky().moonImage());
  cmd.setBinding(2, sky.transLut, Sampler::bilinear(ClampMode::ClampToEdge));
  cmd.setPushData(push);
  cmd.setPipeline(shaders.sun);
  cmd.draw(nullptr, 0, 6);
  }

void Renderer::stashSceneAux(Encoder<CommandBuffer>& cmd) {
  auto& device = Resources::device();
  if(!device.properties().hasSamplerFormat(zBufferFormat))
    return;
  cmd.setFramebuffer({{sceneOpaque, Tempest::Discard, Tempest::Preserve}, {sceneDepth, Tempest::Discard, Tempest::Preserve}});
  cmd.setDebugMarker("Stash scene");
  cmd.setBinding(0, sceneLinear,Sampler::nearest());
  cmd.setBinding(1, zbuffer,    Sampler::nearest());
  cmd.setPipeline(shaders.stash);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawVsmDbg(Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview) {
  static bool enable = false;
  if(!enable || !settings.vsmEnabled)
    return;

  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}});
  cmd.setDebugMarker("VSM-dbg");
  cmd.setBinding(0, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, gbufDiffuse, Sampler::nearest());
  cmd.setBinding(2, gbufNormal,  Sampler::nearest());
  cmd.setBinding(3, zbuffer,     Sampler::nearest());
  cmd.setBinding(4, vsm.pageTbl);
  cmd.setBinding(5, vsm.pageList);
  cmd.setBinding(6, vsm.pageData);
  cmd.setBinding(8, wview.sceneGlobals().vsmDbg);
  cmd.setPushData(&settings.vsmMipBias, sizeof(settings.vsmMipBias));
  cmd.setPipeline(shaders.vsmDbg);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawSwrDbg(Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview) {
  static bool enable = true;
  if(!enable || !settings.swrEnabled)
    return;

  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}});
  cmd.setDebugMarker("SWR-dbg");
  cmd.setBinding(0, swr.outputImage);
  cmd.setBinding(1, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(2, gbufDiffuse, Sampler::nearest());
  cmd.setBinding(3, gbufNormal,  Sampler::nearest());
  cmd.setBinding(4, zbuffer,     Sampler::nearest());
  cmd.setPipeline(shaders.swRenderingDbg);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawRtsmDbg(Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview) {
  static bool enable = false;
  if(!enable || !settings.rtsmEnabled)
    return;

  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}});
  cmd.setDebugMarker("RTSM-dbg");
#if 1
  cmd.setBinding(0, rtsm.dbg);
#else
  cmd.setBinding(0, rtsm.primBins);
  cmd.setBinding(1, rtsm.posList);
  cmd.setBinding(2, rtsm.pages);
#endif
  cmd.setPipeline(shaders.rtsmDbg);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::initGiData() {
  if(!settings.giEnabled)
    return;
  if(!gi.hashTable.isEmpty())
    return;

  auto& device = Resources::device();
  const uint32_t maxProbes = gi.maxProbes;

  gi.hashTable          = device.ssbo(nullptr, 2'097'152*sizeof(uint32_t)); // 8MB
  gi.voteTable          = device.ssbo(nullptr, gi.hashTable.byteSize());
  gi.probes             = device.ssbo(nullptr, maxProbes*32 + 64);        // probes and header
  gi.freeList           = device.ssbo(nullptr, maxProbes*sizeof(uint32_t) + sizeof(int32_t));
  gi.probesGBuffDiff    = device.image2d(TextureFormat::RGBA8, gi.atlasDim*16, gi.atlasDim*16); // 16x16 tile
  gi.probesGBuffNorm    = device.image2d(TextureFormat::RGBA8, gi.atlasDim*16, gi.atlasDim*16);
  gi.probesGBuffRayT    = device.image2d(TextureFormat::R16,   gi.atlasDim*16, gi.atlasDim*16);
  gi.probesLighting     = device.image2d(TextureFormat::R11G11B10UF, gi.atlasDim*3, gi.atlasDim*2);
  gi.probesLightingPrev = device.image2d(TextureFormat::R11G11B10UF, uint32_t(gi.probesLighting.w()), uint32_t(gi.probesLighting.h()));
  gi.fisrtFrame         = true;
  }

void Renderer::drawHiZ(Encoder<CommandBuffer>& cmd, WorldView& view) {
  cmd.setDebugMarker("HiZ-occluders");
  cmd.setFramebuffer({}, {zbuffer, 1.f, Tempest::Preserve});
  view.drawHiZ(cmd);
  }

void Renderer::buildHiZ(Tempest::Encoder<Tempest::CommandBuffer>& cmd) {
  assert(hiz.hiZ.w()<=128 && hiz.hiZ.h()<=128); // shader limitation

  cmd.setDebugMarker("HiZ-mip");
  cmd.setFramebuffer({});
  cmd.setBinding(0, zbuffer, Sampler::nearest(ClampMode::ClampToEdge));
  cmd.setBinding(1, hiz.hiZ);
  cmd.setPipeline(shaders.hiZPot);
  cmd.dispatch(size_t(hiz.hiZ.w()), size_t(hiz.hiZ.h()));

  const uint32_t maxBind = 8, mip = hiz.hiZ.mipCount();
  const uint32_t w = uint32_t(hiz.hiZ.w()), h = uint32_t(hiz.hiZ.h());
  if(hiz.atomicImg) {
    cmd.setBinding(0, hiz.counter, Sampler::nearest(), 0);
    } else {
    cmd.setBinding(0, hiz.counterBuf);
    }
  for(uint32_t i=0; i<maxBind; ++i)
    cmd.setBinding(1+i, hiz.hiZ, Sampler::nearest(), std::min(i, mip-1));
  cmd.setPushData(&mip, sizeof(mip));
  cmd.setPipeline(shaders.hiZMip);
  cmd.dispatchThreads(w,h);
  }

void Renderer::drawVsm(Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview) {
  if(!settings.vsmEnabled)
    return;

  static bool omniLights = true;
  const bool doVirtualFog = sky.quality!=VolumetricLQ && sky.quality!=PathTrace && settings.vsmEnabled;

  auto& device  = Resources::device();
  auto& scene   = wview.sceneGlobals();

  const size_t numOmniPages = wview.lights().size()*6;

  if(omniLights && vsm.pageTblOmni.isEmpty()) {
    Resources::recycle(std::move(vsm.pageTblOmni));
    Resources::recycle(std::move(vsm.visibleLights));
    vsm.pageTblOmni   = device.ssbo(nullptr, shaders.vsmClearOmni.sizeofBuffer(0, wview.lights().size()*6));
    vsm.visibleLights = device.ssbo(nullptr, shaders.vsmClearOmni.sizeofBuffer(1, wview.lights().size()));
    }

  cmd.setFramebuffer({});
  cmd.setDebugMarker("VSM-pages");
  cmd.setBinding(0, vsm.pageList);
  cmd.setBinding(1, vsm.pageTbl);
  cmd.setBinding(2, vsm.pageHiZ);
  cmd.setPipeline(shaders.vsmClear);
  cmd.dispatchThreads(size_t(vsm.pageTbl.w()), size_t(vsm.pageTbl.h()), size_t(vsm.pageTbl.d()));

  if(omniLights) {
    cmd.setBinding(0, vsm.pageTblOmni);
    cmd.setBinding(1, vsm.visibleLights);
    cmd.setPipeline(shaders.vsmClearOmni);
    cmd.dispatchThreads(numOmniPages);

    struct Push { float znear; uint32_t lightsTotal; } push = {};
    push.znear       = scene.znear;
    push.lightsTotal = uint32_t(wview.lights().size());
    cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
    cmd.setBinding(1, wview.lights().lightsSsbo());
    cmd.setBinding(2, vsm.visibleLights);
    cmd.setBinding(3, *scene.hiZ);
    cmd.setPushData(push);
    cmd.setPipeline(shaders.vsmCullLights);
    cmd.dispatchThreads(wview.lights().size());
    }

  cmd.setBinding(0, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, gbufDiffuse, Sampler::nearest());
  cmd.setBinding(2, gbufNormal,  Sampler::nearest());
  cmd.setBinding(3, zbuffer,     Sampler::nearest());
  cmd.setBinding(4, vsm.pageTbl);
  cmd.setBinding(5, vsm.pageHiZ);
  cmd.setPushData(settings.vsmMipBias);
  cmd.setPipeline(shaders.vsmMarkPages);
  cmd.dispatchThreads(zbuffer.size());

  if(omniLights) {
    struct Push { Vec3 originLwc; float znear; float vsmMipBias; } push = {};
    push.originLwc  = scene.originLwc;
    push.znear      = scene.znear;
    push.vsmMipBias = settings.vsmMipBias;
    cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
    cmd.setBinding(1, gbufDiffuse, Sampler::nearest());
    cmd.setBinding(2, gbufNormal,  Sampler::nearest());
    cmd.setBinding(3, zbuffer,     Sampler::nearest());
    cmd.setBinding(4, wview.lights().lightsSsbo());
    cmd.setBinding(5, vsm.visibleLights);
    cmd.setBinding(6, vsm.pageTblOmni);
    cmd.setBinding(7, vsm.vsmDbg);
    cmd.setPushData(&push, sizeof(push));
    cmd.setPipeline(shaders.vsmMarkOmniPages);
    cmd.dispatchThreads(zbuffer.size());

    const uint32_t lightsTotal = uint32_t(wview.lights().size());
    cmd.setBinding(0, vsm.pageTblOmni);
    cmd.setPushData(&lightsTotal, sizeof(lightsTotal));
    cmd.setPipeline(shaders.vsmPostprocessOmni);
    cmd.dispatchThreads(wview.lights().size());
    }

  // sky&fog
  if(doVirtualFog) {
    cmd.setDebugMarker("VSM-epipolar");
    cmd.setBinding(0, sky.occlusionLut);
    cmd.setBinding(1, vsm.epTrace);
    cmd.setBinding(2, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    cmd.setBinding(3, vsm.epipoles);
    cmd.setBinding(4, zbuffer);
    cmd.setBinding(5, vsm.pageTbl);
    cmd.setBinding(6, vsm.pageData);
    cmd.setPipeline(shaders.vsmFogEpipolar);
    cmd.dispatch(uint32_t(vsm.epTrace.h()));

    cmd.setBinding(0, vsm.epTrace);
    cmd.setBinding(1, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    cmd.setBinding(2, vsm.epipoles);
    cmd.setBinding(3, vsm.pageTbl);
    cmd.setBinding(4, vsm.pageHiZ);
    cmd.setPipeline(shaders.vsmFogPages);
    cmd.dispatchThreads(vsm.epTrace.size());
    }

  cmd.setDebugMarker("VSM-pages-alloc");
  if(true) {
    // trimming
    // cmd.setBinding(0, vsm.pageList);
    // cmd.setBinding(1, vsm.pageTbl);
    // cmd.setPipeline(shaders.vsmTrimPages);
    // cmd.dispatch(1);

    // clump
    cmd.setBinding(0, vsm.pageList);
    cmd.setBinding(1, vsm.pageTbl);
    cmd.setPipeline(shaders.vsmClumpPages);
    cmd.dispatchThreads(size_t(vsm.pageTbl.w()), size_t(vsm.pageTbl.h()), size_t(vsm.pageTbl.d()));
    }

  cmd.setBinding(0, vsm.pageList);
  cmd.setBinding(1, vsm.pageTbl);
  cmd.setBinding(2, vsm.pageTblOmni);
  cmd.setBinding(3, vsm.pageListTmp);
  cmd.setBinding(4, scene.vsmDbg);
  // list
  cmd.setPipeline(shaders.vsmListPages);
  if(omniLights)
    cmd.dispatch(size_t(vsm.pageTbl.d() + 1)); else
    cmd.dispatch(size_t(vsm.pageTbl.d()));

  // cmd.setPipeline(shaders.vsmSortPages);
  // cmd.dispatch(1);

  // alloc
  // cmd.setPipeline(shaders.vsmAlloc2Pages);
  // cmd.dispatchThreads(size_t(vsm.pageData.w()/VSM_PAGE_SIZE), size_t(vsm.pageData.h()/VSM_PAGE_SIZE));

  cmd.setPipeline(shaders.vsmAllocPages);
  cmd.dispatch(1);

  // hor-merge
  cmd.setPipeline(shaders.vsmMergePages);
  cmd.dispatch(1);

  cmd.setDebugMarker("VSM-visibility");
  wview.visibilityVsm(cmd);

  cmd.setDebugMarker("VSM-rendering");
  cmd.setFramebuffer({}, {vsm.pageData, 0.f, Tempest::Preserve});
  wview.drawVsm(cmd);
  }

void Renderer::drawRtsm(Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview) {
  if(!settings.rtsmEnabled)
    return;

  static bool omniLights = true;

  const int RTSM_BIN_SIZE   = 32;
  const int RTSM_SMALL_TILE = 32;
  const int RTSM_LARGE_TILE = 128;

  auto& device   = Resources::device();
  auto& shaders  = Shaders::inst();

  const auto& scene        = wview.sceneGlobals();
  const auto& clusters     = wview.clusters();
  const auto& drawCmd      = wview.drawCommands();
  const auto& buckets      = wview.drawBuckets();
  const auto& instanceSsbo = wview.instanceSsbo();

  auto& sceneUbo = scene.uboGlobal[SceneGlobals::V_Vsm];

  {
    // alloc resources
    if(rtsm.pages.isEmpty()) {
      rtsm.pages = device.image3d(TextureFormat::R32U, 32, 32, 16);
      }

    const auto tiles = tileCount(scene.zbuffer->size(), RTSM_SMALL_TILE);
    if(rtsm.tiles.size()!=tiles) {
      Resources::recycle(std::move(rtsm.tiles));
      rtsm.tiles    = device.image2d(TextureFormat::RG32U, tiles);
      rtsm.smpTiles = device.image2d(TextureFormat::R32U, tiles);
      }
    if(rtsm.complexTiles.byteSize()!=shaders.rtsmClear.sizeofBuffer(3, size_t(tiles.w*tiles.h))) {
      const size_t sz = shaders.rtsmClear.sizeofBuffer(3, size_t(tiles.w*tiles.h));
      Resources::recycle(std::move(rtsm.complexTiles));
      rtsm.complexTiles = device.ssbo(nullptr, sz);
      }

    if(rtsm.primBins.size()!=tileCount(scene.zbuffer->size(), RTSM_BIN_SIZE)) {
      auto sz = tileCount(scene.zbuffer->size(), RTSM_BIN_SIZE);
      Resources::recycle(std::move(rtsm.primBins));
      // NOTE: need to improve engine api to accept size
      rtsm.primBins = device.image2d(TextureFormat::RG32U, uint32_t(sz.w), uint32_t(sz.h));
      }
    if(rtsm.primBins8.size()!=tileCount(scene.zbuffer->size(), 8)) {
      auto sz = tileCount(scene.zbuffer->size(), 8);
      Resources::recycle(std::move(rtsm.primBins8));
      // NOTE: need to improve engine api to accept size
      rtsm.primBins8 = device.image2d(TextureFormat::RG32U, uint32_t(sz.w), uint32_t(sz.h));
      }

    if(rtsm.dbg.size()!=tileCount(scene.zbuffer->size(), 32)) {
      auto sz = tileCount(scene.zbuffer->size(), 32);
      Resources::recycle(std::move(rtsm.dbg));
      rtsm.dbg = device.image2d(TextureFormat::R32U, sz);
      }

    if(rtsm.dbg16.size()!=tileCount(scene.zbuffer->size(), 16)) {
      auto sz = tileCount(scene.zbuffer->size(), 16);
      Resources::recycle(std::move(rtsm.dbg16));
      rtsm.dbg16 = device.image2d(TextureFormat::R32U, sz);
      }

    if(rtsm.dbg8.size()!=tileCount(scene.zbuffer->size(), 8)) {
      auto sz = tileCount(scene.zbuffer->size(), 8);
      Resources::recycle(std::move(rtsm.dbg8));
      rtsm.dbg8 = device.image2d(TextureFormat::R32U, sz);
      }

    const size_t clusterCnt = drawCmd.maxMeshlets();
    const size_t clusterSz  = shaders.rtsmClear.sizeofBuffer(1, clusterCnt);
    if(rtsm.visList.byteSize()!=clusterSz) {
      Resources::recycle(std::move(rtsm.visList));
      rtsm.visList = device.ssbo(nullptr, clusterSz);
      }

    if(rtsm.posList.isEmpty()) {
      rtsm.posList = device.ssbo(Tempest::Uninitialized, 64*1024*1024); // arbitrary
      }
  }

  if(omniLights) {
    // alloc resources, for omni-lights
    if(rtsm.visibleLights.byteSize()!=shaders.vsmClearOmni.sizeofBuffer(1, wview.lights().size())) {
      Resources::recycle(std::move(rtsm.visibleLights));
      rtsm.visibleLights = device.ssbo(nullptr, shaders.vsmClearOmni.sizeofBuffer(1, wview.lights().size()));
      }
    }

  cmd.setDebugMarker("RTSM-rendering");
  cmd.setFramebuffer({});

  {
    // clear
    struct Push { uint32_t omni; } push = {omniLights ? 1u : 0u};
    cmd.setBinding(0, rtsm.pages);
    cmd.setBinding(1, rtsm.visList);
    cmd.setBinding(2, rtsm.posList);
    cmd.setBinding(3, rtsm.complexTiles);
    cmd.setBinding(4, rtsm.visibleLights);
    cmd.setPushData(push);

    cmd.setPipeline(shaders.rtsmClear);
    cmd.dispatchThreads(size_t(rtsm.pages.w()), size_t(rtsm.pages.h()), size_t(rtsm.pages.d()));
  }

  if(omniLights) {
    struct Push { float znear; uint32_t lightsTotal; } push = {};
    push.znear       = scene.znear;
    push.lightsTotal = uint32_t(wview.lights().size());
    cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
    cmd.setBinding(1, wview.lights().lightsSsbo());
    cmd.setBinding(2, rtsm.visibleLights);
    cmd.setBinding(3, hiz.hiZ);
    cmd.setPushData(push);
    cmd.setPipeline(shaders.rtsmCullLights);
    cmd.dispatchThreads(wview.lights().size());
    }

  {
    // global cull
    struct Push { uint32_t meshletCount; } push = {};
    push.meshletCount = uint32_t(clusters.size());
    cmd.setPushData(push);

    cmd.setBinding(0, rtsm.outputImage);
    cmd.setBinding(1, sceneUbo);
    cmd.setBinding(2, gbufDiffuse);
    cmd.setBinding(3, gbufNormal);
    cmd.setBinding(4, zbuffer);
    cmd.setBinding(5, clusters.ssbo());
    cmd.setBinding(6, rtsm.visList);
    cmd.setBinding(7, rtsm.pages);

    cmd.setPipeline(shaders.rtsmPages);
    cmd.dispatchThreads(scene.zbuffer->size());

    cmd.setBinding(0, rtsm.pages);
    cmd.setPipeline(shaders.rtsmHiZ);
    cmd.dispatch(1);

    cmd.setBinding(7, rtsm.posList);
    cmd.setPipeline(shaders.rtsmCulling);
    cmd.dispatchThreads(push.meshletCount);
  }

  {
    // position
    cmd.setBinding(0, rtsm.posList);
    cmd.setBinding(1, sceneUbo);
    cmd.setBinding(2, rtsm.visList);

    cmd.setBinding(5,  clusters.ssbo());
    cmd.setBinding(6,  instanceSsbo);
    cmd.setBinding(7,  buckets.ssbo());
    cmd.setBinding(8,  buckets.ibo());
    cmd.setBinding(9,  buckets.vbo());
    cmd.setBinding(10, buckets.morphId());
    cmd.setBinding(11, buckets.morph());

    cmd.setPipeline(shaders.rtsmPosition);
    cmd.dispatchIndirect(rtsm.visList,0);
  }

  {
    // tile hirarchy
    const auto largeTiles = tileCount(scene.zbuffer->size(), RTSM_LARGE_TILE);
    const auto smallTiles = tileCount(scene.zbuffer->size(), RTSM_SMALL_TILE);

    cmd.setBinding(0, rtsm.outputImage);
    cmd.setBinding(1, sceneUbo);
    cmd.setBinding(2, zbuffer);
    cmd.setBinding(3, rtsm.posList);
    cmd.setBinding(4, rtsm.complexTiles);
    cmd.setBinding(5, rtsm.tiles);
    cmd.setBinding(6, rtsm.smpTiles);
    cmd.setBinding(9, rtsm.dbg);

    cmd.setPipeline(shaders.rtsmMeshletCull);
    cmd.dispatch(largeTiles);

    cmd.setPipeline(shaders.rtsmMeshletComplex);
    cmd.dispatchIndirect(rtsm.complexTiles, 0);

    cmd.setPipeline(shaders.rtsmSampleCull);
    cmd.dispatch(smallTiles);
  }

  {
    // in-tile
    cmd.setBinding(0, rtsm.outputImage);
    cmd.setBinding(1, sceneUbo);
    cmd.setBinding(2, gbufNormal);
    cmd.setBinding(3, zbuffer);
    cmd.setBinding(4, rtsm.posList);
    cmd.setBinding(5, rtsm.tiles);
    cmd.setBinding(6, rtsm.primBins);
    cmd.setBinding(7, buckets.textures());
    cmd.setBinding(8, Sampler::trillinear());
    cmd.setBinding(9, rtsm.dbg);

    // primitives
    cmd.setPipeline(shaders.rtsmPrimCull);
    cmd.dispatch(rtsm.primBins.size());

    // raster
    cmd.setBinding(9, rtsm.dbg16);
    cmd.setPipeline(shaders.rtsmRaster);
    cmd.dispatchThreads(rtsm.outputImage.size());
  }

  if(false) {
    struct Push { uint32_t meshletCount; } push = {};
    push.meshletCount = uint32_t(clusters.size());

    cmd.setFramebuffer({});
    cmd.setBinding(0, *scene.vsmPageData);
    cmd.setBinding(1, scene.uboGlobal[SceneGlobals::V_Vsm]);
    cmd.setBinding(2, *scene.vsmPageList);
    cmd.setBinding(3, clusters.ssbo());
    cmd.setBinding(4, instanceSsbo);
    cmd.setBinding(5, buckets.ibo());
    cmd.setBinding(6, buckets.vbo());
    cmd.setBinding(7, buckets.textures());
    cmd.setBinding(8, Sampler::bilinear());
    cmd.setPushData(&push, sizeof(push));
    cmd.setPipeline(Shaders::inst().vsmRendering);
    // const auto sz = Shaders::inst().vsmRendering.workGroupSize();
    cmd.dispatch(1024u);
    }
  }

void Renderer::drawSwr(Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview) {
  if(!settings.swrEnabled)
    return;

  const auto& scene        = wview.sceneGlobals();
  const auto& clusters     = wview.clusters();
  const auto& buckets      = wview.drawBuckets();
  const auto& instanceSsbo = wview.instanceSsbo();

  cmd.setFramebuffer({});
  cmd.setDebugMarker("SW-rendering");

  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; float znear; } push = {};
  push.firstMeshlet = 0;
  push.meshletCount = uint32_t(clusters.size());
  push.znear        = scene.znear;

  cmd.setBinding(0, swr.outputImage);
  cmd.setBinding(1, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(2, *scene.gbufNormals);
  cmd.setBinding(3, *scene.zbuffer);
  cmd.setBinding(4, clusters.ssbo());
  cmd.setBinding(5, buckets.ibo());
  cmd.setBinding(6, buckets.vbo());
  cmd.setBinding(7, buckets.textures());
  cmd.setBinding(8, Sampler::bilinear());

  auto* pso = &Shaders::inst().swRendering;
  switch(Gothic::options().swRenderingPreset) {
    case 1: {
      cmd.setPushData(&push, sizeof(push));
      cmd.setPipeline(*pso);
      //cmd.dispatch(10);
      cmd.dispatch(clusters.size());
      break;
      }
    case 2: {
      IVec2  tileSize = IVec2(128);
      int    tileX    = (swr.outputImage.w()+tileSize.x-1)/tileSize.x;
      int    tileY    = (swr.outputImage.h()+tileSize.y-1)/tileSize.y;
      cmd.setPushData(&push, sizeof(push));
      cmd.setPipeline(*pso);
      cmd.dispatch(size_t(tileX), size_t(tileY)); //swr.outputImage.size());
      break;
      }
    case 3: {
      cmd.setBinding(9,  *scene.lights);
      cmd.setBinding(10, instanceSsbo);
      cmd.setPushData(&push, sizeof(push));
      cmd.setPipeline(*pso);
      cmd.dispatchThreads(swr.outputImage.size());
      break;
      }
    }
  }

void Renderer::drawGBuffer(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& view) {
  cmd.setDebugMarker("GBuffer");
  cmd.setFramebuffer({{gbufDiffuse, Tempest::Vec4(), Tempest::Preserve},
                      {gbufNormal,  Tempest::Vec4(), Tempest::Preserve}},
                     {zbuffer, Tempest::Preserve, Tempest::Preserve});
  view.drawGBuffer(cmd,fId);
  }

void Renderer::drawGWater(Encoder<CommandBuffer>& cmd, WorldView& view) {
  static bool water = true;
  if(!water)
    return;

  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve},
                      {gbufDiffuse, Vec4(0,0,0,0),     Tempest::Preserve},
                      {gbufNormal,  Vec4(0,0,0,0),     Tempest::Preserve}},
                     {zbuffer, Tempest::Preserve, Tempest::Preserve});
  // cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}},
  //                    {zbuffer, Tempest::Preserve, Tempest::Preserve});
  cmd.setDebugMarker("GWater");
  view.drawWater(cmd);
  }

void Renderer::drawReflections(Encoder<CommandBuffer>& cmd, const WorldView& wview) {
  auto& pso = settings.zEnvMappingEnabled ? shaders.waterReflectionSSR : shaders.waterReflection;

  cmd.setDebugMarker("Reflections");
  cmd.setBinding(0, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, sceneOpaque, Sampler::bilinear(ClampMode::ClampToEdge));
  cmd.setBinding(2, gbufDiffuse, Sampler::nearest (ClampMode::ClampToEdge));
  cmd.setBinding(3, gbufNormal,  Sampler::nearest (ClampMode::ClampToEdge));
  cmd.setBinding(4, zbuffer,     Sampler::nearest (ClampMode::ClampToEdge));
  cmd.setBinding(5, sceneDepth,  Sampler::nearest (ClampMode::ClampToEdge));
  cmd.setBinding(6, sky.viewCldLut);
  cmd.setPipeline(pso);
  if(Gothic::options().doMeshShading) {
    cmd.dispatchMeshThreads(gbufDiffuse.size());
    } else {
    cmd.draw(Resources::fsqVbo());
    }
  }

void Renderer::drawUnderwater(Encoder<CommandBuffer>& cmd, const WorldView& wview) {
  cmd.setBinding(0, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, zbuffer);

  cmd.setPipeline(shaders.underwaterT);
  cmd.draw(Resources::fsqVbo());
  cmd.setPipeline(shaders.underwaterS);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawShadowMap(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& view) {
  for(uint8_t i=0; i<Resources::ShadowLayers; ++i) {
    if(shadowMap[i].isEmpty())
      continue;
    cmd.setDebugMarker(string_frm("ShadowMap #",i));
    cmd.setFramebuffer({}, {shadowMap[i], 0.f, Tempest::Preserve});
    if(view.mainLight().dir().y > Camera::minShadowY)
      view.drawShadow(cmd,fId,i);
    }
  }

void Renderer::drawShadowResolve(Encoder<CommandBuffer>& cmd, const WorldView& wview) {
  static bool light = true;
  if(!light)
    return;

  auto& scene = wview.sceneGlobals();
  cmd.setDebugMarker(settings.vsmEnabled ? "DirectSunLight-VSM" : "DirectSunLight");

  auto originLwc = scene.originLwc;
  cmd.setPushData(&originLwc, sizeof(originLwc));
  cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, gbufDiffuse, Sampler::nearest());
  cmd.setBinding(2, gbufNormal,  Sampler::nearest());
  cmd.setBinding(3, zbuffer,     Sampler::nearest());
  if(shadow.directLightPso==&shaders.vsmDirectLight) {
    cmd.setBinding(4, vsm.pageTbl);
    cmd.setBinding(5, vsm.pageList);
    cmd.setBinding(6, vsm.pageData);
    cmd.setBinding(8, scene.vsmDbg);
    }
  else if(shadow.directLightPso==&shaders.directLightRq) {
    for(size_t r=0; r<Resources::ShadowLayers; ++r) {
      if(shadowMap[r].isEmpty())
        continue;
      cmd.setBinding(4+r, shadowMap[r], Resources::shadowSampler());
      }
    cmd.setBinding(6, scene.rtScene.tlas);
    cmd.setBinding(7, Sampler::bilinear());
    cmd.setBinding(8, scene.rtScene.tex);
    cmd.setBinding(9, scene.rtScene.vbo);
    cmd.setBinding(10,scene.rtScene.ibo);
    cmd.setBinding(11,scene.rtScene.rtDesc);
    }
  else if(shadow.directLightPso==&shaders.rtsmDirectLight) {
    cmd.setBinding(4, rtsm.outputImage);
    }
  else {
    for(size_t r=0; r<Resources::ShadowLayers; ++r) {
      if(shadowMap[r].isEmpty())
        continue;
      cmd.setBinding(4+r, shadowMap[r], Resources::shadowSampler());
      }
    }

  cmd.setPipeline(*shadow.directLightPso);
  if(settings.vsmEnabled) {
    cmd.setPushData(settings.vsmMipBias);
    cmd.setPipeline(*shadow.directLightPso);
    }
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawLights(Encoder<CommandBuffer>& cmd, const WorldView& wview) {
  static bool light = true;
  if(!light)
    return;

  auto& scene   = wview.sceneGlobals();
  cmd.setDebugMarker("Point lights");

  cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, gbufDiffuse, Sampler::nearest());
  cmd.setBinding(2, gbufNormal,  Sampler::nearest());
  cmd.setBinding(3, zbuffer,     Sampler::nearest());
  cmd.setBinding(4, wview.lights().lightsSsbo());
  if(lights.directLightPso==&shaders.lightsVsm) {
    cmd.setBinding(5, vsm.pageTblOmni);
    cmd.setBinding(6, vsm.pageData);
    }
  if(lights.directLightPso==&shaders.lightsRq) {
    cmd.setBinding(6, scene.rtScene.tlas);
    cmd.setBinding(7, Sampler::bilinear());
    cmd.setBinding(8, scene.rtScene.tex);
    cmd.setBinding(9, scene.rtScene.vbo);
    cmd.setBinding(10,scene.rtScene.ibo);
    cmd.setBinding(11,scene.rtScene.rtDesc);
    }

  auto  originLwc = scene.originLwc;
  auto& ibo       = Resources::cubeIbo();
  cmd.setPushData(&originLwc, sizeof(originLwc));
  cmd.setPipeline(*lights.directLightPso);
  cmd.draw(nullptr,ibo, 0,ibo.size(), 0,wview.lights().size());
  }

void Renderer::drawSky(Encoder<CommandBuffer>& cmd, const WorldView& wview) {
  auto& scene = wview.sceneGlobals();

  cmd.setDebugMarker("Sky");
  if(sky.quality==PathTrace) {
    cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
    cmd.setBinding(1, sky.transLut,     Sampler::bilinear(ClampMode::ClampToEdge));
    cmd.setBinding(2, sky.multiScatLut, Sampler::bilinear(ClampMode::ClampToEdge));
    cmd.setBinding(3, sky.cloudsLut,    Sampler::bilinear(ClampMode::ClampToEdge));
    cmd.setBinding(4, zbuffer, Sampler::nearest());
    cmd.setBinding(5, shadowMap[1], Resources::shadowSampler());
    cmd.setPipeline(shaders.skyPathTrace);
    cmd.draw(Resources::fsqVbo());
    return;
    }

  auto& skyShader = sky.quality==VolumetricLQ ? shaders.sky : shaders.skySep;
  cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, sky.transLut,     Sampler::bilinear(ClampMode::ClampToEdge));
  cmd.setBinding(2, sky.multiScatLut, Sampler::bilinear(ClampMode::ClampToEdge));
  cmd.setBinding(3, sky.viewLut,      Sampler::bilinear(ClampMode::ClampToEdge));
  cmd.setBinding(4, sky.fogLut3D);
  if(sky.quality!=VolumetricLQ)
    cmd.setBinding(5, sky.fogLut3DMs);
  cmd.setBinding(6, *wview.sky().cloudsDay()  .lay[0], Sampler::trillinear());
  cmd.setBinding(7, *wview.sky().cloudsDay()  .lay[1], Sampler::trillinear());
  cmd.setBinding(8, *wview.sky().cloudsNight().lay[0], Sampler::trillinear());
  cmd.setBinding(9, *wview.sky().cloudsNight().lay[1], Sampler::trillinear());
  cmd.setPipeline(skyShader);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::prepareSSAO(Encoder<CommandBuffer>& cmd, WorldView& wview) {
  if(!settings.zCloudShadowScale)
    return;
  // ssao
  struct PushSsao {
    Matrix4x4 proj;
    Matrix4x4 projInv;
    } push;
  push.proj    = proj;
  push.projInv = proj;
  push.projInv.inverse();

  cmd.setFramebuffer({});
  cmd.setDebugMarker("SSAO");

  cmd.setBinding(0, ssao.ssaoBuf);
  cmd.setBinding(1, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(2, gbufDiffuse, Sampler::nearest(ClampMode::ClampToEdge));
  cmd.setBinding(3, gbufNormal,  Sampler::nearest(ClampMode::ClampToEdge));
  cmd.setBinding(4, zbuffer,     Sampler::nearest(ClampMode::ClampToEdge));
  cmd.setPushData(&push, sizeof(push));
  cmd.setPipeline(shaders.ssao);
  cmd.dispatchThreads(ssao.ssaoBuf.size());

  cmd.setBinding(0, ssao.ssaoBlur);
  cmd.setBinding(1, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(2, ssao.ssaoBuf);
  cmd.setBinding(3, zbuffer, Sampler::nearest(ClampMode::ClampToEdge));
  cmd.setPipeline(shaders.ssaoBlur);
  cmd.dispatchThreads(ssao.ssaoBuf.size());
  }

void Renderer::prepareFog(Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview) {
  auto& scene = wview.sceneGlobals();

  cmd.setDebugMarker("Fog LUTs");
  if(sky.quality!=PathTrace) {
    auto& shader = sky.quality==VolumetricLQ ? shaders.fogViewLut3d : shaders.fogViewLutSep;
    cmd.setFramebuffer({});
    cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
    cmd.setBinding(1, sky.transLut,     Sampler::bilinear(ClampMode::ClampToEdge));
    cmd.setBinding(2, sky.multiScatLut, Sampler::bilinear(ClampMode::ClampToEdge));
    cmd.setBinding(3, sky.cloudsLut,    Sampler::bilinear(ClampMode::ClampToEdge));
    cmd.setBinding(4, sky.fogLut3D);
    if(sky.quality==VolumetricHQ || sky.quality==Epipolar)
      cmd.setBinding(5, sky.fogLut3DMs);
    cmd.setPipeline(shader);
    cmd.dispatchThreads(uint32_t(sky.fogLut3D.w()), uint32_t(sky.fogLut3D.h()));
    }

  if(settings.vsmEnabled && (sky.quality==VolumetricHQ || sky.quality==Epipolar)) {
    cmd.setFramebuffer({});
    cmd.setDebugMarker("VSM-epipolar-fog");
    cmd.setBinding(0, vsm.epTrace);
    cmd.setBinding(1, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    cmd.setBinding(2, vsm.epipoles);
    cmd.setBinding(3, vsm.pageTbl);
    cmd.setBinding(4, vsm.pageData);
    cmd.setPipeline(shaders.vsmFogShadow);
    cmd.dispatchThreads(vsm.epTrace.size());
    }

  switch(sky.quality) {
    case None:
    case VolumetricLQ:
      break;
    case VolumetricHQ: {
      if(settings.vsmEnabled) {
        cmd.setFramebuffer({});
        cmd.setBinding(0, sky.occlusionLut);
        cmd.setBinding(1, vsm.epTrace);
        cmd.setBinding(2, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
        cmd.setBinding(3, vsm.epipoles);
        cmd.setBinding(4, zbuffer);
        cmd.setPipeline(shaders.vsmFogSample);
        cmd.dispatchThreads(zbuffer.size());
        } else {
        cmd.setFramebuffer({});
        cmd.setBinding(2, zbuffer, Sampler::nearest());
        cmd.setBinding(3, scene.uboGlobal[SceneGlobals::V_Main]);
        cmd.setBinding(4, sky.occlusionLut);
        cmd.setBinding(5, shadowMap[1], Resources::shadowSampler());
        cmd.setPipeline(shaders.fogOcclusion);
        cmd.dispatchThreads(sky.occlusionLut.size());
        }
      break;
      }
    case Epipolar:{
      // experimental
      cmd.setFramebuffer({});
      cmd.setDebugMarker("VSM-trace");
      cmd.setBinding(0, vsm.fogDbg);
      cmd.setBinding(1, vsm.epTrace);
      cmd.setBinding(2, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
      cmd.setBinding(3, vsm.epipoles);
      cmd.setBinding(4, zbuffer);
      cmd.setBinding(5, sky.transLut,   Sampler::bilinear(ClampMode::ClampToEdge));
      cmd.setBinding(6, sky.cloudsLut,  Sampler::bilinear(ClampMode::ClampToEdge));
      cmd.setBinding(7, sky.fogLut3DMs, Sampler::bilinear(ClampMode::ClampToEdge));
      cmd.setPipeline(shaders.vsmFogTrace);
      cmd.dispatchThreads(vsm.epTrace.size());
      break;
      }
    case PathTrace: {
      break;
      }
    }
  }

void Renderer::prepareIrradiance(Encoder<CommandBuffer>& cmd, WorldView& wview) {
  auto& scene = wview.sceneGlobals();

  cmd.setDebugMarker("Irradiance");
  cmd.setFramebuffer({});
  cmd.setBinding(0, sky.irradianceLut);
  cmd.setBinding(1, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(2, sky.viewCldLut);
  cmd.setPipeline(shaders.irradiance);
  cmd.dispatch(1);
  }

void Renderer::prepareGi(Encoder<CommandBuffer>& cmd, WorldView& wview) {
  if(!settings.giEnabled || !settings.zCloudShadowScale) {
    return;
    }

  const size_t maxHash = gi.hashTable.byteSize()/sizeof(uint32_t);

  auto& scene = wview.sceneGlobals();

  cmd.setFramebuffer({});
  if(gi.fisrtFrame) {
    cmd.setDebugMarker("GI-Init");
    cmd.setBinding(0, gi.voteTable);
    cmd.setBinding(1, gi.hashTable);
    cmd.setBinding(2, gi.probes);
    cmd.setBinding(3, gi.freeList);
    cmd.setPipeline(shaders.probeInit);
    cmd.dispatch(1);
    cmd.setPipeline(shaders.probeClearHash);
    cmd.dispatchThreads(maxHash);

    cmd.setBinding(0, gi.probesLighting);
    cmd.setBinding(1, Resources::fallbackBlack());
    cmd.setPipeline(shaders.copyImg);
    cmd.dispatchThreads(gi.probesLighting.size());
    gi.fisrtFrame = false;
    }

  static bool alloc = true;
  cmd.setDebugMarker("GI-Alloc");
  cmd.setBinding(0, gi.voteTable);
  cmd.setBinding(1, gi.hashTable);
  cmd.setBinding(2, gi.probes);
  cmd.setBinding(3, gi.freeList);
  cmd.setPipeline(shaders.probeClear);
  cmd.dispatchThreads(maxHash);

  if(alloc) {
    cmd.setBinding(0, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    cmd.setBinding(1, gbufDiffuse, Sampler::nearest());
    cmd.setBinding(2, gbufNormal,  Sampler::nearest());
    cmd.setBinding(3, zbuffer,     Sampler::nearest());
    cmd.setBinding(4, gi.voteTable);
    cmd.setBinding(5, gi.hashTable);
    cmd.setBinding(6, gi.probes);
    cmd.setBinding(7, gi.freeList);
    cmd.setPipeline(shaders.probeVote);
    cmd.dispatchThreads(sceneDepth.size());

    cmd.setBinding(0, gi.voteTable);
    cmd.setBinding(1, gi.hashTable);
    cmd.setBinding(2, gi.probes);
    cmd.setBinding(3, gi.freeList);
    cmd.setPipeline(shaders.probePrune);
    cmd.dispatchThreads(gi.maxProbes);

    cmd.setBinding(0, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    cmd.setBinding(1, gbufDiffuse, Sampler::nearest());
    cmd.setBinding(2, gbufNormal,  Sampler::nearest());
    cmd.setBinding(3, zbuffer,     Sampler::nearest());
    cmd.setBinding(4, gi.voteTable);
    cmd.setBinding(5, gi.hashTable);
    cmd.setBinding(6, gi.probes);
    cmd.setBinding(7, gi.freeList);
    cmd.setPipeline(shaders.probeAlocation);
    cmd.dispatchThreads(sceneDepth.size());
    }

  cmd.setDebugMarker("GI-Trace");
  cmd.setBinding(0, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, gi.probesGBuffDiff);
  cmd.setBinding(2, gi.probesGBuffNorm);
  cmd.setBinding(3, gi.probesGBuffRayT);
  cmd.setBinding(4, gi.hashTable);
  cmd.setBinding(5, gi.probes);
  cmd.setBinding(6, scene.rtScene.tlas);
  cmd.setBinding(7, Sampler::bilinear());
  cmd.setBinding(8, scene.rtScene.tex);
  cmd.setBinding(9, scene.rtScene.vbo);
  cmd.setBinding(10,scene.rtScene.ibo);
  cmd.setBinding(11,scene.rtScene.rtDesc);
  cmd.setPipeline(shaders.probeTrace);
  cmd.dispatch(1024); // TODO: dispath indirect?

  cmd.setDebugMarker("GI-HashMap");
  cmd.setBinding(0, gi.voteTable);
  cmd.setBinding(1, gi.hashTable);
  cmd.setBinding(2, gi.probes);
  cmd.setBinding(3, gi.freeList);
  cmd.setPipeline(shaders.probeClearHash);
  cmd.dispatchThreads(maxHash);
  cmd.setPipeline(shaders.probeMakeHash);
  cmd.dispatchThreads(gi.maxProbes);

  cmd.setDebugMarker("GI-Lighting");
  cmd.setBinding(0, gi.probesLightingPrev);
  cmd.setBinding(1, gi.probesLighting);
  cmd.setPipeline(shaders.copyImg);
  cmd.dispatchThreads(gi.probesLighting.size());

  cmd.setBinding(0, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, gi.probesLighting);
  cmd.setBinding(2, gi.probesGBuffDiff, Sampler::nearest());
  cmd.setBinding(3, gi.probesGBuffNorm, Sampler::nearest());
  cmd.setBinding(4, gi.probesGBuffRayT, Sampler::nearest());
  cmd.setBinding(5, sky.viewCldLut, Sampler::bilinear());
  cmd.setBinding(6, shadowMap[1], Sampler::bilinear());
  cmd.setBinding(7, gi.probesLightingPrev, Sampler::nearest());
  cmd.setBinding(8, gi.hashTable);
  cmd.setBinding(9, gi.probes);
  cmd.setPipeline(shaders.probeLighting);
  cmd.dispatch(1024);
  }

void Renderer::prepareExposure(Encoder<CommandBuffer>& cmd, WorldView& wview) {
  auto& scene = wview.sceneGlobals();

  auto sunDir = wview.sky().sunLight().dir();
  struct Push {
    float baseL        = 0.0;
    float sunOcclusion = 1.0;
    };
  Push push;
  push.sunOcclusion = smoothstep(0.0f, 0.01f, sunDir.y);

  static float override = 0;
  static float add      = 1.f;
  if(override>0)
    push.baseL = override;
  push.baseL += add;

  cmd.setDebugMarker("Exposure");
  cmd.setFramebuffer({});
  cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, sky.viewCldLut);
  cmd.setBinding(2, sky.transLut,  Sampler::bilinear(ClampMode::ClampToEdge));
  cmd.setBinding(3, sky.cloudsLut, Sampler::bilinear(ClampMode::ClampToEdge));
  cmd.setBinding(4, sky.irradianceLut);
  cmd.setPushData(&push, sizeof(push));
  cmd.setPipeline(shaders.skyExposure);
  cmd.dispatch(1);
  }

void Renderer::drawProbesDbg(Encoder<CommandBuffer>& cmd, const WorldView& wview) {
  if(!settings.giEnabled)
    return;

  static bool enable = false;
  if(!enable)
    return;

  cmd.setDebugMarker("GI-dbg");
  cmd.setBinding(0, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, gi.probesLighting);
  cmd.setBinding(2, gi.probes);
  cmd.setBinding(3, gi.hashTable);
  cmd.setPipeline(shaders.probeDbg);
  cmd.draw(nullptr, 0, 36, 0, gi.maxProbes);
  }

void Renderer::drawProbesHitDbg(Encoder<CommandBuffer>& cmd) {
  if(!settings.giEnabled)
    return;

  static bool enable = false;
  if(!enable)
    return;

  cmd.setDebugMarker("GI-dbg");
  cmd.setBinding(1, gi.probesLighting);
  cmd.setBinding(2, gi.probesGBuffDiff);
  cmd.setBinding(3, gi.probesGBuffRayT);
  cmd.setBinding(4, gi.probes);
  cmd.setPipeline(shaders.probeHitDbg);
  cmd.draw(nullptr, 0, 36, 0, gi.maxProbes*256);
  //cmd.draw(nullptr, 0, 36, 0, 1024);
  }

void Renderer::drawAmbient(Encoder<CommandBuffer>& cmd, const WorldView& view) {
  static bool enable = true;
  if(!enable)
    return;

  if(settings.giEnabled && settings.zCloudShadowScale) {
    cmd.setDebugMarker("AmbientLight");
    cmd.setBinding(0, view.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    cmd.setBinding(1, gi.probesLighting);
    cmd.setBinding(2, gbufDiffuse,   Sampler::nearest());
    cmd.setBinding(3, gbufNormal,    Sampler::nearest());
    cmd.setBinding(4, zbuffer,       Sampler::nearest());
    cmd.setBinding(5, ssao.ssaoBlur, Sampler::nearest());
    cmd.setBinding(6, gi.hashTable);
    cmd.setBinding(7, gi.probes);
    cmd.setPipeline(shaders.probeAmbient);
    cmd.draw(Resources::fsqVbo());
    return;
    }

  cmd.setDebugMarker("AmbientLight");
  cmd.setBinding(0, view.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, gbufDiffuse, Sampler::nearest());
  cmd.setBinding(2, gbufNormal,  Sampler::nearest());
  cmd.setBinding(3, sky.irradianceLut);
  if(settings.zCloudShadowScale) {
    cmd.setBinding(4, ssao.ssaoBlur, Sampler::nearest(ClampMode::ClampToEdge));
    cmd.setPipeline(shaders.ambientLightSsao);
    } else {
    cmd.setPipeline(shaders.ambientLight);
    }
  cmd.draw(Resources::fsqVbo());
  }

Tempest::Attachment Renderer::screenshoot(uint8_t frameId) {
  auto& device = Resources::device();
  device.waitIdle();

  uint32_t w    = uint32_t(zbuffer.w());
  uint32_t h    = uint32_t(zbuffer.h());
  auto     img  = device.attachment(Tempest::TextureFormat::RGBA8,w,h);

  CommandBuffer cmd;
  {
  auto enc = cmd.startEncoding(device);
  draw(img,enc,frameId);
  }

  Fence sync = device.fence();
  device.submit(cmd,sync);
  sync.wait();
  return img;

  // debug
  auto d16     = device.attachment(TextureFormat::R16,    swapchain.w(),swapchain.h());
  auto normals = device.attachment(TextureFormat::RGBA16, swapchain.w(),swapchain.h());

  {
  auto enc = cmd.startEncoding(device);
  enc.setFramebuffer({{normals,Tempest::Discard,Tempest::Preserve}});
  enc.setBinding(0, gbufNormal, Sampler::nearest());
  enc.setPipeline(shaders.copy);
  enc.draw(Resources::fsqVbo());
  }
  device.submit(cmd,sync);
  sync.wait();

  {
  auto enc = cmd.startEncoding(device);
  enc.setFramebuffer({{d16,Tempest::Discard,Tempest::Preserve}});
  enc.setBinding(0,zbuffer,Sampler::nearest());
  enc.setPipeline(shaders.copy);
  enc.draw(Resources::fsqVbo());
  }
  device.submit(cmd,sync);
  sync.wait();

  auto pm  = device.readPixels(textureCast<const Texture2d&>(normals));
  pm.save("gbufNormal.png");

  pm  = device.readPixels(textureCast<const Texture2d&>(d16));
  pm.save("zbuffer.hdr");

  return img;
  }

float Renderer::internalResolutionScale() const {
  if(settings.vidResIndex==0)
    return 1;
  if(settings.vidResIndex==1)
    return 0.75;
  return 0.5;
  }

Size Renderer::internalResolution() const {
  if(settings.vidResIndex==0)
     return Size(int(swapchain.w()), int(swapchain.h()));
  if(settings.vidResIndex==1)
    return Size(int(3*swapchain.w()/4), int(3*swapchain.h()/4));
  return Size(int(swapchain.w()/2), int(swapchain.h()/2));
  }

