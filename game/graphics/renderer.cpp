#include "renderer.h"

#include <Tempest/Color>
#include <Tempest/Fence>
#include <Tempest/Log>

#include "ui/inventorymenu.h"
#include "camera.h"
#include "gothic.h"
#include "ui/videowidget.h"
#include "utils/string_frm.h"

#include <ui/videowidget.h>

using namespace Tempest;

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

  Log::i("GPU = ",device.properties().name);
  Log::i("Depth format = ", Tempest::formatName(zBufferFormat), " Shadow format = ", Tempest::formatName(shadowFormat));

  Gothic::inst().onSettingsChanged.bind(this,&Renderer::initSettings);
  Gothic::inst().toggleGi.bind(this, &Renderer::toggleGi);
  initSettings();
  }

Renderer::~Renderer() {
  Gothic::inst().onSettingsChanged.ubind(this,&Renderer::initSettings);
  }

void Renderer::resetSwapchain() {
  auto& device = Resources::device();
  device.waitIdle();

  const uint32_t w      = swapchain.w();
  const uint32_t h      = swapchain.h();
  const uint32_t smSize = settings.shadowResolution;

  auto smpN = Sampler::nearest();
  smpN.setClamping(ClampMode::ClampToEdge);

  sceneLinear = device.attachment(TextureFormat::R11G11B10UF,swapchain.w(),swapchain.h());
  zbuffer     = device.zbuffer(zBufferFormat,w,h);

  if(Gothic::options().doMeshShading) {
    uint32_t pw = nextPot(w);
    uint32_t ph = nextPot(h);

    uint32_t hw = pw;
    uint32_t hh = ph;
    while(hw>64 || hh>64) {
      hw = std::max(1u, (hw+1)/2u);
      hh = std::max(1u, (hh+1)/2u);
      }

    hiz.hiZ    = device.image2d(TextureFormat::R16, hw, hh, true);
    hiz.uboPot = device.descriptors(Shaders::inst().hiZPot);
    hiz.uboPot.set(0, zbuffer, smpN);
    hiz.uboPot.set(1, hiz.hiZ);

    hiz.uboMip.clear();
    for(uint32_t i=0; (hw>1 || hh>1); ++i) {
      hw = std::max(1u, hw/2u);
      hh = std::max(1u, hh/2u);
      auto& ubo = hiz.uboMip.emplace_back(device.descriptors(Shaders::inst().hiZMip));
      ubo.set(0, hiz.hiZ, smpN, i);
      ubo.set(1, hiz.hiZ, smpN, i+1);
      }

    if(smSize>0) {
      hiz.smProj    = device.zbuffer(shadowFormat, smSize, smSize);
      hiz.uboReproj = device.descriptors(Shaders::inst().hiZReproj);
      hiz.uboReproj.set(0, zbuffer, smpN);
      // hiz.uboReproj.set(1, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);

      hiz.hiZSm1    = device.image2d(TextureFormat::R16, 64, 64, true);
      hiz.uboPotSm1 = device.descriptors(Shaders::inst().hiZPot);
      hiz.uboPotSm1.set(0, hiz.smProj, smpN);
      hiz.uboPotSm1.set(1, hiz.hiZSm1);

      hw = hh = 64;
      hiz.uboMipSm1.clear();
      for(uint32_t i=0; (hw>1 || hh>1); ++i) {
        hw = std::max(1u, hw/2u);
        hh = std::max(1u, hh/2u);
        auto& ubo = hiz.uboMipSm1.emplace_back(device.descriptors(Shaders::inst().hiZMip));
        ubo.set(0, hiz.hiZSm1, smpN, i);
        ubo.set(1, hiz.hiZSm1, smpN, i+1);
        }
      }
    }

  if(smSize>0) {
    for(int i=0; i<Resources::ShadowLayers; ++i)
      shadowMap[i] = device.zbuffer(shadowFormat,smSize,smSize);
    }

  sceneOpaque = device.attachment(TextureFormat::R11G11B10UF,swapchain.w(),swapchain.h());
  sceneDepth  = device.attachment(TextureFormat::R32F,       swapchain.w(),swapchain.h());

  gbufDiffuse = device.attachment(TextureFormat::RGBA8,      swapchain.w(),swapchain.h());
  gbufNormal  = device.attachment(TextureFormat::R11G11B10UF,swapchain.w(),swapchain.h());

  uboStash = device.descriptors(Shaders::inst().stash);
  uboStash.set(0,sceneLinear,Sampler::nearest());
  uboStash.set(1,zbuffer,    Sampler::nearest());

  if(Gothic::options().doRayQuery && Resources::device().properties().descriptors.nonUniformIndexing &&
     settings.shadowResolution>0)
    shadow.composePso = &Shaders::inst().shadowResolveRq;
  else if(settings.shadowResolution>0)
    shadow.composePso = &Shaders::inst().shadowResolveSh;
  else
    shadow.composePso = &Shaders::inst().shadowResolve;
  shadow.ubo = device.descriptors(*shadow.composePso);

  water.underUbo = device.descriptors(Shaders::inst().underwaterT);

  ssao.ssaoBuf = device.image2d(ssao.aoFormat, swapchain.w(),swapchain.h());
  ssao.ssaoPso = &Shaders::inst().ssao;
  ssao.uboSsao = device.descriptors(*ssao.ssaoPso);

  irradiance.lut = device.image2d(TextureFormat::RGBA32F, 3,2);
  irradiance.pso = &Shaders::inst().irradiance;
  irradiance.ubo = device.descriptors(*irradiance.pso);

  tonemapping.pso     = &Shaders::inst().tonemapping;
  tonemapping.uboTone = device.descriptors(*tonemapping.pso);

  if(settings.giEnabled) {
    gi.uboDbg         = DescriptorSet();

    gi.probeInitPso   = &Shaders::inst().probeInit;

    gi.probeClearPso  = &Shaders::inst().probeClear;
    gi.probeClearHPso = &Shaders::inst().probeClearHash;
    gi.probeMakeHPso  = &Shaders::inst().probeMakeHash;
    gi.uboClear       = device.descriptors(*gi.probeClearPso);

    gi.probeVotePso   = &Shaders::inst().probeVote;
    gi.probeAllocPso  = &Shaders::inst().probeAlocation;
    gi.probePrunePso  = &Shaders::inst().probePrune;
    gi.uboProbes      = device.descriptors(*gi.probeAllocPso);

    gi.uboZeroIrr     = device.descriptors(Shaders::inst().copyImg);
    gi.uboPrevIrr     = device.descriptors(Shaders::inst().copyImg);

    gi.probeTracePso  = &Shaders::inst().probeTrace;
    gi.uboTrace       = device.descriptors(*gi.probeTracePso);

    gi.probeLightPso  = &Shaders::inst().probeLighting;
    gi.uboLight       = device.descriptors(*gi.probeLightPso);

    gi.probeDrawPso   = &Shaders::inst().probeDraw;
    gi.uboDraw        = device.descriptors(*gi.probeDrawPso);

    const uint32_t maxProbes = gi.maxProbes;
    if(gi.hashTable.isEmpty()) {
      gi.hashTable          = device.ssbo(nullptr, 2'097'152*sizeof(uint32_t)); // 8MB
      gi.voteTable          = device.ssbo(nullptr, gi.hashTable.byteSize());
      gi.probes             = device.ssbo(nullptr, maxProbes*32 + 64);        // probes and header
      gi.freeList           = device.ssbo(nullptr, maxProbes*sizeof(uint32_t) + sizeof(int32_t));
      gi.probesGBuffDiff    = device.image2d(TextureFormat::RGBA8, gi.atlasDim*16, gi.atlasDim*16); // 16x16 tile
      gi.probesGBuffNorm    = device.image2d(TextureFormat::RGBA8, gi.atlasDim*16, gi.atlasDim*16);
      gi.probesGBuffRayT    = device.image2d(TextureFormat::R16,   gi.atlasDim*16, gi.atlasDim*16);
      gi.probesLighting     = device.image2d(TextureFormat::R11G11B10UF, gi.atlasDim*3, gi.atlasDim*2);
      gi.probesLightingPrev = device.image2d(TextureFormat::R11G11B10UF, uint32_t(gi.probesLighting.w()), uint32_t(gi.probesLighting.h()));
      gi.fisrtFrame          = true;
      }
    }

  prepareUniforms();
  prepareRtUniforms();
  }

