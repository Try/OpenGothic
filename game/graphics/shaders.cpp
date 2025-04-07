#include "shaders.h"

#include <Tempest/Device>
#include <Tempest/Log>

#include "gothic.h"
#include "resources.h"

#include "shader.h"
#include "utils/string_frm.h"

using namespace Tempest;

//static constexpr uint32_t defaultWg = 64;

Shaders* Shaders::instance = nullptr;

Shaders::Shaders() {
  instance = this;
  auto& device = Resources::device();

  const bool meshlets = Gothic::options().doMeshShading;

  copyBuf = computeShader("copy.comp.sprv");
  copyImg = computeShader("copy_img.comp.sprv");
  copy    = postEffect("copy");

  patch   = computeShader("patch.comp.sprv");

  stash   = postEffect("stash");

  clusterInit         = computeShader("cluster_init.comp.sprv");
  clusterPatch        = computeShader("cluster_patch.comp.sprv");
  visibilityPassSh    = computeShader("visibility_pass.comp.sprv");
  visibilityPassHiZ   = computeShader("visibility_pass_hiz.comp.sprv");
  visibilityPassHiZCr = computeShader("visibility_pass_hiz_cr.comp.sprv");

  ssao                = computeShader("ssao.comp.sprv");
  ssaoBlur            = computeShader("ssao_blur.comp.sprv");

  directLight      = postEffect("direct_light",    RenderState::ZTestMode::NoEqual);
  directLightSh    = postEffect("direct_light_sh", RenderState::ZTestMode::NoEqual);
  if(Gothic::options().doRayQuery && device.properties().descriptors.nonUniformIndexing)
    directLightRq  = postEffect("direct_light_rq", RenderState::ZTestMode::NoEqual);

  ambientLight       = ambientLightShader("ambient_light");
  ambientLightSsao   = ambientLightShader("ambient_light_ssao");

  irradiance         = computeShader("irradiance.comp.sprv");
  cloudsLut          = computeShader("clouds_lut.comp.sprv");
  skyTransmittance   = postEffect("sky_transmittance");
  skyMultiScattering = postEffect("sky_multi_scattering");
  skyViewLut         = postEffect("sky_view_lut");
  skyViewCldLut      = postEffect("sky_view_clouds_lut");

  fogViewLut3d       = computeShader("fog_view_lut.comp.sprv");
  fogViewLutSep      = computeShader("fog_view_lut_sep.comp.sprv");
  fogOcclusion       = computeShader("fog3d.comp.sprv");

  skyExposure        = computeShader("sky_exposure.comp.sprv");
  sky                = postEffect("sky");
  skySep             = postEffect("sky_sep");
  fog                = fogShader ("fog");
  fog3dHQ            = fogShader ("fog3d_hq");

  {
    RenderState state;
    state.setCullFaceMode (RenderState::CullMode::Front);
    state.setBlendSource  (RenderState::BlendMode::One);
    state.setBlendDest    (RenderState::BlendMode::SrcAlpha);
    state.setZTestMode    (RenderState::ZTestMode::Always);
    state.setZWriteEnabled(false);

    auto sh      = GothicShader::get("copy.vert.sprv");
    auto vsLight = device.shader(sh.data,sh.len);
    sh           = GothicShader::get("sky_pathtrace.frag.sprv");
    auto fsLight = device.shader(sh.data,sh.len);
    skyPathTrace = device.pipeline(Triangles, state, vsLight, fsLight);
  }

  underwaterT        = inWaterShader("underwater_t", false);
  underwaterS        = inWaterShader("underwater_s", true);
  waterReflection    = reflectionShader("water_reflection.frag.sprv",meshlets);
  waterReflectionSSR = reflectionShader("water_reflection_ssr.frag.sprv",meshlets);

  {
  RenderState state;
  state.setCullFaceMode (RenderState::CullMode::Front);
  state.setBlendSource  (RenderState::BlendMode::One);
  state.setBlendDest    (RenderState::BlendMode::SrcAlpha);
  state.setZTestMode    (RenderState::ZTestMode::Equal);
  state.setZWriteEnabled(false);

  auto sh      = GothicShader::get("sun.vert.sprv");
  auto vsLight = device.shader(sh.data,sh.len);
  sh           = GothicShader::get("sun.frag.sprv");
  auto fsLight = device.shader(sh.data,sh.len);
  sun          = device.pipeline(Triangles, state, vsLight, fsLight);
  }

  {
  RenderState state;
  state.setCullFaceMode (RenderState::CullMode::Front);
  state.setBlendSource  (RenderState::BlendMode::One);
  state.setBlendDest    (RenderState::BlendMode::One);
  state.setZTestMode    (RenderState::ZTestMode::Less);

  state.setZWriteEnabled(false);

  auto sh      = GothicShader::get("light.vert.sprv");
  auto vsLight = device.shader(sh.data,sh.len);
  sh           = GothicShader::get("light.frag.sprv");
  auto fsLight = device.shader(sh.data,sh.len);
  lights       = device.pipeline(Triangles, state, vsLight, fsLight);
  if(Gothic::options().doRayQuery) {
    if(Resources::device().properties().descriptors.nonUniformIndexing) {
      sh      = GothicShader::get("light_rq_at.frag.sprv");
      fsLight = device.shader(sh.data,sh.len);
      } else {
      sh      = GothicShader::get("light_rq.frag.sprv");
      fsLight = device.shader(sh.data,sh.len);
      }
    lightsRq = device.pipeline(Triangles, state, vsLight, fsLight);
    }
  if(Shaders::isVsmSupported()) {
    sh      = GothicShader::get("light_vsm.frag.sprv");
    fsLight = device.shader(sh.data,sh.len);
    lightsVsm = device.pipeline(Triangles, state, vsLight, fsLight);
    }
  }

  tonemapping        = postEffect("copy_uv", "tonemapping",    RenderState::ZTestMode::Always);
  tonemappingUpscale = postEffect("copy_uv", "tonemapping_up", RenderState::ZTestMode::Always);

  cmaa2EdgeColor2x2Presets[uint32_t(AaPreset::OFF)]    = Tempest::ComputePipeline();
  cmaa2EdgeColor2x2Presets[uint32_t(AaPreset::MEDIUM)] = computeShader("cmaa2_edges_color2x2_quality_0.comp.sprv");
  cmaa2EdgeColor2x2Presets[uint32_t(AaPreset::ULTRA)]  = computeShader("cmaa2_edges_color2x2_quality_1.comp.sprv");

  cmaa2ProcessCandidates = computeShader("cmaa2_process_candidates.comp.sprv");
  {
    auto sh = GothicShader::get("cmaa2_deferred_color_apply_2x2.vert.sprv");
    auto vs = device.shader(sh.data,sh.len);
    sh = GothicShader::get("cmaa2_deferred_color_apply_2x2.frag.sprv");
    auto fs = device.shader(sh.data,sh.len);
    cmaa2DeferredColorApply2x2 = device.pipeline(Tempest::Points,RenderState(),vs,fs);
  }

  hiZPot = computeShader("hiz_pot.comp.sprv");
  hiZMip = computeShader("hiz_mip.comp.sprv");

  if(Gothic::options().doRayQuery) {
    RenderState state;
    state.setCullFaceMode(RenderState::CullMode::NoCull);
    state.setZTestMode   (RenderState::ZTestMode::Less);

    auto sh = GothicShader::get("probe_dbg.vert.sprv");
    auto vs = device.shader(sh.data,sh.len);
    sh = GothicShader::get("probe_dbg.frag.sprv");
    auto fs = device.shader(sh.data,sh.len);
    probeDbg = device.pipeline(Triangles,state,vs,fs);

    sh = GothicShader::get("probe_hit_dbg.vert.sprv");
    vs = device.shader(sh.data,sh.len);
    sh = GothicShader::get("probe_hit_dbg.frag.sprv");
    fs = device.shader(sh.data,sh.len);
    probeHitDbg = device.pipeline(Triangles,state,vs,fs);

    probeInit      = computeShader("probe_init.comp.sprv");
    probeClear     = computeShader("probe_clear.comp.sprv");
    probeClearHash = computeShader("probe_clear_hash.comp.sprv");
    probeMakeHash  = computeShader("probe_make_hash.comp.sprv");
    probeVote      = computeShader("probe_vote.comp.sprv");
    probePrune     = computeShader("probe_prune.comp.sprv");
    probeAlocation = computeShader("probe_allocation.comp.sprv");
    probeTrace     = computeShader("probe_trace.comp.sprv");
    probeLighting  = computeShader("probe_lighting.comp.sprv");

    state.setBlendSource  (RenderState::BlendMode::One);
    state.setBlendDest    (RenderState::BlendMode::SrcAlpha);  // for debugging
    state.setZTestMode    (RenderState::ZTestMode::NoEqual);
    state.setZWriteEnabled(false);
    sh = GothicShader::get("probe_ambient.vert.sprv");
    vs = device.shader(sh.data,sh.len);
    sh = GothicShader::get("probe_ambient.frag.sprv");
    fs = device.shader(sh.data,sh.len);
    probeAmbient = device.pipeline(Triangles,state,vs,fs);
    }

  if(Shaders::isVsmSupported()) {
    vsmVisibilityPass  = computeShader("vsm_visibility_pass.comp.sprv");
    vsmClear           = computeShader("vsm_clear.comp.sprv");
    vsmClearOmni       = computeShader("vsm_clear_omni.comp.sprv");
    vsmCullLights      = computeShader("vsm_cull_lights.comp.sprv");
    vsmMarkPages       = computeShader("vsm_mark_pages.comp.sprv");
    vsmMarkOmniPages   = computeShader("vsm_mark_omni_pages.comp.sprv");
    vsmPostprocessOmni = computeShader("vsm_postprocess_omni.comp.sprv");
    vsmTrimPages       = computeShader("vsm_trim_pages.comp.sprv");
    vsmClumpPages      = computeShader("vsm_clump_pages.comp.sprv");
    vsmListPages       = computeShader("vsm_list_pages.comp.sprv");
    vsmSortPages       = computeShader("vsm_sort_pages.comp.sprv");
    vsmAllocPages      = computeShader("vsm_alloc_pages.comp.sprv");
    vsmAlloc2Pages     = computeShader("vsm_alloc_pages2.comp.sprv");
    vsmMergePages      = computeShader("vsm_merge_pages.comp.sprv");
    vsmPackDraw0       = computeShader("vsm_pack_draws0.comp.sprv");
    vsmPackDraw1       = computeShader("vsm_pack_draws1.comp.sprv");
    vsmFogEpipolar     = computeShader("vsm_fog_epipolar.comp.sprv");
    vsmFogPages        = computeShader("vsm_fog_mark_pages.comp.sprv");
    vsmFogShadow       = computeShader("vsm_fog_shadow.comp.sprv");
    vsmFogSample       = computeShader("vsm_fog_sample.comp.sprv");
    vsmFogTrace        = computeShader("vsm_fog_trace.comp.sprv");
    vsmFog             = fogShader("fog_epipolar");

    vsmDirectLight     = postEffect("direct_light_vsm", RenderState::ZTestMode::NoEqual);
    vsmDbg             = postEffect("vsm_dbg", RenderState::ZTestMode::Always);
    vsmRendering       = computeShader("vsm_rendering.comp.sprv");
    }

  if(Shaders::isRtsmSupported()) {
    rtsmClear       = computeShader("rtsm_clear.comp.sprv");
    rtsmPages       = computeShader("rtsm_mark_pages.comp.sprv");
    rtsmHiZ         = computeShader("rtsm_hiz_pages.comp.sprv");
    rtsmCulling     = computeShader("rtsm_culling.comp.sprv");
    rtsmCullLights  = computeShader("rtsm_cull_lights.comp.sprv");
    rtsmPosition    = computeShader("rtsm_position.comp.sprv");

    rtsmMeshletCull    = computeShader("rtsm_meshlet_cull.comp.sprv");
    rtsmMeshletComplex = computeShader("rtsm_meshlet_complex.comp.sprv");
    rtsmSampleCull     = computeShader("rtsm_sample_cull.comp.sprv");
    rtsmPrimCull       = computeShader("rtsm_primitive_cull.comp.sprv");
    rtsmPrimCull2      = computeShader("rtsm_primitive_cull2.comp.sprv");

    rtsmRaster      = computeShader("rtsm_raster.comp.sprv");
    rtsmDirectLight = postEffect("rtsm_direct_light", RenderState::ZTestMode::NoEqual);

    rtsmRendering   = computeShader("rtsm_rendering.comp.sprv");
    rtsmDbg         = postEffect("rtsm_dbg", RenderState::ZTestMode::Always);
    }

  if(Gothic::options().swRenderingPreset>0) {
    switch(Gothic::options().swRenderingPreset) {
      case 1:
        swRendering = computeShader("sw_rendering_imm.comp.sprv");
        break;
      case 2:
        swRendering = computeShader("sw_rendering_tbr.comp.sprv");
        break;
      case 3:
        swRendering = computeShader("sw_light.comp.sprv");
        break;
      }
    swRenderingDbg = postEffect("vbuffer_blit", RenderState::ZTestMode::Always);
    }

  {
    RenderState state;
    state.setCullFaceMode(RenderState::CullMode::Front);
    state.setZTestMode   (RenderState::ZTestMode::LEqual);

    auto sh = GothicShader::get("item.vert.sprv");
    auto vs = device.shader(sh.data,sh.len);
    sh = GothicShader::get("item.frag.sprv");
    auto fs = device.shader(sh.data,sh.len);
    inventory = device.pipeline(Triangles,state,vs,fs);
  }
  }

