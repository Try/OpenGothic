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
  Gothic::inst().toggleGi .bind(this, &Renderer::toggleGi);
  Gothic::inst().toggleVsm.bind(this, &Renderer::toggleVsm);

  settings.giEnabled  = Gothic::options().doRtGi;
  settings.vsmEnabled = Gothic::options().doVirtualShadow;
  settings.swrEnabled = Gothic::options().swRenderingPreset>0;

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

  auto& shaders = Shaders::inst();

  const auto     res    = internalResolution();
  const uint32_t w      = uint32_t(res.w);
  const uint32_t h      = uint32_t(res.h);
  const uint32_t smSize = settings.shadowResolution;

  auto smpN = Sampler::nearest();
  smpN.setClamping(ClampMode::ClampToEdge);

  sceneLinear    = device.attachment(TextureFormat::R11G11B10UF,w,h);

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

  hiz.atomicImg = device.properties().hasAtomicFormat(TextureFormat::R32U);

  uint32_t pw = nextPot(w);
  uint32_t ph = nextPot(h);

  uint32_t hw = pw;
  uint32_t hh = ph;
  while(hw>64 || hh>64) {
    hw = std::max(1u, (hw+1)/2u);
    hh = std::max(1u, (hh+1)/2u);
    }

  hiz.hiZ       = device.image2d(TextureFormat::R16,  hw, hh, true);
  hiz.uboPot    = device.descriptors(shaders.hiZPot);
  hiz.uboPot.set(0, zbuffer, smpN);
  hiz.uboPot.set(1, hiz.hiZ);

  hiz.uboMip = device.descriptors(shaders.hiZMip);
  if(hiz.atomicImg) {
    hiz.counter = device.image2d(TextureFormat::R32U, std::max(hw/4, 1u), std::max(hh/4, 1u), false);
    hiz.uboMip.set(0, hiz.counter, Sampler::nearest(), 0);
    } else {
    hiz.counterBuf = device.ssbo(Tempest::Uninitialized, std::max(hw/4, 1u)*std::max(hh/4, 1u)*sizeof(uint32_t));
    hiz.uboMip.set(0, hiz.counterBuf);
    }
  const uint32_t maxBind = 8, mip = hiz.hiZ.mipCount();
  for(uint32_t i=0; i<maxBind; ++i)
    hiz.uboMip.set(1+i, hiz.hiZ, Sampler::nearest(), std::min(i, mip-1));

  if(smSize>0) {
    for(int i=0; i<Resources::ShadowLayers; ++i) {
      if(settings.vsmEnabled && !(settings.giEnabled && i==1) && !(sky.quality==PathTrace && i==1))
        continue; //TODO: support vsm in gi code
      shadowMap[i] = device.zbuffer(shadowFormat,smSize,smSize);
      }
    }

  sceneOpaque = device.attachment(TextureFormat::R11G11B10UF,w,h);
  sceneDepth  = device.attachment(TextureFormat::R32F,       w,h);

  gbufDiffuse = device.attachment(TextureFormat::RGBA8,w,h);
  gbufNormal  = device.attachment(TextureFormat::R32U, w,h);

  uboStash = device.descriptors(shaders.stash);
  uboStash.set(0,sceneLinear,Sampler::nearest());
  uboStash.set(1,zbuffer,    Sampler::nearest());

  if(settings.vsmEnabled)
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
  Resources::recycle(std::move(vsm.uboOmniPages));
  Resources::recycle(std::move(vsm.uboClearOmni));
  Resources::recycle(std::move(vsm.uboCullLights));

  water.underUbo = device.descriptors(shaders.underwaterT);

  ssao.ssaoBuf = device.image2d(ssao.aoFormat, w,h);
  ssao.ssaoPso = &shaders.ssao;
  ssao.uboSsao = device.descriptors(*ssao.ssaoPso);

  ssao.ssaoBlur = device.image2d(ssao.aoFormat, w,h);
  ssao.uboBlur  = device.descriptors(shaders.ssaoBlur);

  tonemapping.pso             = (settings.vidResIndex==0) ? &shaders.tonemapping : &shaders.tonemappingUpscale;
  tonemapping.uboTone         = device.descriptors(*tonemapping.pso);

  cmaa2.detectEdges2x2        = &shaders.cmaa2EdgeColor2x2Presets[Gothic::options().aaPreset];
  cmaa2.detectEdges2x2Ubo     = device.descriptors(*cmaa2.detectEdges2x2);

  cmaa2.processCandidates     = &shaders.cmaa2ProcessCandidates;
  cmaa2.processCandidatesUbo  = device.descriptors(*cmaa2.processCandidates);

  cmaa2.defferedColorApply    = &shaders.cmaa2DeferredColorApply2x2;
  cmaa2.defferedColorApplyUbo = device.descriptors(*cmaa2.defferedColorApply);

  if(settings.vsmEnabled) {
    vsm.uboDbg          = device.descriptors(shaders.vsmDbg);
    vsm.uboClear        = device.descriptors(shaders.vsmClear);
    vsm.uboPages        = device.descriptors(shaders.vsmMarkPages);
    vsm.uboFogPages     = device.descriptors(shaders.vsmFogPages);
    vsm.uboFogShadow    = device.descriptors(shaders.vsmFogShadow);
    vsm.uboClump        = device.descriptors(shaders.vsmClumpPages);
    Resources::recycle(std::move(vsm.uboAlloc));

    vsm.pagesDbgPso     = &shaders.vsmDbg;

    vsm.pageTbl         = device.image3d(TextureFormat::R32U, 32, 32, 16);
    vsm.pageHiZ         = device.image3d(TextureFormat::R32U, 32, 32, 16);
    vsm.pageData        = device.zbuffer(shadowFormat, 8192, 8192);
    vsm.vsmDbg          = device.image2d(TextureFormat::R32U, uint32_t(zbuffer.w()), uint32_t(zbuffer.h()));

    // vsm.ssTrace  = device.image2d(TextureFormat::RGBA8, w, h);
    vsm.ssTrace  = device.image2d(TextureFormat::R32U, w, h);
    vsm.epTrace  = device.image2d(TextureFormat::R16,   1024, 2*1024);
    vsm.fogDbg   = device.image2d(sky.lutRGBFormat,     1024, 2*1024);
    vsm.epipoles = device.ssbo(nullptr, shaders.vsmFogEpipolar.sizeofBuffer(3, size_t(vsm.epTrace.h())));

    auto pageCount      = uint32_t(vsm.pageData.w()/VSM_PAGE_SIZE) * uint32_t(vsm.pageData.h()/VSM_PAGE_SIZE);
    auto pageSsboSize   = shaders.vsmClear.sizeofBuffer(0, pageCount);

    vsm.pageList        = device.ssbo(nullptr, pageSsboSize);
    vsm.pageListTmp     = device.ssbo(nullptr, shaders.vsmAllocPages.sizeofBuffer(3, pageCount));
    }

  if(settings.swrEnabled) {
    //swr.outputImage = device.image2d(Tempest::RGBA8, w, h);
    swr.outputImage = device.image2d(Tempest::R32U, w, h);
    swr.uboDbg      = device.descriptors(shaders.swRenderingDbg);
    }

  resetSkyFog();
  initGiData();
  prepareUniforms();
  prepareRtUniforms();
  }

