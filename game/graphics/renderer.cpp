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

  settings.giEnabled  = Gothic::options().doRtGi;
  settings.vsmEnabled = Gothic::options().doVirtualShadow;
  settings.swrEnabled = Gothic::options().swRenderingPreset>0;
  initSettings();
  }

Renderer::~Renderer() {
  Gothic::inst().onSettingsChanged.ubind(this,&Renderer::initSettings);
  }

void Renderer::resetSwapchain() {
  auto& device = Resources::device();
  device.waitIdle();

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
  hiz.uboPot    = device.descriptors(Shaders::inst().hiZPot);
  hiz.uboPot.set(0, zbuffer, smpN);
  hiz.uboPot.set(1, hiz.hiZ);

  hiz.uboMip = device.descriptors(Shaders::inst().hiZMip);
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
    hiz.smProj    = device.zbuffer(shadowFormat, smSize, smSize);
    hiz.uboReproj = device.descriptors(Shaders::inst().hiZReproj);
    hiz.uboReproj.set(0, zbuffer, smpN);
    // hiz.uboReproj.set(1, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);

    hiz.hiZSm1    = device.image2d(TextureFormat::R16, 64, 64, true);
    hiz.uboPotSm1 = device.descriptors(Shaders::inst().hiZPot);
    hiz.uboPotSm1.set(0, hiz.smProj, smpN);
    hiz.uboPotSm1.set(1, hiz.hiZSm1);
    hiz.uboMipSm1 = Tempest::DescriptorSet();
    }

  if(smSize>0) {
    for(int i=0; i<Resources::ShadowLayers; ++i) {
      if(settings.vsmEnabled)
        continue;
      if(settings.vsmEnabled && !settings.giEnabled)
        ;//continue; //TODO: support vsm in gi code
      shadowMap[i] = device.zbuffer(shadowFormat,smSize,smSize);
      }
    }

  sceneOpaque = device.attachment(TextureFormat::R11G11B10UF,w,h);
  sceneDepth  = device.attachment(TextureFormat::R32F,       w,h);

  gbufDiffuse = device.attachment(TextureFormat::RGBA8,w,h);
  gbufNormal  = device.attachment(TextureFormat::R32U, w,h);

  uboStash = device.descriptors(Shaders::inst().stash);
  uboStash.set(0,sceneLinear,Sampler::nearest());
  uboStash.set(1,zbuffer,    Sampler::nearest());

  if(Gothic::options().doRayQuery && Resources::device().properties().descriptors.nonUniformIndexing &&
     settings.shadowResolution>0)
    shadow.directLightPso = &Shaders::inst().directLightRq;
  else if(settings.shadowResolution>0)
    shadow.directLightPso = &Shaders::inst().directLightSh;
  else
    shadow.directLightPso = &Shaders::inst().directLight;
  shadow.ubo = device.descriptors(*shadow.directLightPso);

  water.underUbo = device.descriptors(Shaders::inst().underwaterT);

  ssao.ssaoBuf = device.image2d(ssao.aoFormat, w,h);
  ssao.ssaoPso = &Shaders::inst().ssao;
  ssao.uboSsao = device.descriptors(*ssao.ssaoPso);

  ssao.ssaoBlur = device.image2d(ssao.aoFormat, w,h);
  ssao.uboBlur  = device.descriptors(Shaders::inst().ssaoBlur);

  tonemapping.pso             = (settings.vidResIndex==0) ? &Shaders::inst().tonemapping : &Shaders::inst().tonemappingUpscale;
  tonemapping.uboTone         = device.descriptors(*tonemapping.pso);

  cmaa2.detectEdges2x2        = &Shaders::inst().cmaa2EdgeColor2x2Presets[Gothic::options().aaPreset];
  cmaa2.detectEdges2x2Ubo     = device.descriptors(*cmaa2.detectEdges2x2);

  cmaa2.processCandidates     = &Shaders::inst().cmaa2ProcessCandidates;
  cmaa2.processCandidatesUbo  = device.descriptors(*cmaa2.processCandidates);

  cmaa2.defferedColorApply    = &Shaders::inst().cmaa2DeferredColorApply2x2;
  cmaa2.defferedColorApplyUbo = device.descriptors(*cmaa2.defferedColorApply);

  if(settings.vsmEnabled) {
    vsm.uboClear        = device.descriptors(Shaders::inst().vsmClear);
    if(!vsm.pageDataCs.isEmpty())
      vsm.uboClearPages = device.descriptors(Shaders::inst().vsmClearPages);

    vsm.uboPages        = device.descriptors(Shaders::inst().vsmMarkPages);
    vsm.uboEpipole      = device.descriptors(Shaders::inst().vsmFogEpipolar);
    vsm.uboFogShadow    = device.descriptors(Shaders::inst().vsmFogShadow);
    vsm.uboClump        = device.descriptors(Shaders::inst().vsmClumpPages);
    vsm.uboAlloc        = device.descriptors(Shaders::inst().vsmAllocPages);
    vsm.uboReproj       = device.descriptors(Shaders::inst().vsmReprojectSm);

    vsm.directLightPso  = &Shaders::inst().vsmDirectLight;
    vsm.pagesDbgPso     = &Shaders::inst().vsmDbg;
    vsm.uboLight        = device.descriptors(*vsm.directLightPso);

    vsm.pageTbl         = device.image3d(TextureFormat::R32U, 32, 32, 16);
    vsm.pageHiZ         = device.image3d(TextureFormat::R32U, 32, 32, 16);
    vsm.pageData        = device.zbuffer(shadowFormat, 8192, 8192);
    // vsm.pageDataCs      = device.image2d(TextureFormat::R32U, 4096, 4096);

    // vsm.ssTrace  = device.image2d(TextureFormat::RGBA8, w, h);
    vsm.ssTrace  = device.image2d(TextureFormat::R32U, w, h);
    vsm.epTrace  = device.image2d(TextureFormat::R16, 512, 2*1024);
    vsm.epipoles = device.ssbo(nullptr, uint32_t(vsm.epTrace.h())*6*sizeof(float));

    const int32_t VSM_PAGE_SIZE = 128;
    auto pageCount      = uint32_t(vsm.pageData.w()/VSM_PAGE_SIZE) * uint32_t(vsm.pageData.h()/VSM_PAGE_SIZE);
    auto pageSsboSize   = Shaders::inst().vsmClear.sizeOfBuffer(0, pageCount);

    vsm.pageList        = device.ssbo(nullptr, pageSsboSize);
    }

  if(settings.swrEnabled) {
    //swr.outputImage = device.image2d(Tempest::RGBA8, w, h);
    swr.outputImage = device.image2d(Tempest::R32U, w, h);
    swr.uboDbg      = device.descriptors(Shaders::inst().swRenderingDbg);
    }

  initGiData();
  prepareUniforms();
  prepareRtUniforms();
  }