Shaders::~Shaders() {
  instance = nullptr;
  }

Shaders& Shaders::inst() {
  return *instance;
  }

bool Shaders::isVsmSupported() {
  auto& gpu = Resources::device().properties();
  if(gpu.compute.maxInvocations>=1024 && gpu.render.maxClipCullDistances>=4 &&
     gpu.render.maxViewportSize.w>=8192 && gpu.render.maxViewportSize.h>=8192) {
    return true;
    }
  return false;
  }

bool Shaders::isRtsmSupported() {
  if(!Gothic::options().doBindless) {
    return false;
    }
  auto& gpu = Resources::device().properties();
  if(gpu.compute.maxInvocations>=1024 && gpu.descriptors.nonUniformIndexing) {
    return true;
    }
  return false;
  }

const RenderPipeline* Shaders::materialPipeline(const Material& mat, DrawCommands::Type t, PipelineType pt, bool bl) const {
  if(t==DrawCommands::Static) {
    // same shader
    t = DrawCommands::Movable;
    }

  if(pt!=PipelineType::T_Main && !mat.isSolid()) {
    return nullptr;
    }
  if(pt!=PipelineType::T_Main && mat.alpha==Material::Water) {
    return nullptr;
    }

  const auto alpha   = (mat.isGhost ? Material::Ghost : mat.alpha);
  const bool trivial = (!mat.hasUvAnimation() && alpha==Material::Solid && t==DrawCommands::Landscape);

  for(auto& i:materials) {
    if(i.alpha==alpha && i.type==t && i.pipelineType==pt && i.bindless==bl && i.trivial==trivial)
      return &i.pipeline;
    }

  RenderState state;
  state.setCullFaceMode(RenderState::CullMode::Front);
  state.setZTestMode   (RenderState::ZTestMode::LEqual);
  //state.setZTestMode   (RenderState::ZTestMode::Less);

  if(pt==PipelineType::T_Shadow || pt==PipelineType::T_Vsm) {
    state.setZTestMode(RenderState::ZTestMode::Greater); //FIXME
    }
  if(pt==PipelineType::T_Vsm) {
    // state.setZTestMode(RenderState::ZTestMode::Always); //FIXME
    state.setZTestMode(RenderState::ZTestMode::Greater); //FIXME
    }

  switch(alpha) {
    case Material::Solid:
    case Material::AlphaTest:
    case Material::Water:
    case Material::Ghost:
      break;
    case Material::Transparent:
      state.setBlendSource  (RenderState::BlendMode::SrcAlpha); // premultiply in shader
      state.setBlendDest    (RenderState::BlendMode::OneMinusSrcAlpha);
      state.setZWriteEnabled(false);
      break;
    case Material::AdditiveLight:
      state.setBlendSource  (RenderState::BlendMode::SrcAlpha);
      state.setBlendDest    (RenderState::BlendMode::One);
      state.setZWriteEnabled(false);
      break;
    case Material::Multiply:
    case Material::Multiply2:
      state.setBlendSource  (RenderState::BlendMode::DstColor);
      state.setBlendDest    (RenderState::BlendMode::SrcColor);
      state.setZWriteEnabled(false);
      break;
    }

  static bool overdrawDbg = false;
  if(overdrawDbg &&
      (alpha==Material::Solid || alpha==Material::AlphaTest) && t!=DrawCommands::Landscape && pt!=T_Shadow) {
    state.setBlendSource(RenderState::BlendMode::One);
    state.setBlendDest  (RenderState::BlendMode::One);
    state.setZWriteEnabled(false);
    state.setZTestMode(RenderState::ZTestMode::Always);
    }

  const char* vsTok = "";
  const char* fsTok = "";

  switch(t) {
    case DrawCommands::Landscape:
      vsTok = "lnd";
      fsTok = "lnd";
      break;
    case DrawCommands::Static:
    case DrawCommands::Movable:
      vsTok = "obj";
      fsTok = "obj";
      break;
    case DrawCommands::Animated:
      vsTok = "ani";
      fsTok = "obj";
      break;
    case DrawCommands::Pfx:
      vsTok = "pfx";
      fsTok = "pfx";
      break;
    case DrawCommands::Morph:
      vsTok = "mph";
      fsTok = "obj";
      break;
    }

  const char* typeVsM = nullptr;
  const char* typeVsD = nullptr;
  const char* typeVsV = nullptr;
  const char* typeFsM = nullptr;
  const char* typeFsD = nullptr;
  const char* typeFsV = nullptr;

  switch(alpha) {
    case Material::Solid:
      typeVsM = "";
      typeFsM = "_g";
      typeVsD = "_d";
      typeFsD = "_d";
      typeVsV = "_v";
      typeFsV = "_v";
      if(trivial)
        typeFsM = "_g_s";
      break;
    case Material::AlphaTest:
      typeVsM = "";
      typeFsM = "_g_at";
      typeVsD = "_d_at";
      typeFsD = "_d_at";
      typeVsV = "_v_at";
      typeFsV = "_v_at";
      break;
    case Material::Transparent:
      typeVsM = "_f";
      typeFsM = "_f";
      break;
    case Material::Multiply:
    case Material::Multiply2:
      typeVsM = "";
      typeFsM = "_e";
      break;
    case Material::AdditiveLight:
      typeVsM = "";
      typeFsM = "_e";
      break;
    case Material::Water:
      typeVsM = "_f";
      typeFsM = "_w";
      break;
    case Material::Ghost:
      typeVsM = "";
      typeFsM = "_x";
      break;
    }

  const char* typeVs = "";
  const char* typeFs = "";
  switch(pt) {
    case T_Shadow:
    case T_Depth:
      typeVs = typeVsD;
      typeFs = typeFsD;
      break;
    case T_Vsm:
      typeVs = typeVsV;
      typeFs = typeFsV;
      break;
    case T_Main:
      typeVs = typeVsM;
      typeFs = typeFsM;
      break;
    }

  if(typeVs==nullptr || typeFs==nullptr)
    return nullptr;

  const char* bindless = bl ? "_bindless" : "_slot";

  materials.emplace_front();
  auto& b = materials.front();
  b.alpha        = alpha;
  b.type         = t;
  b.pipelineType = pt;
  b.bindless     = bl;
  b.trivial      = trivial;

  auto& device = Resources::device();
  if(mat.isTesselated() && device.properties().tesselationShader && t==DrawCommands::Landscape && true) {
    auto shVs = GothicShader::get(string_frm("main_", vsTok, typeVs, bindless, ".vert.sprv"));
    auto shTc = GothicShader::get(string_frm("main_", vsTok, typeVs, bindless, ".tesc.sprv"));
    auto shTe = GothicShader::get(string_frm("main_", vsTok, typeVs, bindless, ".tese.sprv"));
    auto shFs = GothicShader::get(string_frm("main_", fsTok, typeFs, bindless, ".frag.sprv"));

    auto vs = device.shader(shVs.data,shVs.len);
    auto tc = device.shader(shTc.data,shTc.len);
    auto te = device.shader(shTe.data,shTe.len);
    auto fs = device.shader(shFs.data,shFs.len);
    b.pipeline = device.pipeline(Triangles, state, vs, tc, te, fs);
    }
  else if(Gothic::options().doMeshShading && t!=DrawCommands::Pfx) {
    auto shMs = GothicShader::get(string_frm("main_", vsTok, typeVs, bindless, ".mesh.sprv"));
    auto shFs = GothicShader::get(string_frm("main_", fsTok, typeFs, bindless, ".frag.sprv"));

    auto ms = device.shader(shMs.data,shMs.len);
    auto fs = device.shader(shFs.data,shFs.len);
    b.pipeline = device.pipeline(state, Shader(), ms, fs);
    }
  else if(t!=DrawCommands::Pfx) {
    auto shVs = GothicShader::get(string_frm("main_", vsTok, typeVs, bindless, ".vert.sprv"));
    auto shFs = GothicShader::get(string_frm("main_", fsTok, typeFs, bindless, ".frag.sprv"));

    auto vs = device.shader(shVs.data,shVs.len);
    auto fs = device.shader(shFs.data,shFs.len);
    b.pipeline = device.pipeline(Triangles, state, vs, fs);
    }
  else {
    auto shVs = GothicShader::get(string_frm("main_", vsTok, typeVs, ".vert.sprv"));
    auto shFs = GothicShader::get(string_frm("main_", fsTok, typeFs, ".frag.sprv"));

    auto vs = device.shader(shVs.data,shVs.len);
    auto fs = device.shader(shFs.data,shFs.len);
    b.pipeline = device.pipeline(Triangles, state, vs, fs);
    }

  return &b.pipeline;
  }