void Renderer::setupSettings() {
  auto& device = Resources::device();

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

  auto prevCompose = water.reflectionsPso;
  if(settings.zCloudShadowScale)
    ssao.ambientLightPso = &Shaders::inst().ambientLightSsao; else
    ssao.ambientLightPso = &Shaders::inst().ambientLight;

  auto prevRefl = water.reflectionsPso;
  if(settings.zEnvMappingEnabled)
    water.reflectionsPso = &Shaders::inst().waterReflectionSSR; else
    water.reflectionsPso = &Shaders::inst().waterReflection;

  if(ssao.ambientLightPso!=prevCompose ||
     water.reflectionsPso  !=prevRefl) {
    device.waitIdle();
    water.ubo       = device.descriptors(*water.reflectionsPso);
    ssao.uboCompose = device.descriptors(*ssao.ambientLightPso);
    prepareUniforms();
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
  prepareRtUniforms();
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

void Renderer::onWorldChanged() {
  gi.fisrtFrame = true;

  Resources::recycle(std::move(vsm.uboOmniPages));
  Resources::recycle(std::move(vsm.uboClearOmni));
  Resources::recycle(std::move(vsm.uboCullLights));
  Resources::recycle(std::move(vsm.uboAlloc));

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

  auto smpN = Sampler::nearest();
  smpN.setClamping(ClampMode::ClampToEdge);

  ssao.uboSsao.set(0, ssao.ssaoBuf);
  ssao.uboSsao.set(1, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  ssao.uboSsao.set(2, gbufDiffuse, smpN);
  ssao.uboSsao.set(3, gbufNormal,  smpN);
  ssao.uboSsao.set(4, zbuffer,     smpN);

  ssao.uboBlur.set(0, ssao.ssaoBlur);
  ssao.uboBlur.set(1, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  ssao.uboBlur.set(2, ssao.ssaoBuf);
  ssao.uboBlur.set(3, zbuffer, smpN);

  ssao.uboCompose.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  ssao.uboCompose.set(1, gbufDiffuse,  smpN);
  ssao.uboCompose.set(2, gbufNormal,   smpN);
  ssao.uboCompose.set(3, sky.irradianceLut);
  if(settings.zCloudShadowScale)
    ssao.uboCompose.set(4, ssao.ssaoBlur, smpN);

  {
    auto smpB = Sampler::bilinear();
    smpB.setClamping(ClampMode::ClampToEdge);
    smpB.setFiltration(Filter::Nearest); // Lanczos upscale requires nearest sampling

    tonemapping.uboTone.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    tonemapping.uboTone.set(1, sceneLinear, smpB);
  }

  if(settings.aaEnabled) {
    auto smpB = Sampler::bilinear();
    smpB.setClamping(ClampMode::ClampToEdge);

    cmaa2.detectEdges2x2Ubo.set(0, sceneLinear, smpB);
    cmaa2.detectEdges2x2Ubo.set(1, cmaa2.workingEdges);
    cmaa2.detectEdges2x2Ubo.set(2, cmaa2.shapeCandidates);
    cmaa2.detectEdges2x2Ubo.set(5, cmaa2.deferredBlendItemListHeads);
    cmaa2.detectEdges2x2Ubo.set(6, cmaa2.controlBuffer);
    cmaa2.detectEdges2x2Ubo.set(7, cmaa2.indirectBuffer);

    cmaa2.processCandidatesUbo.set(0, sceneLinear, smpB);
    cmaa2.processCandidatesUbo.set(1, cmaa2.workingEdges);
    cmaa2.processCandidatesUbo.set(2, cmaa2.shapeCandidates);
    cmaa2.processCandidatesUbo.set(3, cmaa2.deferredBlendLocationList);
    cmaa2.processCandidatesUbo.set(4, cmaa2.deferredBlendItemList);
    cmaa2.processCandidatesUbo.set(5, cmaa2.deferredBlendItemListHeads);
    cmaa2.processCandidatesUbo.set(6, cmaa2.controlBuffer);
    cmaa2.processCandidatesUbo.set(7, cmaa2.indirectBuffer);

    cmaa2.defferedColorApplyUbo.set(0, sceneLinear);
    cmaa2.defferedColorApplyUbo.set(3, cmaa2.deferredBlendLocationList);
    cmaa2.defferedColorApplyUbo.set(4, cmaa2.deferredBlendItemList);
    cmaa2.defferedColorApplyUbo.set(5, cmaa2.deferredBlendItemListHeads);
    cmaa2.defferedColorApplyUbo.set(6, cmaa2.controlBuffer);
    }

  water.underUbo.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  water.underUbo.set(1, zbuffer);

  {
    auto smp = Sampler::bilinear();
    smp.setClamping(ClampMode::MirroredRepeat);

    auto smpd = Sampler::nearest();
    smpd.setClamping(ClampMode::ClampToEdge);

    water.ubo.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    water.ubo.set(1, sceneOpaque, smp);
    water.ubo.set(2, gbufDiffuse, smpd);
    water.ubo.set(3, gbufNormal,  smpd);
    water.ubo.set(4, zbuffer,     smpd);
    water.ubo.set(5, sceneDepth,  smpd);
    water.ubo.set(6, sky.viewCldLut);
  }

  if(settings.vsmEnabled) {
    vsm.uboClear.set(0, vsm.pageList);
    vsm.uboClear.set(1, vsm.pageTbl);
    vsm.uboClear.set(2, vsm.pageHiZ);

    vsm.uboPages.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    vsm.uboPages.set(1, gbufDiffuse, Sampler::nearest());
    vsm.uboPages.set(2, gbufNormal,  Sampler::nearest());
    vsm.uboPages.set(3, zbuffer,     Sampler::nearest());
    vsm.uboPages.set(4, vsm.pageTbl);
    vsm.uboPages.set(5, vsm.pageHiZ);

    vsm.uboClump.set(0, vsm.pageList);
    vsm.uboClump.set(1, vsm.pageTbl);

    vsm.uboDbg.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    vsm.uboDbg.set(1, gbufDiffuse, Sampler::nearest());
    vsm.uboDbg.set(2, gbufNormal,  Sampler::nearest());
    vsm.uboDbg.set(3, zbuffer,     Sampler::nearest());
    vsm.uboDbg.set(4, vsm.pageTbl);
    vsm.uboDbg.set(5, vsm.pageList);
    vsm.uboDbg.set(6, vsm.pageData);
    vsm.uboDbg.set(8, wview->sceneGlobals().vsmDbg);
    }

  const bool doVirtualFog = sky.quality!=VolumetricLQ;
  if(settings.vsmEnabled && doVirtualFog) {
    vsm.uboFogPages.set(0, vsm.epTrace);
    vsm.uboFogPages.set(1, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    vsm.uboFogPages.set(2, vsm.epipoles);
    vsm.uboFogPages.set(3, vsm.pageTbl);
    vsm.uboFogPages.set(4, vsm.pageHiZ);

    vsm.uboFogShadow.set(0, vsm.epTrace);
    vsm.uboFogShadow.set(1, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    vsm.uboFogShadow.set(2, vsm.epipoles);
    vsm.uboFogShadow.set(3, vsm.pageTbl);
    vsm.uboFogShadow.set(4, vsm.pageData);
    }

  if(settings.swrEnabled) {
    swr.uboDbg.set(0, swr.outputImage);
    swr.uboDbg.set(1, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    swr.uboDbg.set(2, gbufDiffuse, Sampler::nearest());
    swr.uboDbg.set(3, gbufNormal,  Sampler::nearest());
    swr.uboDbg.set(4, zbuffer,     Sampler::nearest());
    }

  const Texture2d* sh[Resources::ShadowLayers] = {};
  for(size_t i=0; i<Resources::ShadowLayers; ++i)
    if(!shadowMap[i].isEmpty()) {
      sh[i] = &textureCast<const Texture2d&>(shadowMap[i]);
      }
  wview->setShadowMaps(sh);
  wview->setVirtualShadowMap(vsm.pageData, vsm.pageTbl, vsm.pageHiZ, vsm.pageList);
  wview->setSwRenderingImage(swr.outputImage);

  wview->setHiZ(textureCast<const Texture2d&>(hiz.hiZ));
  wview->setGbuffer(textureCast<const Texture2d&>(gbufDiffuse), textureCast<const Texture2d&>(gbufNormal));
  wview->setSceneImages(textureCast<const Texture2d&>(sceneOpaque), textureCast<const Texture2d&>(sceneDepth), zbuffer);
  wview->prepareUniforms();
  }

void Renderer::prepareRtUniforms() {
  }

void Renderer::resetSkyFog() {
  auto& device = Resources::device();

  const auto     res    = internalResolution();
  const uint32_t w      = uint32_t(res.w);
  const uint32_t h      = uint32_t(res.h);

  Resources::recycle(std::move(vsm.uboFogTrace));
  Resources::recycle(std::move(vsm.uboEpipole));
  Resources::recycle(std::move(vsm.uboFogSample));

  Resources::recycle(std::move(sky.uboFogViewLut3d));
  Resources::recycle(std::move(sky.uboOcclusion));

  Resources::recycle(std::move(sky.fogLut3D));
  Resources::recycle(std::move(sky.fogLut3DMs));
  Resources::recycle(std::move(sky.occlusionLut));

  Resources::recycle(std::move(sky.uboFog));
  Resources::recycle(std::move(sky.uboFog3d));

  Resources::recycle(std::move(sky.uboSky));
  Resources::recycle(std::move(sky.uboSkyPathtrace));
  Resources::recycle(std::move(sky.uboExp));
  Resources::recycle(std::move(sky.uboIrradiance));

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

void Renderer::prepareSky(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  auto& shaders = Shaders::inst();
  auto& scene   = wview.sceneGlobals();

  auto smp  = Sampler::trillinear();
  auto smpB = Sampler::bilinear();
  smpB.setClamping(ClampMode::ClampToEdge);

  cmd.setDebugMarker("Sky LUT");

  if(!sky.lutIsInitialized) {
    sky.lutIsInitialized = true;

    cmd.setFramebuffer({});
    cmd.setBinding(0, sky.cloudsLut);
    cmd.setBinding(5, *wview.sky().cloudsDay()  .lay[0], smp);
    cmd.setBinding(6, *wview.sky().cloudsDay()  .lay[1], smp);
    cmd.setBinding(7, *wview.sky().cloudsNight().lay[0], smp);
    cmd.setBinding(8, *wview.sky().cloudsNight().lay[1], smp);
    cmd.setUniforms(shaders.cloudsLut);
    cmd.dispatchThreads(size_t(sky.cloudsLut.w()), size_t(sky.cloudsLut.h()));

    auto sz = Vec2(float(sky.transLut.w()), float(sky.transLut.h()));
    cmd.setFramebuffer({{sky.transLut, Tempest::Discard, Tempest::Preserve}});
    cmd.setBinding(5, *wview.sky().cloudsDay()  .lay[0],smp);
    cmd.setBinding(6, *wview.sky().cloudsDay()  .lay[1],smp);
    cmd.setBinding(7, *wview.sky().cloudsNight().lay[0],smp);
    cmd.setBinding(8, *wview.sky().cloudsNight().lay[1],smp);
    cmd.setUniforms(shaders.skyTransmittance, &sz, sizeof(sz));
    cmd.draw(Resources::fsqVbo());

    sz = Vec2(float(sky.multiScatLut.w()), float(sky.multiScatLut.h()));
    cmd.setFramebuffer({{sky.multiScatLut, Tempest::Discard, Tempest::Preserve}});
    cmd.setBinding(0, sky.transLut, smpB);
    cmd.setUniforms(shaders.skyMultiScattering, &sz, sizeof(sz));
    cmd.draw(Resources::fsqVbo());
    }

  auto sz = Vec2(float(sky.viewLut.w()), float(sky.viewLut.h()));
  cmd.setFramebuffer({{sky.viewLut, Tempest::Discard, Tempest::Preserve}});
  cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, sky.transLut,     smpB);
  cmd.setBinding(2, sky.multiScatLut, smpB);
  cmd.setBinding(3, sky.cloudsLut,    smpB);
  cmd.setUniforms(shaders.skyViewLut, &sz, sizeof(sz));
  cmd.draw(Resources::fsqVbo());

  sz = Vec2(float(sky.viewCldLut.w()), float(sky.viewCldLut.h()));
  cmd.setFramebuffer({{sky.viewCldLut, Tempest::Discard, Tempest::Preserve}});
  cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, sky.viewLut);
  cmd.setBinding(2, *wview.sky().cloudsDay()  .lay[0], smp);
  cmd.setBinding(3, *wview.sky().cloudsDay()  .lay[1], smp);
  cmd.setBinding(4, *wview.sky().cloudsNight().lay[0], smp);
  cmd.setBinding(5, *wview.sky().cloudsNight().lay[1], smp);
  cmd.setUniforms(shaders.skyViewCldLut, &sz, sizeof(sz));
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

  if(wview->updateRtScene()) {
    prepareRtUniforms();
    }

  if(wview->updateLights()) {
    Resources::recycle(std::move(vsm.uboClearOmni));
    Resources::recycle(std::move(vsm.uboOmniPages));
    Resources::recycle(std::move(vsm.uboCullLights));
    Resources::recycle(std::move(vsm.uboPostprocessOmni));
    Resources::recycle(std::move(vsm.uboAlloc));
    }

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

  wview->visibilityPass(cmd, fId, 0);
  prepareSky(cmd,fId,*wview);

  drawHiZ (cmd,fId,*wview);
  buildHiZ(cmd,fId);

  wview->visibilityPass(cmd, fId, 1);
  drawGBuffer(cmd,fId,*wview);

  drawShadowMap(cmd,fId,*wview);
  drawVsm(cmd,fId,*wview);
  drawSwr(cmd,fId,*wview);

  prepareIrradiance(cmd,fId,*wview);
  prepareExposure(cmd,fId,*wview);
  prepareSSAO(cmd);
  prepareFog (cmd,fId,*wview);
  prepareGi  (cmd,fId,*wview);

  cmd.setFramebuffer({{sceneLinear, Tempest::Discard, Tempest::Preserve}}, {zbuffer, Tempest::Readonly});
  drawShadowResolve(cmd,fId,*wview);
  drawAmbient(cmd,*wview);
  drawLights(cmd,fId,*wview);
  drawSky(cmd,fId,*wview);

  stashSceneAux(cmd,fId);

  drawGWater(cmd,fId,*wview);

  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}}, {zbuffer, Tempest::Preserve, Tempest::Preserve});
  cmd.setDebugMarker("Sun&Moon");
  drawSunMoon(cmd, fId, *wview);
  cmd.setDebugMarker("Translucent");
  wview->drawTranslucent(cmd,fId);

  drawProbesDbg(cmd, fId, *wview);
  drawProbesHitDbg(cmd, fId);
  drawVsmDbg(cmd, fId);
  drawSwrDbg(cmd);

  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}});
  drawReflections(cmd,fId);
  if(camera->isInWater()) {
    cmd.setDebugMarker("Underwater");
    drawUnderwater(cmd,fId);
    } else {
    cmd.setDebugMarker("Fog");
    drawFog(cmd, fId, *wview);
    }

  if(settings.aaEnabled) {
    cmd.setDebugMarker("CMAA2 & Tonemapping");
    drawCMAA2(result, cmd);
    } else {
    cmd.setDebugMarker("Tonemapping");
    drawTonemapping(result, cmd);
    }

  wview->postFrameupdate();
  }