void Renderer::initSettings() {
  settings.zEnvMappingEnabled = Gothic::inst().settingsGetI("ENGINE","zEnvMappingEnabled")!=0;
  settings.zCloudShadowScale  = Gothic::inst().settingsGetI("ENGINE","zCloudShadowScale") !=0;

  settings.zVidBrightness     = Gothic::inst().settingsGetF("VIDEO","zVidBrightness");
  settings.zVidContrast       = Gothic::inst().settingsGetF("VIDEO","zVidContrast");
  settings.zVidGamma          = Gothic::inst().settingsGetF("VIDEO","zVidGamma");

  if(!Gothic::options().doRtGi)
    settings.giEnabled = false;

  auto prevCompose = water.reflectionsPso;
  if(settings.zCloudShadowScale)
    ssao.ambientComposePso = &Shaders::inst().ambientComposeSsao; else
    ssao.ambientComposePso = &Shaders::inst().ambientCompose;

  auto prevRefl = water.reflectionsPso;
  if(settings.zEnvMappingEnabled)
    water.reflectionsPso = &Shaders::inst().waterReflectionSSR; else
    water.reflectionsPso = &Shaders::inst().waterReflection;

  if(ssao.ambientComposePso!=prevCompose ||
     water.reflectionsPso  !=prevRefl) {
    auto& device = Resources::device();
    device.waitIdle();
    water.ubo       = device.descriptors(*water.reflectionsPso);
    ssao.uboCompose = device.descriptors(*ssao.ambientComposePso);
    prepareUniforms();
    }
  }