RenderPipeline Shaders::postEffect(std::string_view name) {
  return postEffect("copy",name);
  }

RenderPipeline Shaders::postEffect(std::string_view name, Tempest::RenderState::ZTestMode ztest) {
  return postEffect("copy",name,ztest);
  }

RenderPipeline Shaders::postEffect(std::string_view vsName, std::string_view fsName, Tempest::RenderState::ZTestMode ztest) {
  auto& device = Resources::device();

  RenderState stateFsq;
  stateFsq.setCullFaceMode (RenderState::CullMode::Front);
  stateFsq.setZTestMode    (ztest);
  stateFsq.setZWriteEnabled(false);

  auto sh = GothicShader::get(string_frm(vsName,".vert.sprv"));
  auto vs = device.shader(sh.data,sh.len);

  sh      = GothicShader::get(string_frm(fsName,".frag.sprv"));
  auto fs = device.shader(sh.data,sh.len);
  return device.pipeline(Triangles,stateFsq,vs,fs);
  }

ComputePipeline Shaders::computeShader(std::string_view name) {
  auto& device = Resources::device();
  auto  sh     = GothicShader::get(name);
  return device.pipeline(device.shader(sh.data,sh.len));
  }

RenderPipeline Shaders::fogShader(std::string_view name) {
  auto& device = Resources::device();
  const bool fogDbg = false;

  RenderState state;
  state.setZWriteEnabled(false);
  state.setCullFaceMode (RenderState::CullMode::Front);
  state.setBlendSource  (RenderState::BlendMode::One);
  if(!fogDbg) {
    state.setBlendDest(RenderState::BlendMode::OneMinusSrcAlpha);
    }

  auto sh = GothicShader::get("copy.vert.sprv");
  auto vs = device.shader(sh.data,sh.len);

  sh      = GothicShader::get(string_frm(name,".frag.sprv"));
  auto fs = device.shader(sh.data,sh.len);
  return device.pipeline(Triangles,state,vs,fs);
  }