void Renderer::drawTonemapping(Attachment& result, Encoder<CommandBuffer>& cmd) {
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

  cmd.setFramebuffer({ {result, Tempest::Discard, Tempest::Preserve} });
  cmd.setUniforms(*tonemapping.pso, tonemapping.uboTone, &p, sizeof(p));
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawCMAA2(Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd) {
  const IVec3    inputGroupSize  = cmaa2.detectEdges2x2->workGroupSize();
  const IVec3    outputGroupSize = inputGroupSize - IVec3(2, 2, 0);
  const uint32_t groupCountX     = uint32_t((sceneLinear.w() + outputGroupSize.x * 2 - 1) / (outputGroupSize.x * 2));
  const uint32_t groupCountY     = uint32_t((sceneLinear.h() + outputGroupSize.y * 2 - 1) / (outputGroupSize.y * 2));

  cmd.setFramebuffer({});

  // detect edges
  cmd.setUniforms(*cmaa2.detectEdges2x2, cmaa2.detectEdges2x2Ubo);
  cmd.dispatch(groupCountX, groupCountY, 1);

  // process candidates pass
  cmd.setUniforms(*cmaa2.processCandidates, cmaa2.processCandidatesUbo);
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

  cmd.setFramebuffer({{result, Tempest::Discard, Tempest::Preserve}});
  cmd.setUniforms(*tonemapping.pso, tonemapping.uboTone, &p, sizeof(p));
  cmd.draw(Resources::fsqVbo());

  cmd.setUniforms(*cmaa2.defferedColorApply, cmaa2.defferedColorApplyUbo, &p, sizeof(p));
  cmd.drawIndirect(cmaa2.indirectBuffer, 3*sizeof(uint32_t));
  }

void Renderer::drawFog(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  auto& device  = Resources::device();
  auto& scene   = wview.sceneGlobals();
  auto& shaders = Shaders::inst();

  auto  smpB    = Sampler::bilinear();
  smpB.setClamping(ClampMode::ClampToEdge);

  switch(sky.quality) {
    case None:
    case VolumetricLQ: {
      if(sky.uboFog.isEmpty()) {
        sky.uboFog = device.descriptors(shaders.fog);
        sky.uboFog.set(0, sky.fogLut3D, smpB);
        sky.uboFog.set(1, sky.fogLut3D, smpB);
        sky.uboFog.set(2, zbuffer, Sampler::nearest()); // NOTE: wanna here depthFetch from gles2
        sky.uboFog.set(3, scene.uboGlobal[SceneGlobals::V_Main]);
        }
      cmd.setUniforms(shaders.fog, sky.uboFog);
      break;
      }
    case VolumetricHQ: {
      if(sky.uboFog3d.isEmpty()) {
        sky.uboFog3d = device.descriptors(shaders.fog3dHQ);
        sky.uboFog3d.set(0, sky.fogLut3D,   smpB);
        sky.uboFog3d.set(1, sky.fogLut3DMs, smpB);
        sky.uboFog3d.set(2, zbuffer,        Sampler::nearest());
        sky.uboFog3d.set(3, scene.uboGlobal[SceneGlobals::V_Main]);
        sky.uboFog3d.set(4, sky.occlusionLut);
        }
      cmd.setUniforms(shaders.fog3dHQ, sky.uboFog3d);
      break;
      }
    case Epipolar: {
      if(sky.uboFog3d.isEmpty()) {
        sky.uboFog3d = device.descriptors(shaders.vsmFog);
        //sky.uboFog3d.set(0, sky.fogLut3D,   smpB);
        sky.uboFog3d.set(0, scene.uboGlobal[SceneGlobals::V_Main]);
        sky.uboFog3d.set(1, zbuffer, Sampler::nearest());
        sky.uboFog3d.set(2, vsm.fogDbg, smpB);
        sky.uboFog3d.set(3, vsm.epipoles);
        sky.uboFog3d.set(4, sky.fogLut3DMs, smpB);
        }
      cmd.setUniforms(shaders.vsmFog, sky.uboFog3d);
      break;
      }
    case PathTrace:
      return;
    }
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawSunMoon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  drawSunMoon(cmd, fId, wview, false);
  drawSunMoon(cmd, fId, wview, true);
  }

void Renderer::drawSunMoon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& wview, bool isSun) {
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

  const float sunSize        = settings.sunSize;
  const float moonSize       = settings.moonSize;

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

  auto smpB = Sampler::bilinear();
  smpB.setClamping(ClampMode::ClampToEdge);

  cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, isSun ? wview.sky().sunImage() : wview.sky().moonImage());
  cmd.setBinding(2, sky.transLut, smpB);
  cmd.setUniforms(Shaders::inst().sun, &push, sizeof(push));
  cmd.draw(nullptr, 0, 6);
  }