void Renderer::toggleGi() {
  if(!Gothic::options().doRtGi)
    return;
  settings.giEnabled = !settings.giEnabled;
  gi.fisrtFrame = true;

  auto& device = Resources::device();
  device.waitIdle();
  prepareUniforms();
  }

void Renderer::onWorldChanged() {
  gi.fisrtFrame = true;
  prepareUniforms();
  }

void Renderer::updateCamera(const Camera& camera) {
  proj        = camera.projective();
  viewProj    = camera.viewProj();
  viewProjLwc = camera.viewProjLwc();

  if(auto wview=Gothic::inst().worldView()) {
    for(size_t i=0; i<Resources::ShadowLayers; ++i)
      shadowMatrix[i] = camera.viewShadow(wview->mainLight().dir(),i);
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

  auto smpN = Sampler::nearest();
  smpN.setClamping(ClampMode::ClampToEdge);

  if(!hiz.uboReproj.isEmpty()) {
    hiz.uboReproj.set(1, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    }

  ssao.uboSsao.set(0, ssao.ssaoBuf);
  ssao.uboSsao.set(1, gbufDiffuse, smpN);
  ssao.uboSsao.set(2, gbufNormal,  smpN);
  ssao.uboSsao.set(3, zbuffer,     smpN);
  ssao.uboSsao.set(4, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);

  ssao.uboCompose.set(0, gbufDiffuse,  smpN);
  ssao.uboCompose.set(1, gbufNormal,   smpN);
  ssao.uboCompose.set(2, zbuffer,      smpN);
  ssao.uboCompose.set(3, irradiance.lut);
  if(settings.zCloudShadowScale)
    ssao.uboCompose.set(4, ssao.ssaoBuf, smpN);

  tonemapping.uboTone.set(0, sceneLinear);

  shadow.ubo.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  shadow.ubo.set(1, gbufDiffuse);
  shadow.ubo.set(2, gbufNormal);
  shadow.ubo.set(3, zbuffer);

  for(size_t r=0; r<Resources::ShadowLayers; ++r) {
    if(shadowMap[r].isEmpty())
      continue;
    shadow.ubo.set(4+r, shadowMap[r]);
    }

  water.underUbo.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  water.underUbo.set(1, zbuffer);

  irradiance.ubo.set(0, irradiance.lut);
  irradiance.ubo.set(1, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  irradiance.ubo.set(2, wview->sky().skyLut());

  {
    auto& sky = wview->sky();

    auto smp = Sampler::bilinear();
    smp.setClamping(ClampMode::MirroredRepeat);

    auto smpd = Sampler::nearest();
    smpd.setClamping(ClampMode::ClampToEdge);

    water.ubo.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    water.ubo.set(1, sceneOpaque, smp);
    water.ubo.set(2, gbufDiffuse, smp);
    water.ubo.set(3, gbufNormal,  smp);
    water.ubo.set(4, zbuffer,     smpd);
    water.ubo.set(5, sceneDepth,  smpd);

    water.ubo.set(6,  wview->sky().skyLut());
    water.ubo.set(7, *sky.cloudsDay()  .lay[0],Sampler::bilinear());
    water.ubo.set(8, *sky.cloudsDay()  .lay[1],Sampler::bilinear());
    water.ubo.set(9, *sky.cloudsNight().lay[0],Sampler::bilinear());
    water.ubo.set(10,*sky.cloudsNight().lay[1],Sampler::bilinear());
  }

  if(settings.giEnabled) {
    auto smpN = Sampler::nearest();
    smpN.setClamping(ClampMode::ClampToEdge);

    gi.uboClear.set(0, gi.voteTable);
    gi.uboClear.set(1, gi.hashTable);
    gi.uboClear.set(2, gi.probes);
    gi.uboClear.set(3, gi.freeList);

    gi.uboProbes.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    gi.uboProbes.set(1, gbufDiffuse, Sampler::nearest());
    gi.uboProbes.set(2, gbufNormal,  Sampler::nearest());
    gi.uboProbes.set(3, zbuffer,     Sampler::nearest());
    gi.uboProbes.set(4, gi.voteTable);
    gi.uboProbes.set(5, gi.hashTable);
    gi.uboProbes.set(6, gi.probes);
    gi.uboProbes.set(7, gi.freeList);

    gi.uboTrace.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    gi.uboTrace.set(1, gi.probesGBuffDiff);
    gi.uboTrace.set(2, gi.probesGBuffNorm);
    gi.uboTrace.set(3, gi.probesGBuffRayT);
    gi.uboTrace.set(4, gi.hashTable);
    gi.uboTrace.set(5, gi.probes);

    gi.uboPrevIrr.set(0, gi.probesLighting);
    gi.uboZeroIrr.set(1, Resources::fallbackBlack());

    gi.uboPrevIrr.set(0, gi.probesLighting);
    gi.uboPrevIrr.set(1, gi.probesLightingPrev);

    gi.uboLight.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    gi.uboLight.set(1, gi.probesLighting);
    gi.uboLight.set(2, gi.probesGBuffDiff, Sampler::nearest());
    gi.uboLight.set(3, gi.probesGBuffNorm, Sampler::nearest());
    gi.uboLight.set(4, gi.probesGBuffRayT, Sampler::nearest());
    gi.uboLight.set(5, wview->sky().skyLut(), Sampler::bilinear());
    gi.uboLight.set(6, shadowMap[1], Sampler::bilinear());
    gi.uboLight.set(7, gi.probesLightingPrev, Sampler::nearest());
    gi.uboLight.set(8, gi.hashTable);
    gi.uboLight.set(9, gi.probes);

    gi.uboDraw.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    gi.uboDraw.set(1, gi.probesLighting);
    gi.uboDraw.set(2, gbufDiffuse,  Sampler::nearest());
    gi.uboDraw.set(3, gbufNormal,   Sampler::nearest());
    gi.uboDraw.set(4, zbuffer,      Sampler::nearest());
    gi.uboDraw.set(5, ssao.ssaoBuf, smpN);
    gi.uboDraw.set(6, gi.hashTable);
    gi.uboDraw.set(7, gi.probes);
    }

  const Texture2d* sh[Resources::ShadowLayers] = {};
  for(size_t i=0; i<Resources::ShadowLayers; ++i)
    if(!shadowMap[i].isEmpty()) {
      sh[i] = &textureCast(shadowMap[i]);
      }
  wview->setShadowMaps(sh);

  wview->setHiZ(textureCast(hiz.hiZ));
  wview->setGbuffer(textureCast(gbufDiffuse), textureCast(gbufNormal));
  wview->setSceneImages(textureCast(sceneOpaque), textureCast(sceneDepth), zbuffer);
  wview->prepareUniforms();
  }

void Renderer::prepareRtUniforms() {
  auto wview = Gothic::inst().worldView();
  if(wview==nullptr)
    return;
  auto& scene = wview->sceneGlobals();
  if(scene.rtScene.tlas.isEmpty())
    return;

  if(shadow.composePso==&Shaders::inst().shadowResolveRq) {
    shadow.ubo.set(6, scene.rtScene.tlas);
    shadow.ubo.set(7, Sampler::bilinear());
    shadow.ubo.set(8, scene.rtScene.tex);
    shadow.ubo.set(9, scene.rtScene.vbo);
    shadow.ubo.set(10,scene.rtScene.ibo);
    shadow.ubo.set(11,scene.rtScene.rtDesc);
    }

  if(!gi.uboTrace.isEmpty()) {
    gi.uboTrace.set(6, scene.rtScene.tlas);
    gi.uboTrace.set(7, Sampler::bilinear());
    gi.uboTrace.set(8, scene.rtScene.tex);
    gi.uboTrace.set(9, scene.rtScene.vbo);
    gi.uboTrace.set(10,scene.rtScene.ibo);
    gi.uboTrace.set(11,scene.rtScene.rtDesc);
    }
  }

void Renderer::prepareSky(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  cmd.setDebugMarker("Sky LUT");
  wview.prepareSky(cmd,fId);
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
    cmd.setFramebuffer({{result, Tempest::Preserve, Tempest::Preserve}},{zbuffer, 1.f, Tempest::Preserve});
    cmd.setDebugMarker("Inventory");
    inventory.draw(cmd,cmdId);

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
  //tex.push_back(&textureCast(hiz.hiZ));
  //tex.push_back(&textureCast(hiz.smProj));
  //tex.push_back(&textureCast(hiz.hiZSm1));
  tex.push_back(&textureCast(shadowMap[1]));
  //tex.push_back(&textureCast(shadowMap[0]));

  int left = 10;
  for(auto& t:tex) {
    p.setBrush(Brush(*t,Painter::Alpha,ClampMode::ClampToBorder));
    auto sz = Size(p.brush().w(),p.brush().h());
    if(sz.isEmpty())
      continue;
    const int size = 200;
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

void Renderer::draw(Tempest::Attachment& result, Tempest::Encoder<CommandBuffer>& cmd, uint8_t fId) {
  auto wview  = Gothic::inst().worldView();
  auto camera = Gothic::inst().camera();
  if(wview==nullptr || camera==nullptr) {
    cmd.setFramebuffer({{result, Vec4(), Tempest::Preserve}});
    return;
    }

  if(wview->updateRtScene())
    prepareRtUniforms();

  wview->updateLight();
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
    wview->visibilityPass(frustrum);
    }

  wview->preFrameUpdate(*camera,Gothic::inst().world()->tickCount(),fId);
  wview->prepareGlobals(cmd,fId);

  drawHiZ(cmd,fId,*wview);
  prepareSky(cmd,fId,*wview);

  drawGBuffer  (cmd,fId,*wview);
  drawShadowMap(cmd,fId,*wview);

  prepareSSAO(cmd);
  prepareFog (cmd,fId,*wview);
  prepareGi(cmd,fId);

  cmd.setFramebuffer({{sceneLinear, Tempest::Discard, Tempest::Preserve}}, {zbuffer, Tempest::Readonly});
  drawShadowResolve(cmd,fId,*wview);
  drawAmbient(cmd,*wview);
  drawLights(cmd,fId,*wview);
  drawSky(cmd,fId,*wview);

  stashSceneAux(cmd,fId);

  drawGWater(cmd,fId,*wview);

  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}}, {zbuffer, Tempest::Preserve, Tempest::Preserve});
  cmd.setDebugMarker("Sun&Moon");
  wview->drawSunMoon(cmd,fId);
  cmd.setDebugMarker("Translucent");
  wview->drawTranslucent(cmd,fId);

  drawProbesDbg(cmd, fId);

  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}});
  drawReflections(cmd,fId);
  if(camera->isInWater()) {
    cmd.setDebugMarker("Underwater");
    drawUnderwater(cmd,fId);
    } else {
    cmd.setDebugMarker("Fog");
    wview->drawFog(cmd,fId);
    }

  cmd.setFramebuffer({{result, Tempest::Discard, Tempest::Preserve}});
  cmd.setDebugMarker("Tonemapping");
  drawTonemapping(cmd);
  }