RenderPipeline Shaders::inWaterShader(std::string_view name, bool isScattering) {
  auto& device = Resources::device();

  RenderState state;
  state.setZWriteEnabled(false);
  state.setCullFaceMode(RenderState::CullMode::Front);

  if(isScattering) {
    state.setBlendSource(RenderState::BlendMode::One);
    state.setBlendDest  (RenderState::BlendMode::One);
    } else {
    state.setBlendSource(RenderState::BlendMode::Zero);
    state.setBlendDest  (RenderState::BlendMode::SrcColor);
    }

  auto sh = GothicShader::get("copy.vert.sprv");
  auto vs = device.shader(sh.data,sh.len);

  sh      = GothicShader::get(string_frm(name,".frag.sprv"));
  auto fs = device.shader(sh.data,sh.len);
  return device.pipeline(Triangles,state,vs,fs);
  }

RenderPipeline Shaders::reflectionShader(std::string_view name, bool hasMeshlets) {
  auto& device = Resources::device();

  RenderState state;
  state.setCullFaceMode (RenderState::CullMode::Front);
  state.setZTestMode    (RenderState::ZTestMode::LEqual);
  state.setZWriteEnabled(false);
  state.setBlendSource  (RenderState::BlendMode::One);
  state.setBlendDest    (RenderState::BlendMode::One);

  auto sh = GothicShader::get("copy.vert.sprv");
  auto vs = device.shader(sh.data,sh.len);
  sh      = GothicShader::get(name);
  auto fs = device.shader(sh.data,sh.len);

  if(hasMeshlets) {
    sh = GothicShader::get("water_reflection.mesh.sprv");
    vs = device.shader(sh.data,sh.len);
    }

  return device.pipeline(Triangles, state, vs, fs);
  }

RenderPipeline Shaders::ambientLightShader(std::string_view name) {
  auto& device = Resources::device();

  RenderState state;
  state.setCullFaceMode (RenderState::CullMode::Front);
  state.setBlendSource  (RenderState::BlendMode::One);
  state.setBlendDest    (RenderState::BlendMode::SrcAlpha); // debug
  state.setZTestMode    (RenderState::ZTestMode::NoEqual);
  state.setZWriteEnabled(false);

  auto sh = GothicShader::get("copy.vert.sprv");
  auto vs = device.shader(sh.data,sh.len);
  sh      = GothicShader::get(string_frm(name,".frag.sprv"));
  auto fs = device.shader(sh.data,sh.len);

  return device.pipeline(Triangles, state, vs, fs);
  }