void Renderer::stashSceneAux(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  auto& device = Resources::device();
  if(!device.properties().hasSamplerFormat(zBufferFormat))
    return;
  cmd.setFramebuffer({{sceneOpaque, Tempest::Discard, Tempest::Preserve}, {sceneDepth, Tempest::Discard, Tempest::Preserve}});
  cmd.setDebugMarker("Stash scene");
  cmd.setUniforms(Shaders::inst().stash, uboStash);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawVsmDbg(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  static bool enable = false;
  if(!enable || !settings.vsmEnabled)
    return;

  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}});
  cmd.setDebugMarker("VSM-dbg");
  cmd.setUniforms(*vsm.pagesDbgPso, vsm.uboDbg, &settings.vsmMipBias, sizeof(settings.vsmMipBias));
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawSwrDbg(Tempest::Encoder<Tempest::CommandBuffer>& cmd) {
  static bool enable = true;
  if(!enable || !settings.swrEnabled)
    return;

  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}});
  cmd.setDebugMarker("SWR-dbg");
  cmd.setUniforms(Shaders::inst().swRenderingDbg, swr.uboDbg);
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

void Renderer::drawHiZ(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& view) {
  cmd.setDebugMarker("HiZ-occluders");
  cmd.setFramebuffer({}, {zbuffer, 1.f, Tempest::Preserve});
  view.drawHiZ(cmd,fId);
  }