void Renderer::drawTonemapping(Tempest::Encoder<Tempest::CommandBuffer>& cmd) {
  struct Push {
    float exposure   = 1.0;
    float brightness = 0;
    float contrast   = 1;
    float gamma      = 1.f/2.2f;
    };
  Push p;
  if(auto wview = Gothic::inst().worldView()) {
    p.exposure = wview->sky().autoExposure();
    }

  p.brightness = (settings.zVidBrightness - 0.5f)*0.1f;
  p.contrast   = std::max(1.5f - settings.zVidContrast, 0.01f);
  p.gamma      = p.gamma/std::max(2.0f*settings.zVidGamma,  0.01f);

  cmd.setUniforms(*tonemapping.pso, tonemapping.uboTone, &p, sizeof(p));
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::stashSceneAux(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  auto& device = Resources::device();
  if(!device.properties().hasSamplerFormat(zBufferFormat))
    return;
  cmd.setFramebuffer({{sceneOpaque, Tempest::Discard, Tempest::Preserve}, {sceneDepth, Tempest::Discard, Tempest::Preserve}});
  cmd.setDebugMarker("Stash scene");
  cmd.setUniforms(Shaders::inst().stash, uboStash);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawHiZ(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view) {
  if(!Gothic::options().doMeshShading)
    return;

  cmd.setDebugMarker("HiZ-occluders");
  cmd.setFramebuffer({}, {zbuffer, 1.f, Tempest::Preserve});
  view.drawHiZ(cmd,fId);

  cmd.setDebugMarker("HiZ-mip");
  cmd.setFramebuffer({});
  cmd.setUniforms(Shaders::inst().hiZPot, hiz.uboPot);
  cmd.dispatch(size_t(hiz.hiZ.w()), size_t(hiz.hiZ.h()));

  uint32_t w = uint32_t(hiz.hiZ.w()), h = uint32_t(hiz.hiZ.h());
  for(uint32_t i=0; i<hiz.uboMip.size(); ++i) {
    w = w/2;
    h = h/2;
    cmd.setUniforms(Shaders::inst().hiZMip, hiz.uboMip[i]);
    cmd.dispatchThreads(std::max<uint32_t>(w,1),std::max<uint32_t>(h,1));
    }

  /*
  cmd.setDebugMarker("HiZ-shadows");
  cmd.setFramebuffer({}, {hiz.smProj, 0, Tempest::Preserve});
  cmd.setUniforms(Shaders::inst().hiZReproj, hiz.uboReproj);
  cmd.dispatchMeshThreads(zbuffer.size());

  cmd.setFramebuffer({});
  cmd.setUniforms(Shaders::inst().hiZPot, hiz.uboPotSm1);
  cmd.dispatch(size_t(hiz.hiZSm1.w()), size_t(hiz.hiZSm1.h()));

  w = uint32_t(hiz.hiZSm1.w()), h = uint32_t(hiz.hiZSm1.h());
  for(uint32_t i=0; i<hiz.uboMipSm1.size(); ++i) {
    w = w/2;
    h = h/2;
    cmd.setUniforms(Shaders::inst().hiZMip, hiz.uboMipSm1[i]);
    cmd.dispatchThreads(std::max<uint32_t>(w,1),std::max<uint32_t>(h,1));
    }
  */
  }

void Renderer::drawGBuffer(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view) {
  if(Gothic::options().doMeshShading) {
    cmd.setFramebuffer({{gbufDiffuse, Tempest::Vec4(),  Tempest::Preserve},
                        {gbufNormal,  Tempest::Discard, Tempest::Preserve}},
                       {zbuffer, Tempest::Preserve, Tempest::Preserve});
    } else {
    cmd.setFramebuffer({{gbufDiffuse, Tempest::Discard, Tempest::Preserve},
                        {gbufNormal,  Tempest::Discard, Tempest::Preserve}},
                       {zbuffer, 1.f, Tempest::Preserve});
    }
  cmd.setDebugMarker("GBuffer");
  view.drawGBuffer(cmd,fId);
  }

void Renderer::drawGWater(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view) {
  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve},
                      {gbufDiffuse, Vec4(0,0,0,0),     Tempest::Preserve},
                      {gbufNormal,  Tempest::Preserve, Tempest::Preserve}},
                     {zbuffer, Tempest::Preserve, Tempest::Preserve});
  // cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}},
  //                    {zbuffer, Tempest::Preserve, Tempest::Preserve});
  cmd.setDebugMarker("GWater");
  view.drawWater(cmd,fId);
  }