void Renderer::initSettings() {
  settings.zEnvMappingEnabled = Gothic::inst().settingsGetI("ENGINE","zEnvMappingEnabled")!=0;
  settings.zCloudShadowScale  = Gothic::inst().settingsGetI("ENGINE","zCloudShadowScale") !=0;

  settings.zVidBrightness     = Gothic::inst().settingsGetF("VIDEO","zVidBrightness");
  settings.zVidContrast       = Gothic::inst().settingsGetF("VIDEO","zVidContrast");
  settings.zVidGamma          = Gothic::inst().settingsGetF("VIDEO","zVidGamma");

  auto prevVidResIndex = settings.vidResIndex;
  settings.vidResIndex = Gothic::inst().settingsGetF("INTERNAL","vidResIndex");
  settings.aaEnabled = (Gothic::options().aaPreset>0) && (settings.vidResIndex==0);

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
    auto& device = Resources::device();
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
  initGiData();
  prepareUniforms();
  prepareRtUniforms();
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

  if(!hiz.uboReproj.isEmpty()) {
    hiz.uboReproj.set(1, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    }

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
  ssao.uboCompose.set(3, wview->sky().irradiance());
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

  shadow.ubo.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
  shadow.ubo.set(1, gbufDiffuse, Sampler::nearest());
  shadow.ubo.set(2, gbufNormal,  Sampler::nearest());
  shadow.ubo.set(3, zbuffer,     Sampler::nearest());

  for(size_t r=0; r<Resources::ShadowLayers; ++r) {
    if(shadowMap[r].isEmpty())
      continue;
    shadow.ubo.set(4+r, shadowMap[r]);
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
    water.ubo.set(6, wview->sky().skyLut());
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

    gi.uboZeroIrr.set(0, gi.probesLighting);
    gi.uboZeroIrr.set(1, Resources::fallbackBlack());

    gi.uboPrevIrr.set(0, gi.probesLightingPrev);
    gi.uboPrevIrr.set(1, gi.probesLighting);

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

    gi.uboCompose.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    gi.uboCompose.set(1, gi.probesLighting);
    gi.uboCompose.set(2, gbufDiffuse,   Sampler::nearest());
    gi.uboCompose.set(3, gbufNormal,    Sampler::nearest());
    gi.uboCompose.set(4, zbuffer,       Sampler::nearest());
    gi.uboCompose.set(5, ssao.ssaoBlur, Sampler::nearest());
    gi.uboCompose.set(6, gi.hashTable);
    gi.uboCompose.set(7, gi.probes);
    }

  if(settings.vsmEnabled) {
    vsm.uboClear.set(0, vsm.pageList);
    vsm.uboClear.set(1, vsm.pageTbl);
    vsm.uboClear.set(2, vsm.pageHiZ);

    if(!vsm.uboClearPages.isEmpty())
      vsm.uboClearPages.set(0, vsm.pageDataCs);

    vsm.uboPages.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    vsm.uboPages.set(1, gbufDiffuse, Sampler::nearest());
    vsm.uboPages.set(2, gbufNormal,  Sampler::nearest());
    vsm.uboPages.set(3, zbuffer,     Sampler::nearest());
    vsm.uboPages.set(4, vsm.pageTbl);
    vsm.uboPages.set(5, vsm.pageHiZ);
    //vsm.uboPages.set(7, vsm.pageList);

    vsm.uboEpipole.set(0, vsm.ssTrace);
    vsm.uboEpipole.set(1, vsm.epTrace);
    vsm.uboEpipole.set(2, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    vsm.uboEpipole.set(3, vsm.epipoles);
    vsm.uboEpipole.set(4, zbuffer);
    vsm.uboEpipole.set(5, vsm.pageTbl);
    vsm.uboEpipole.set(6, vsm.pageData);

    vsm.uboFogShadow.set(0, vsm.ssTrace);
    vsm.uboFogShadow.set(1, vsm.epTrace);
    vsm.uboFogShadow.set(2, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    vsm.uboFogShadow.set(3, vsm.epipoles);
    vsm.uboFogShadow.set(4, zbuffer);

    vsm.uboClump.set(0, vsm.pageList);
    vsm.uboClump.set(1, vsm.pageTbl);

    vsm.uboAlloc.set(0, vsm.pageList);
    vsm.uboAlloc.set(1, vsm.pageTbl);
    vsm.uboAlloc.set(2, wview->sceneGlobals().vsmDbg);

    vsm.uboLight.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
    vsm.uboLight.set(1, gbufDiffuse, Sampler::nearest());
    vsm.uboLight.set(2, gbufNormal,  Sampler::nearest());
    vsm.uboLight.set(3, zbuffer,     Sampler::nearest());
    vsm.uboLight.set(4, vsm.pageTbl);
    vsm.uboLight.set(5, vsm.pageList);
    if(!vsm.pageDataCs.isEmpty())
      vsm.uboLight.set(6, vsm.pageDataCs); else
      vsm.uboLight.set(6, vsm.pageData);
    vsm.uboLight.set(8, wview->sceneGlobals().vsmDbg);
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
  wview->setVirtualShadowMap(vsm.pageData, vsm.pageDataCs, vsm.pageTbl, vsm.pageHiZ, vsm.pageList);
  wview->setVsmSkyShadows(vsm.ssTrace);
  wview->setSwRenderingImage(swr.outputImage);

  wview->setHiZ(textureCast<const Texture2d&>(hiz.hiZ));
  wview->setGbuffer(textureCast<const Texture2d&>(gbufDiffuse), textureCast<const Texture2d&>(gbufNormal));
  wview->setSceneImages(textureCast<const Texture2d&>(sceneOpaque), textureCast<const Texture2d&>(sceneDepth), zbuffer);
  wview->prepareUniforms();
  }

void Renderer::prepareRtUniforms() {
  auto wview = Gothic::inst().worldView();
  if(wview==nullptr)
    return;
  auto& scene = wview->sceneGlobals();
  if(scene.rtScene.tlas.isEmpty())
    return;

  if(shadow.directLightPso==&Shaders::inst().directLightRq) {
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

  if(wview->updateRtScene())
    prepareRtUniforms();

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
    wview->drawFog(cmd,fId);
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
  cmd.setUniforms(*vsm.pagesDbgPso, vsm.uboLight, &settings.vsmMipBias, sizeof(settings.vsmMipBias));
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

  auto& device = Resources::device();

  gi.uboDbg          = DescriptorSet();
  gi.probeInitPso    = &Shaders::inst().probeInit;

  gi.probeClearPso   = &Shaders::inst().probeClear;
  gi.probeClearHPso  = &Shaders::inst().probeClearHash;
  gi.probeMakeHPso   = &Shaders::inst().probeMakeHash;
  gi.uboClear        = device.descriptors(*gi.probeClearPso);

  gi.probeVotePso    = &Shaders::inst().probeVote;
  gi.probeAllocPso   = &Shaders::inst().probeAlocation;
  gi.probePrunePso   = &Shaders::inst().probePrune;
  gi.uboProbes       = device.descriptors(*gi.probeAllocPso);

  gi.uboZeroIrr      = device.descriptors(Shaders::inst().copyImg);
  gi.uboPrevIrr      = device.descriptors(Shaders::inst().copyImg);

  gi.probeTracePso   = &Shaders::inst().probeTrace;
  gi.uboTrace        = device.descriptors(*gi.probeTracePso);

  gi.probeLightPso   = &Shaders::inst().probeLighting;
  gi.uboLight        = device.descriptors(*gi.probeLightPso);

  gi.ambientLightPso = &Shaders::inst().probeAmbient;
  gi.uboCompose      = device.descriptors(*gi.ambientLightPso);

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

  auto& shaders = Shaders::inst();
  cmd.setFramebuffer({});
  cmd.setDebugMarker("VSM-pages");
  cmd.setUniforms(shaders.vsmClear, vsm.uboClear);
  cmd.dispatchThreads(size_t(vsm.pageTbl.w()), size_t(vsm.pageTbl.h()), size_t(vsm.pageTbl.d()));

  cmd.setUniforms(shaders.vsmMarkPages, vsm.uboPages, &settings.vsmMipBias, sizeof(settings.vsmMipBias));
  cmd.dispatchThreads(zbuffer.size());

  // sky&fog
  if(true && wview.sky().isVolumetric()) {
    cmd.setUniforms(shaders.vsmMarkFogPages, vsm.uboPages);
    cmd.dispatchThreads(zbuffer.size());
    // wview.vsmMarkSkyPages(cmd, fId);
    }

  if(vsm.pageDataCs.isEmpty()) {
    // trimming
    // cmd.setUniforms(shaders.vsmTrimPages, vsm.uboClump);
    // cmd.dispatch(1);

    // clump
    cmd.setUniforms(shaders.vsmClumpPages, vsm.uboClump);
    cmd.dispatchThreads(size_t(vsm.pageTbl.w()), size_t(vsm.pageTbl.h()), size_t(vsm.pageTbl.d()));
    }

  if(!vsm.pageDataCs.isEmpty()) {
    cmd.setUniforms(shaders.vsmClearPages, vsm.uboClearPages);
    cmd.dispatchThreads(size_t(vsm.pageDataCs.w()), size_t(vsm.pageDataCs.h()));
    }

  // list?
  cmd.setUniforms(shaders.vsmListPages, vsm.uboAlloc);
  cmd.dispatch(size_t(vsm.pageTbl.d()));

  // alloc
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

  if(true) {
    cmd.setFramebuffer({});
    cmd.setDebugMarker("VSM-epipolar");
    cmd.setUniforms(shaders.vsmFogEpipolar, vsm.uboEpipole);
    cmd.dispatch(uint32_t(vsm.epTrace.h()));

    cmd.setDebugMarker("VSM-epipolar-fog");
    cmd.setUniforms(shaders.vsmFogShadow, vsm.uboFogShadow);
    cmd.dispatchThreads(zbuffer.size());
    }

  if(false) {
    cmd.setDebugMarker("VSM-reproject");
    cmd.setFramebuffer({}, {shadowMap[1], 0.f, Tempest::Preserve});
    auto viewShadowLwcInv = shadowMatrix[1];
    viewShadowLwcInv.inverse();
    cmd.setUniforms(shaders.vsmReprojectSm, vsm.uboReproj, &viewShadowLwcInv, sizeof(viewShadowLwcInv));
    cmd.draw(Resources::fsqVbo());
    }
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

void Renderer::drawShadowResolve(Encoder<CommandBuffer>& cmd, uint8_t fId, const WorldView& view) {
  static bool useDsm = true;
  if(!useDsm)
    return;
  if(settings.vsmEnabled) {
    cmd.setDebugMarker("DirectSunLight-VSM");
    cmd.setUniforms(*vsm.directLightPso, vsm.uboLight, &settings.vsmMipBias, sizeof(settings.vsmMipBias));
    cmd.draw(Resources::fsqVbo());
    return;
    }
  cmd.setDebugMarker("DirectSunLight");
  cmd.setUniforms(*shadow.directLightPso, shadow.ubo);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawLights(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  cmd.setDebugMarker("Point lights");
  wview.drawLights(cmd,fId);
  }

void Renderer::drawSky(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  cmd.setDebugMarker("Sky");
  wview.drawSky(cmd,fId);
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
  cmd.setDebugMarker("Fog LUTs");
  wview.prepareFog(cmd,fId);
  }

void Renderer::prepareIrradiance(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  cmd.setDebugMarker("Irradiance");
  wview.prepareIrradiance(cmd, fId);
  }

void Renderer::prepareGi(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  if(!settings.giEnabled || !settings.zCloudShadowScale) {
    return;
    }

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

void Renderer::prepareExposure(Encoder<CommandBuffer>& cmd, uint8_t fId, WorldView& wview) {
  cmd.setDebugMarker("Exposure");
  wview.prepareExposure(cmd,fId);
  }

void Renderer::drawProbesDbg(Encoder<CommandBuffer>& cmd, uint8_t fId) {
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
  cmd.draw(nullptr, 0, 36, 0, gi.maxProbes);
  }

void Renderer::drawProbesHitDbg(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  if(!settings.giEnabled)
    return;

  static bool enable = false;
  if(!enable)
    return;

  auto& device = Resources::device();
  auto& pso    = Shaders::inst().probeHitDbg;

  if(auto wview=Gothic::inst().worldView()) {
    if(gi.uboDbg.isEmpty()) {
      gi.uboDbg = device.descriptors(pso);
      gi.uboDbg.set(0, wview->sceneGlobals().uboGlobal[SceneGlobals::V_Main]);
      gi.uboDbg.set(1, gi.probesLighting);
      gi.uboDbg.set(2, gi.probesGBuffDiff);
      gi.uboDbg.set(3, gi.probesGBuffRayT);
      gi.uboDbg.set(4, gi.probes);
      }
    }

  cmd.setDebugMarker("GI-dbg");
  cmd.setUniforms(pso, gi.uboDbg);
  cmd.draw(nullptr, 0, 36, 0, gi.maxProbes*256);
  //cmd.draw(nullptr, 0, 36, 0, 1024);
  }

void Renderer::drawAmbient(Encoder<CommandBuffer>& cmd, const WorldView& view) {
  static bool enable = true;
  if(!enable)
    return;

  if(settings.giEnabled && settings.zCloudShadowScale) {
    cmd.setDebugMarker("AmbientLight");
    cmd.setUniforms(*gi.ambientLightPso, gi.uboCompose);
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