void Renderer::buildHiZ(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  cmd.setDebugMarker("HiZ-mip");

  assert(hiz.hiZ.w()<=128 && hiz.hiZ.h()<=128); // shader limitation
  cmd.setFramebuffer({});
  cmd.setUniforms(Shaders::inst().hiZPot, hiz.uboPot);
  cmd.dispatch(size_t(hiz.hiZ.w()), size_t(hiz.hiZ.h()));

  uint32_t w = uint32_t(hiz.hiZ.w()), h = uint32_t(hiz.hiZ.h()), mip = hiz.hiZ.mipCount();
  cmd.setUniforms(Shaders::inst().hiZMip, hiz.uboMip, &mip, sizeof(mip));
  cmd.dispatchThreads(w,h);
  }

void Renderer::drawVsm(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  if(!settings.vsmEnabled)
    return;

  static bool omniLights = true;
  const bool doVirtualFog = sky.quality!=VolumetricLQ && sky.quality!=PathTrace && settings.vsmEnabled;

  auto& device  = Resources::device();
  auto& shaders = Shaders::inst();
  auto& scene   = wview.sceneGlobals();

  const size_t numOmniPages = wview.lights().size()*6;

  if(omniLights && vsm.uboOmniPages.isEmpty()) {
    Resources::recycle(std::move(vsm.pageTblOmni));
    vsm.pageTblOmni   = device.ssbo(nullptr, shaders.vsmClearOmni.sizeofBuffer(0, wview.lights().size()*6));
    vsm.visibleLights = device.ssbo(nullptr, shaders.vsmClearOmni.sizeofBuffer(1, wview.lights().size()));

    vsm.uboOmniPages = device.descriptors(shaders.vsmMarkOmniPages);
    vsm.uboOmniPages.set(0, scene.uboGlobal[SceneGlobals::V_Main]);
    vsm.uboOmniPages.set(1, gbufDiffuse, Sampler::nearest());
    vsm.uboOmniPages.set(2, gbufNormal,  Sampler::nearest());
    vsm.uboOmniPages.set(3, zbuffer,     Sampler::nearest());
    vsm.uboOmniPages.set(4, wview.lights().lightsSsbo());
    vsm.uboOmniPages.set(5, vsm.visibleLights);
    vsm.uboOmniPages.set(6, vsm.pageTblOmni);
    vsm.uboOmniPages.set(7, vsm.vsmDbg);

    vsm.uboPostprocessOmni = device.descriptors(shaders.vsmPostprocessOmni);
    vsm.uboPostprocessOmni.set(0, vsm.pageTblOmni);

    vsm.uboClearOmni = device.descriptors(shaders.vsmClearOmni);
    vsm.uboClearOmni.set(0, vsm.pageTblOmni);
    vsm.uboClearOmni.set(1, vsm.visibleLights);

    vsm.uboCullLights = device.descriptors(shaders.vsmCullLights);
    vsm.uboCullLights.set(0, scene.uboGlobal[SceneGlobals::V_Main]);
    vsm.uboCullLights.set(1, wview.lights().lightsSsbo());
    vsm.uboCullLights.set(2, vsm.visibleLights);
    vsm.uboCullLights.set(3, *scene.hiZ);
    }

  if(vsm.uboAlloc.isEmpty()) {
    vsm.uboAlloc = device.descriptors(shaders.vsmListPages);
    vsm.uboAlloc.set(0, vsm.pageList);
    vsm.uboAlloc.set(1, vsm.pageTbl);
    vsm.uboAlloc.set(2, vsm.pageTblOmni);
    vsm.uboAlloc.set(3, vsm.pageListTmp);
    vsm.uboAlloc.set(4, scene.vsmDbg);
    }

  cmd.setFramebuffer({});
  cmd.setDebugMarker("VSM-pages");
  cmd.setUniforms(shaders.vsmClear, vsm.uboClear);
  cmd.dispatchThreads(size_t(vsm.pageTbl.w()), size_t(vsm.pageTbl.h()), size_t(vsm.pageTbl.d()));

  if(omniLights) {
    cmd.setUniforms(shaders.vsmClearOmni, vsm.uboClearOmni);
    cmd.dispatchThreads(numOmniPages);

    struct Push { float znear; uint32_t lightsTotal; } push = {};
    push.znear       = scene.znear;
    push.lightsTotal = uint32_t(wview.lights().size());
    cmd.setUniforms(shaders.vsmCullLights, vsm.uboCullLights, &push, sizeof(push));
    cmd.dispatchThreads(wview.lights().size());
    }

  cmd.setUniforms(shaders.vsmMarkPages, vsm.uboPages, &settings.vsmMipBias, sizeof(settings.vsmMipBias));
  cmd.dispatchThreads(zbuffer.size());

  if(omniLights) {
    struct Push { Vec3 originLwc; float znear; float vsmMipBias; } push = {};
    push.originLwc  = scene.originLwc;
    push.znear      = scene.znear;
    push.vsmMipBias = settings.vsmMipBias;
    cmd.setUniforms(shaders.vsmMarkOmniPages, vsm.uboOmniPages, &push, sizeof(push));
    cmd.dispatchThreads(zbuffer.size());

    const uint32_t lightsTotal = uint32_t(wview.lights().size());
    cmd.setUniforms(shaders.vsmPostprocessOmni, vsm.uboPostprocessOmni, &lightsTotal, sizeof(lightsTotal));
    cmd.dispatchThreads(wview.lights().size());
    }

  // sky&fog
  if(doVirtualFog) {
    if(vsm.uboEpipole.isEmpty()) {
      vsm.uboEpipole = device.descriptors(shaders.vsmFogEpipolar);
      vsm.uboEpipole.set(0, sky.occlusionLut);
      vsm.uboEpipole.set(1, vsm.epTrace);
      vsm.uboEpipole.set(2, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
      vsm.uboEpipole.set(3, vsm.epipoles);
      vsm.uboEpipole.set(4, zbuffer);
      vsm.uboEpipole.set(5, vsm.pageTbl);
      vsm.uboEpipole.set(6, vsm.pageData);
      }

    cmd.setDebugMarker("VSM-epipolar");
    cmd.setUniforms(shaders.vsmFogEpipolar, vsm.uboEpipole);
    cmd.dispatch(uint32_t(vsm.epTrace.h()));

    cmd.setUniforms(shaders.vsmFogPages, vsm.uboFogPages);
    cmd.dispatchThreads(vsm.epTrace.size());
    }

  cmd.setDebugMarker("VSM-pages-alloc");
  if(true) {
    // trimming
    // cmd.setUniforms(shaders.vsmTrimPages, vsm.uboClump);
    // cmd.dispatch(1);

    // clump
    cmd.setUniforms(shaders.vsmClumpPages, vsm.uboClump);
    cmd.dispatchThreads(size_t(vsm.pageTbl.w()), size_t(vsm.pageTbl.h()), size_t(vsm.pageTbl.d()));
    }

  // list
  cmd.setUniforms(shaders.vsmListPages, vsm.uboAlloc);
  if(omniLights)
    cmd.dispatch(size_t(vsm.pageTbl.d() + 1)); else
    cmd.dispatch(size_t(vsm.pageTbl.d()));

  // cmd.setUniforms(shaders.vsmSortPages, vsm.uboAlloc);
  // cmd.dispatch(1);

  // alloc
  // cmd.setUniforms(shaders.vsmAlloc2Pages, vsm.uboAlloc);
  // cmd.dispatchThreads(size_t(vsm.pageData.w()/VSM_PAGE_SIZE), size_t(vsm.pageData.h()/VSM_PAGE_SIZE));

  cmd.setUniforms(shaders.vsmAllocPages, vsm.uboAlloc);
  cmd.dispatch(1);

  // hor-merge
  cmd.setUniforms(shaders.vsmMergePages, vsm.uboAlloc);
  cmd.dispatch(1);

  cmd.setDebugMarker("VSM-visibility");
  wview.visibilityVsm(cmd,fId);

  cmd.setDebugMarker("VSM-rendering");
  cmd.setFramebuffer({}, {vsm.pageData, 0.f, Tempest::Preserve});
  wview.drawVsm(cmd,fId);
  }

void Renderer::drawSwr(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view) {
  if(!settings.swrEnabled)
    return;
  cmd.setFramebuffer({});
  cmd.setDebugMarker("SW-rendering");
  view.drawSwr(cmd,fId);
  }

void Renderer::drawGBuffer(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& view) {
  cmd.setDebugMarker("GBuffer");
  cmd.setFramebuffer({{gbufDiffuse, Tempest::Vec4(), Tempest::Preserve},
                      {gbufNormal,  Tempest::Vec4(), Tempest::Preserve}},
                     {zbuffer, Tempest::Preserve, Tempest::Preserve});
  view.drawGBuffer(cmd,fId);
  }

void Renderer::drawGWater(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& view) {
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
  view.drawWater(cmd,fId);
  }

void Renderer::drawReflections(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  cmd.setDebugMarker("Reflections");
  cmd.setUniforms(*water.reflectionsPso, water.ubo);
  if(Gothic::options().doMeshShading) {
    cmd.dispatchMeshThreads(gbufDiffuse.size());
    } else {
    cmd.draw(Resources::fsqVbo());
    }
  }

void Renderer::drawUnderwater(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  cmd.setUniforms(Shaders::inst().underwaterT, water.underUbo);
  cmd.draw(Resources::fsqVbo());

  cmd.setUniforms(Shaders::inst().underwaterS, water.underUbo);
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

void Renderer::drawShadowResolve(Encoder<CommandBuffer>& cmd, uint8_t fId, const WorldView& wview) {
  static bool light = true;
  if(!light)
    return;

  auto& scene = wview.sceneGlobals();
  cmd.setDebugMarker(settings.vsmEnabled ? "DirectSunLight-VSM" : "DirectSunLight");

  cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, gbufDiffuse, Sampler::nearest());
  cmd.setBinding(2, gbufNormal,  Sampler::nearest());
  cmd.setBinding(3, zbuffer,     Sampler::nearest());
  if(shadow.directLightPso==&Shaders::inst().vsmDirectLight) {
    cmd.setBinding(4, vsm.pageTbl);
    cmd.setBinding(5, vsm.pageList);
    cmd.setBinding(6, vsm.pageData);
    cmd.setBinding(8, scene.vsmDbg);
    }
  else if(shadow.directLightPso==&Shaders::inst().directLightRq) {
    for(size_t r=0; r<Resources::ShadowLayers; ++r) {
      if(shadowMap[r].isEmpty())
        continue;
      cmd.setBinding(4+r, shadowMap[r]);
      }
    cmd.setBinding(6, scene.rtScene.tlas);
    cmd.setBinding(7, Sampler::bilinear());
    cmd.setBinding(8, scene.rtScene.tex);
    cmd.setBinding(9, scene.rtScene.vbo);
    cmd.setBinding(10,scene.rtScene.ibo);
    cmd.setBinding(11,scene.rtScene.rtDesc);
    }
  else {
    for(size_t r=0; r<Resources::ShadowLayers; ++r) {
      if(shadowMap[r].isEmpty())
        continue;
      cmd.setBinding(4+r, shadowMap[r]);
      }
    }

  cmd.setUniforms(*shadow.directLightPso);
  if(settings.vsmEnabled)
    cmd.setUniforms(*shadow.directLightPso, &settings.vsmMipBias, sizeof(settings.vsmMipBias));
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawLights(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  static bool light = true;
  if(!light)
    return;

  auto& scene = wview.sceneGlobals();
  cmd.setDebugMarker("Point lights");

  cmd.setBinding(0, scene.uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, gbufDiffuse, Sampler::nearest());
  cmd.setBinding(2, gbufNormal,  Sampler::nearest());
  cmd.setBinding(3, zbuffer,     Sampler::nearest());
  cmd.setBinding(4, wview.lights().lightsSsbo());
  if(lights.directLightPso==&Shaders::inst().lightsVsm) {
    cmd.setBinding(5, vsm.pageTblOmni);
    cmd.setBinding(6, vsm.pageData);
    }
  if(lights.directLightPso==&Shaders::inst().lightsRq) {
    cmd.setBinding(6, scene.rtScene.tlas);
    cmd.setBinding(7, Sampler::bilinear());
    cmd.setBinding(8, scene.rtScene.tex);
    cmd.setBinding(9, scene.rtScene.vbo);
    cmd.setBinding(10,scene.rtScene.ibo);
    cmd.setBinding(11,scene.rtScene.rtDesc);
    }

  auto  originLwc = scene.originLwc;
  auto& ibo       = Resources::cubeIbo();
  cmd.setUniforms(*lights.directLightPso, &originLwc, sizeof(originLwc));
  cmd.draw(nullptr,ibo, 0,ibo.size(), 0,wview.lights().size());
  }

void Renderer::drawSky(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  auto& device  = Resources::device();
  auto& scene   = wview.sceneGlobals();
  auto& shaders = Shaders::inst();

  auto  smp    = Sampler::trillinear();
  auto  smpB   = Sampler::bilinear();
  smpB.setClamping(ClampMode::ClampToEdge);

  cmd.setDebugMarker("Sky");

  if(sky.quality==PathTrace) {
    if(sky.uboSkyPathtrace.isEmpty()) {
      sky.uboSkyPathtrace = device.descriptors(Shaders::inst().skyPathTrace);
      sky.uboSkyPathtrace.set(0, scene.uboGlobal[SceneGlobals::V_Main]);
      sky.uboSkyPathtrace.set(1, sky.transLut,     smpB);
      sky.uboSkyPathtrace.set(2, sky.multiScatLut, smpB);
      sky.uboSkyPathtrace.set(3, sky.cloudsLut,    smpB);
      sky.uboSkyPathtrace.set(4, zbuffer, Sampler::nearest());
      sky.uboSkyPathtrace.set(5, shadowMap[1], Resources::shadowSampler());
      }
    cmd.setUniforms(shaders.skyPathTrace, sky.uboSkyPathtrace);
    cmd.draw(Resources::fsqVbo());
    return;
    }

  auto& skyShader = sky.quality==VolumetricLQ ? shaders.sky : shaders.skySep;
  if(sky.uboSky.isEmpty()) {
    sky.uboSky = device.descriptors(skyShader);
    sky.uboSky.set(0, scene.uboGlobal[SceneGlobals::V_Main]);
    sky.uboSky.set(1, sky.transLut,     smpB);
    sky.uboSky.set(2, sky.multiScatLut, smpB);
    sky.uboSky.set(3, sky.viewLut,      smpB);
    sky.uboSky.set(4, sky.fogLut3D);
    if(sky.quality!=VolumetricLQ)
      sky.uboSky.set(5, sky.fogLut3DMs);
    sky.uboSky.set(6,*wview.sky().cloudsDay()  .lay[0],smp);
    sky.uboSky.set(7,*wview.sky().cloudsDay()  .lay[1],smp);
    sky.uboSky.set(8,*wview.sky().cloudsNight().lay[0],smp);
    sky.uboSky.set(9,*wview.sky().cloudsNight().lay[1],smp);
    }
  cmd.setUniforms(skyShader, sky.uboSky);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::prepareSSAO(Encoder<CommandBuffer>& cmd) {
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
  cmd.setUniforms(*ssao.ssaoPso, ssao.uboSsao, &push, sizeof(push));
  cmd.dispatchThreads(ssao.ssaoBuf.size());

  cmd.setUniforms(Shaders::inst().ssaoBlur, ssao.uboBlur);
  cmd.dispatchThreads(ssao.ssaoBuf.size());
  }

void Renderer::prepareFog(Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  auto& device  = Resources::device();
  auto& scene   = wview.sceneGlobals();
  auto& shaders = Shaders::inst();

  //auto  smp    = Sampler::trillinear();
  auto  smpB   = Sampler::bilinear();
  smpB.setClamping(ClampMode::ClampToEdge);

  cmd.setDebugMarker("Fog LUTs");

  if(sky.quality!=PathTrace) {
    auto& shader = sky.quality==VolumetricLQ ? shaders.fogViewLut3d : shaders.fogViewLutSep;
    if(sky.uboFogViewLut3d.isEmpty()) {
      sky.uboFogViewLut3d = device.descriptors(shader);
      sky.uboFogViewLut3d.set(0, scene.uboGlobal[SceneGlobals::V_Main]);
      sky.uboFogViewLut3d.set(1, sky.transLut,     smpB);
      sky.uboFogViewLut3d.set(2, sky.multiScatLut, smpB);
      sky.uboFogViewLut3d.set(3, sky.cloudsLut,    smpB);
      sky.uboFogViewLut3d.set(4, sky.fogLut3D);
      if(sky.quality==VolumetricHQ || sky.quality==Epipolar)
        sky.uboFogViewLut3d.set(5, sky.fogLut3DMs);
      }
    cmd.setFramebuffer({});
    cmd.setUniforms(shader, sky.uboFogViewLut3d);
    cmd.dispatchThreads(uint32_t(sky.fogLut3D.w()), uint32_t(sky.fogLut3D.h()));
    }

  if(settings.vsmEnabled && (sky.quality==VolumetricHQ || sky.quality==Epipolar)) {
    cmd.setFramebuffer({});
    cmd.setDebugMarker("VSM-epipolar-fog");
    cmd.setUniforms(shaders.vsmFogShadow, vsm.uboFogShadow);
    cmd.dispatchThreads(vsm.epTrace.size());
    }

  switch(sky.quality) {
    case None:
    case VolumetricLQ:
      break;
    case VolumetricHQ: {
      if(settings.vsmEnabled) {
        if(vsm.uboFogSample.isEmpty()) {
          vsm.uboFogSample = device.descriptors(shaders.vsmFogSample);
          vsm.uboFogSample.set(0, sky.occlusionLut);
          vsm.uboFogSample.set(1, vsm.epTrace);
          vsm.uboFogSample.set(2, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
          vsm.uboFogSample.set(3, vsm.epipoles);
          vsm.uboFogSample.set(4, zbuffer);
          }
        cmd.setUniforms(shaders.vsmFogSample, vsm.uboFogSample);
        cmd.dispatchThreads(zbuffer.size());
        } else {
        if(sky.uboOcclusion.isEmpty()) {
          sky.uboOcclusion = device.descriptors(shaders.fogOcclusion);
          sky.uboOcclusion.set(2, zbuffer, Sampler::nearest());
          sky.uboOcclusion.set(3, scene.uboGlobal[SceneGlobals::V_Main]);
          sky.uboOcclusion.set(4, sky.occlusionLut);
          sky.uboOcclusion.set(5, shadowMap[1], Resources::shadowSampler());
          }
        cmd.setFramebuffer({});
        cmd.setUniforms(shaders.fogOcclusion, sky.uboOcclusion);
        cmd.dispatchThreads(sky.occlusionLut.size());
        }
      break;
      }
    case Epipolar:{
      // experimental
      if(vsm.uboFogTrace.isEmpty()) {
        auto smpB = Sampler::bilinear();
        smpB.setClamping(ClampMode::ClampToEdge);
        vsm.uboFogTrace = device.descriptors(shaders.vsmFogTrace);
        vsm.uboFogTrace.set(0, vsm.fogDbg);
        vsm.uboFogTrace.set(1, vsm.epTrace);
        vsm.uboFogTrace.set(2, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
        vsm.uboFogTrace.set(3, vsm.epipoles);
        vsm.uboFogTrace.set(4, zbuffer);
        vsm.uboFogTrace.set(5, sky.transLut,   smpB);
        vsm.uboFogTrace.set(6, sky.cloudsLut,  smpB);
        vsm.uboFogTrace.set(7, sky.fogLut3DMs, smpB);
        }
      cmd.setFramebuffer({});
      cmd.setDebugMarker("VSM-trace");
      cmd.setUniforms(shaders.vsmFogTrace, vsm.uboFogTrace);
      cmd.dispatchThreads(vsm.epTrace.size());
      break;
      }
    case PathTrace: {
      break;
      }
    }
  }

void Renderer::prepareIrradiance(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  auto& device  = Resources::device();
  auto& shaders = Shaders::inst();
  auto& scene   = wview.sceneGlobals();

  cmd.setDebugMarker("Irradiance");
  // wview.prepareIrradiance(cmd, fId);

  if(sky.uboIrradiance.isEmpty()) {
    sky.uboIrradiance = device.descriptors(shaders.irradiance);
    sky.uboIrradiance.set(0, sky.irradianceLut);
    sky.uboIrradiance.set(1, scene.uboGlobal[SceneGlobals::V_Main]);
    sky.uboIrradiance.set(2, sky.viewCldLut);
    }
  cmd.setFramebuffer({});
  cmd.setUniforms(shaders.irradiance, sky.uboIrradiance);
  cmd.dispatch(1);
  }

void Renderer::prepareGi(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  if(!settings.giEnabled || !settings.zCloudShadowScale) {
    return;
    }

  const size_t maxHash = gi.hashTable.byteSize()/sizeof(uint32_t);

  auto& shaders = Shaders::inst();
  auto& scene   = wview.sceneGlobals();

  cmd.setFramebuffer({});
  if(gi.fisrtFrame) {
    cmd.setDebugMarker("GI-Init");
    cmd.setBinding(0, gi.voteTable);
    cmd.setBinding(1, gi.hashTable);
    cmd.setBinding(2, gi.probes);
    cmd.setBinding(3, gi.freeList);
    cmd.setUniforms(shaders.probeInit);
    cmd.dispatch(1);
    cmd.setUniforms(shaders.probeClearHash);
    cmd.dispatchThreads(maxHash);

    cmd.setBinding(0, gi.probesLighting);
    cmd.setBinding(1, Resources::fallbackBlack());
    cmd.setUniforms(shaders.copyImg);
    cmd.dispatchThreads(gi.probesLighting.size());
    gi.fisrtFrame = false;
    }

  static bool alloc = true;
  cmd.setDebugMarker("GI-Alloc");
  cmd.setBinding(0, gi.voteTable);
  cmd.setBinding(1, gi.hashTable);
  cmd.setBinding(2, gi.probes);
  cmd.setBinding(3, gi.freeList);
  cmd.setUniforms(shaders.probeClear);
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
    cmd.setUniforms(shaders.probeVote);
    cmd.dispatchThreads(sceneDepth.size());

    cmd.setBinding(0, gi.voteTable);
    cmd.setBinding(1, gi.hashTable);
    cmd.setBinding(2, gi.probes);
    cmd.setBinding(3, gi.freeList);
    cmd.setUniforms(shaders.probePrune);
    cmd.dispatchThreads(gi.maxProbes);

    cmd.setBinding(0, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    cmd.setBinding(1, gbufDiffuse, Sampler::nearest());
    cmd.setBinding(2, gbufNormal,  Sampler::nearest());
    cmd.setBinding(3, zbuffer,     Sampler::nearest());
    cmd.setBinding(4, gi.voteTable);
    cmd.setBinding(5, gi.hashTable);
    cmd.setBinding(6, gi.probes);
    cmd.setBinding(7, gi.freeList);
    cmd.setUniforms(shaders.probeAlocation);
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
  cmd.setUniforms(shaders.probeTrace);
  cmd.dispatch(1024); // TODO: dispath indirect?

  cmd.setDebugMarker("GI-HashMap");
  cmd.setBinding(0, gi.voteTable);
  cmd.setBinding(1, gi.hashTable);
  cmd.setBinding(2, gi.probes);
  cmd.setBinding(3, gi.freeList);
  cmd.setUniforms(shaders.probeClearHash);
  cmd.dispatchThreads(maxHash);
  cmd.setUniforms(shaders.probeMakeHash);
  cmd.dispatchThreads(gi.maxProbes);

  cmd.setDebugMarker("GI-Lighting");
  cmd.setBinding(0, gi.probesLightingPrev);
  cmd.setBinding(1, gi.probesLighting);
  cmd.setUniforms(shaders.copyImg);
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
  cmd.setUniforms(shaders.probeLighting);
  cmd.dispatch(1024);
  }

void Renderer::prepareExposure(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  auto& device  = Resources::device();
  auto& shaders = Shaders::inst();
  auto& scene   = wview.sceneGlobals();

  auto  smpB   = Sampler::bilinear();
  smpB.setClamping(ClampMode::ClampToEdge);

  cmd.setDebugMarker("Exposure");
  //wview.prepareExposure(cmd,fId);

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

  if(sky.uboExp.isEmpty()) {
    sky.uboExp = device.descriptors(shaders.skyExposure);
    sky.uboExp.set(0, scene.uboGlobal[SceneGlobals::V_Main]);
    sky.uboExp.set(1, sky.viewCldLut);
    sky.uboExp.set(2, sky.transLut,  smpB);
    sky.uboExp.set(3, sky.cloudsLut, smpB);
    sky.uboExp.set(4, sky.irradianceLut);
    }

  cmd.setFramebuffer({});
  cmd.setUniforms(shaders.skyExposure, sky.uboExp, &push, sizeof(push));
  cmd.dispatch(1);
  }

void Renderer::drawProbesDbg(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  if(!settings.giEnabled)
    return;

  static bool enable = false;
  if(!enable)
    return;

  auto& pso = Shaders::inst().probeDbg;
  cmd.setDebugMarker("GI-dbg");
  cmd.setBinding(0, wview.sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  cmd.setBinding(1, gi.probesLighting);
  cmd.setBinding(2, gi.probes);
  cmd.setBinding(3, gi.hashTable);
  cmd.setUniforms(pso);
  cmd.draw(nullptr, 0, 36, 0, gi.maxProbes);
  }

void Renderer::drawProbesHitDbg(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  if(!settings.giEnabled)
    return;

  static bool enable = false;
  if(!enable)
    return;

  auto& pso = Shaders::inst().probeHitDbg;
  cmd.setDebugMarker("GI-dbg");
  cmd.setBinding(1, gi.probesLighting);
  cmd.setBinding(2, gi.probesGBuffDiff);
  cmd.setBinding(3, gi.probesGBuffRayT);
  cmd.setBinding(4, gi.probes);
  cmd.setUniforms(pso);
  cmd.draw(nullptr, 0, 36, 0, gi.maxProbes*256);
  //cmd.draw(nullptr, 0, 36, 0, 1024);
  }

void Renderer::drawAmbient(Encoder<CommandBuffer>& cmd, const WorldView& view) {
  static bool enable = true;
  if(!enable)
    return;

  auto& shaders = Shaders::inst();
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
    cmd.setUniforms(shaders.probeAmbient);
    cmd.draw(Resources::fsqVbo());
    return;
    }

  cmd.setDebugMarker("AmbientLight");
  cmd.setUniforms(*ssao.ambientLightPso,ssao.uboCompose);
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

  auto pm  = device.readPixels(textureCast<const Texture2d&>(normals));
  pm.save("gbufNormal.png");

  pm  = device.readPixels(textureCast<const Texture2d&>(d16));
  pm.save("zbuffer.hdr");

  return img;
  }

Size Renderer::internalResolution() const {
  if(settings.vidResIndex==0)
     return Size(int(swapchain.w()), int(swapchain.h()));
  if(settings.vidResIndex==1)
    return Size(int(3*swapchain.w()/4), int(3*swapchain.h()/4));
  return Size(int(swapchain.w()/2), int(swapchain.h()/2));
  }