void Renderer::drawReflections(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  cmd.setDebugMarker("Reflections");
  cmd.setUniforms(*water.reflectionsPso, water.ubo);
  if(Gothic::options().doMeshShading) {
    cmd.dispatchMeshThreads(gbufDiffuse.size());
    } else {
    cmd.draw(Resources::fsqVbo());
    }
  }

void Renderer::drawUnderwater(Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  cmd.setUniforms(Shaders::inst().underwaterT, water.underUbo);
  cmd.draw(Resources::fsqVbo());

  cmd.setUniforms(Shaders::inst().underwaterS, water.underUbo);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawShadowMap(Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view) {
  if(settings.shadowResolution<=0)
    return;

  for(uint8_t i=0; i<Resources::ShadowLayers; ++i) {
    cmd.setFramebuffer({}, {shadowMap[i], 0.f, Tempest::Preserve});
    cmd.setDebugMarker(string_frm("ShadowMap #",i));
    if(view.mainLight().dir().y > Camera::minShadowY)
      view.drawShadow(cmd,fId,i);
    }
  }

void Renderer::drawShadowResolve(Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, const WorldView& view) {
  static bool useDsm = true;
  if(!useDsm)
    return;
  cmd.setDebugMarker("DirectSunLight");
  cmd.setUniforms(*shadow.composePso, shadow.ubo);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawLights(Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  cmd.setDebugMarker("Point lights");
  wview.drawLights(cmd,fId);
  }

void Renderer::drawSky(Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  cmd.setDebugMarker("Sky");
  wview.drawSky(cmd,fId);
  }

void Renderer::prepareSSAO(Encoder<Tempest::CommandBuffer>& cmd) {
  if(!settings.zCloudShadowScale)
    return;
  // ssao
  struct PushSsao {
    Matrix4x4 mvp;
    Matrix4x4 mvpInv;
    } push;
  push.mvp    = viewProjLwc;
  push.mvpInv = viewProjLwc;
  push.mvpInv.inverse();

  cmd.setFramebuffer({});
  cmd.setDebugMarker("SSAO");
  cmd.setUniforms(*ssao.ssaoPso, ssao.uboSsao, &push, sizeof(push));
  cmd.dispatchThreads(ssao.ssaoBuf.size());
  }

void Renderer::prepareFog(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  cmd.setDebugMarker("Fog LUTs");
  wview.prepareFog(cmd,fId);
  }

void Renderer::prepareIrradiance(Tempest::Encoder<CommandBuffer>& cmd, uint8_t fId) {
  cmd.setFramebuffer({});
  cmd.setDebugMarker("Irradiance");
  cmd.setUniforms(*irradiance.pso, irradiance.ubo);
  cmd.dispatch(1);
  }

void Renderer::prepareGi(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  if(!settings.giEnabled) {
    prepareIrradiance(cmd,fId);
    return;
    }

  if(!settings.zCloudShadowScale)
    return;

  const size_t maxHash = gi.hashTable.byteSize()/sizeof(uint32_t);

  cmd.setFramebuffer({});

  if(gi.fisrtFrame) {
    cmd.setDebugMarker("GI-Init");
    cmd.setUniforms(*gi.probeInitPso, gi.uboClear);
    cmd.dispatch(1);

    cmd.setUniforms(*gi.probeClearHPso, gi.uboClear);
    cmd.dispatchThreads(maxHash);

    cmd.setUniforms(Shaders::inst().copyImg, gi.uboZeroIrr);
    cmd.dispatchThreads(gi.probesLighting.size());
    gi.fisrtFrame = false;
    }

  static bool alloc = true;
  cmd.setDebugMarker("GI-Alloc");
  cmd.setUniforms(*gi.probeClearPso, gi.uboClear);
  cmd.dispatchThreads(maxHash);

  if(alloc) {
    cmd.setUniforms(*gi.probeVotePso, gi.uboProbes);
    cmd.dispatchThreads(sceneDepth.size());

    cmd.setUniforms(*gi.probePrunePso, gi.uboClear);
    cmd.dispatchThreads(gi.maxProbes);

    cmd.setUniforms(*gi.probeAllocPso, gi.uboProbes);
    cmd.dispatchThreads(sceneDepth.size());
    }

  cmd.setDebugMarker("GI-Trace");
  cmd.setUniforms(*gi.probeTracePso, gi.uboTrace);
  cmd.dispatch(1024); // dispath indirect? :(

  cmd.setDebugMarker("GI-HashMap");
  cmd.setUniforms(*gi.probeClearHPso, gi.uboClear);
  cmd.dispatchThreads(maxHash);
  cmd.setUniforms(*gi.probeMakeHPso, gi.uboClear);
  cmd.dispatchThreads(gi.maxProbes);

  cmd.setDebugMarker("GI-Lighting");
  cmd.setUniforms(Shaders::inst().copyImg, gi.uboPrevIrr);
  cmd.dispatchThreads(gi.probesLighting.size());

  cmd.setUniforms(*gi.probeLightPso, gi.uboLight);
  cmd.dispatch(1024);
  }

void Renderer::drawProbesDbg(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  if(!settings.giEnabled)
    return;

  static bool enable = false;
  if(!enable)
    return;

  auto& device = Resources::device();
  auto& pso    = Shaders::inst().probeDbg;

  if(auto wview=Gothic::inst().worldView()) {
    if(gi.uboDbg.isEmpty()) {
      gi.uboDbg = device.descriptors(pso);
      gi.uboDbg.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
      gi.uboDbg.set(1, gi.probesLighting);
      gi.uboDbg.set(2, gi.probes);
      gi.uboDbg.set(3, gi.hashTable);
      }
    }

  cmd.setDebugMarker("GI-dbg");
  cmd.setUniforms(pso, gi.uboDbg);
  cmd.draw(36, 0, gi.maxProbes);
  }

void Renderer::drawAmbient(Encoder<CommandBuffer>& cmd, const WorldView& view) {
  if(settings.giEnabled && settings.zCloudShadowScale) {
    cmd.setDebugMarker("AmbientLight");
    cmd.setUniforms(*gi.probeDrawPso, gi.uboDraw);
    cmd.draw(Resources::fsqVbo());
    return;
    }

  struct Push {
    Vec3      ambient;
    float     exposure = 1;
    Vec3      ldir;
    float     padd1 = 0;
    Vec3      clipInfo;
  } push;
  push.ambient  = view.ambientLight();
  push.ldir     = view.mainLight().dir();
  push.clipInfo = clipInfo;
  push.exposure = view.sky().autoExposure();

  cmd.setDebugMarker("AmbientLight");
  cmd.setUniforms(*ssao.ambientComposePso,ssao.uboCompose,&push,sizeof(push));
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

  auto ubo = device.descriptors(Shaders::inst().copy);
  ubo.set(0,gbufNormal,Sampler::nearest());
  {
  auto enc = cmd.startEncoding(device);
  enc.setFramebuffer({{normals,Tempest::Discard,Tempest::Preserve}});
  enc.setUniforms(Shaders::inst().copy,ubo);
  enc.draw(Resources::fsqVbo());
  }
  device.submit(cmd,sync);
  sync.wait();

  ubo.set(0,zbuffer,Sampler::nearest());
  {
  auto enc = cmd.startEncoding(device);
  enc.setFramebuffer({{d16,Tempest::Discard,Tempest::Preserve}});
  enc.setUniforms(Shaders::inst().copy,ubo);
  enc.draw(Resources::fsqVbo());
  }
  device.submit(cmd,sync);
  sync.wait();

  auto pm  = device.readPixels(textureCast(normals));
  pm.save("gbufNormal.png");

  pm  = device.readPixels(textureCast(d16));
  pm.save("zbuffer.hdr");

  return img;
  }
