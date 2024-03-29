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

  path    = computeShader("path.comp.sprv");

  stash   = postEffect("stash");

  clusterInit      = computeShader("cluster_init.comp.sprv");
  clusterPath      = computeShader("cluster_patch.comp.sprv");
  clusterTask      = computeShader("cluster_task.comp.sprv");
  clusterTaskHiZ   = computeShader("cluster_task_hiz.comp.sprv");
  clusterTaskHiZCr = computeShader("cluster_task_hiz_cr.comp.sprv");

  ssao             = computeShader("ssao.comp.sprv");
  ssaoBlur         = computeShader("ssao_blur.comp.sprv");

  directLight      = postEffect("direct_light", "direct_light",    RenderState::ZTestMode::NoEqual);
  directLightSh    = postEffect("direct_light", "direct_light_sh", RenderState::ZTestMode::NoEqual);
  if(Gothic::options().doRayQuery && Resources::device().properties().descriptors.nonUniformIndexing)
    directLightRq  = postEffect("direct_light", "direct_light_rq", RenderState::ZTestMode::NoEqual);

  ambientLight     = ambientLightShader("ambient_light");
  ambientLightSsao = ambientLightShader("ambient_light_ssao");

  irradiance         = computeShader("irradiance.comp.sprv");
  cloudsLut          = computeShader("clouds_lut.comp.sprv");
  skyTransmittance   = postEffect("sky", "sky_transmittance");
  skyMultiScattering = postEffect("sky", "sky_multi_scattering");
  skyViewLut         = postEffect("sky", "sky_view_lut");
  skyViewCldLut      = postEffect("sky", "sky_view_clouds_lut");

  fogViewLut3dLQ     = computeShader("fog_view_lut_lq.comp.sprv");
  fogViewLut3dHQ     = computeShader("fog_view_lut_hq.comp.sprv");
  shadowDownsample   = computeShader("shadow_downsample.comp.sprv");
  fogOcclusion       = computeShader("fog3d.comp.sprv");

  skyExposure        = computeShader("sky_exposure.comp.sprv");

  sky                = postEffect("sky");
  fog                = fogShader ("fog");
  fog3dLQ            = fogShader ("fog3d_lq");
  fog3dHQ            = fogShader ("fog3d_hq");

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
  }

  tonemapping = postEffect("tonemapping", "tonemapping", RenderState::ZTestMode::Always);

  const auto fxaaZTestMode = RenderState::ZTestMode::Always;
  fxaaPresets[uint32_t(FxaaPreset::OFF)]        = Tempest::RenderPipeline();
  fxaaPresets[uint32_t(FxaaPreset::CONSOLE)]    = postEffect("fxaa", "fxaa_quality_0", fxaaZTestMode);
  fxaaPresets[uint32_t(FxaaPreset::PC_LOW)]     = postEffect("fxaa", "fxaa_quality_1", fxaaZTestMode);
  fxaaPresets[uint32_t(FxaaPreset::PC_MEDIUM)]  = postEffect("fxaa", "fxaa_quality_2", fxaaZTestMode);
  fxaaPresets[uint32_t(FxaaPreset::PC_HIGH)]    = postEffect("fxaa", "fxaa_quality_3", fxaaZTestMode);
  fxaaPresets[uint32_t(FxaaPreset::PC_EXTREME)] = postEffect("fxaa", "fxaa_quality_4", fxaaZTestMode);

  hiZPot    = computeShader("hiz_pot.comp.sprv");
  if(device.properties().hasAtomicFormat(TextureFormat::R32U))
    hiZMip = computeShader("hiz_mip_img.comp.sprv"); else
    hiZMip = computeShader("hiz_mip.comp.sprv");

  if(meshlets && device.properties().meshlets.maxGroupSize.x>=256) {
    RenderState state;
    state.setCullFaceMode(RenderState::CullMode::Front);
    state.setZTestMode   (RenderState::ZTestMode::Greater);

    auto sh = GothicShader::get("hiz_reproject.mesh.sprv");
    auto ms = device.shader(sh.data,sh.len);
    sh = GothicShader::get("hiz_reproject.frag.sprv");
    auto fs = device.shader(sh.data,sh.len);
    hiZReproj = device.pipeline(state,Shader(),ms,fs);
    }

  if(Gothic::options().doRayQuery) {
    RenderState state;
    state.setCullFaceMode(RenderState::CullMode::NoCull);
    state.setZTestMode   (RenderState::ZTestMode::Less);

    auto sh = GothicShader::get("probe_dbg.vert.sprv");
    auto vs = device.shader(sh.data,sh.len);
    sh = GothicShader::get("probe_dbg.frag.sprv");
    auto fs = device.shader(sh.data,sh.len);
    probeDbg = device.pipeline(Triangles,state,vs,fs);

    probeInit       = computeShader("probe_init.comp.sprv");
    probeClear      = computeShader("probe_clear.comp.sprv");
    probeClearHash  = computeShader("probe_clear_hash.comp.sprv");
    probeMakeHash   = computeShader("probe_make_hash.comp.sprv");
    probeTiles      = computeShader("probe_tiles.comp.sprv");
    probeVoteTile   = computeShader("probe_vote_tile.comp.sprv");
    probeVote       = computeShader("probe_vote.comp.sprv");
    probePrune      = computeShader("probe_prune.comp.sprv");
    probeAlocation  = computeShader("probe_allocation.comp.sprv");
    probeTrace      = computeShader("probe_trace.comp.sprv");
    probeLighting   = computeShader("probe_lighting.comp.sprv");

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

  if(pt==PipelineType::T_Shadow) {
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
  const char* typeFsM = nullptr;
  const char* typeFsD = nullptr;

  switch(alpha) {
    case Material::Solid:
      typeVsM = "";
      typeFsM = "_g";
      typeVsD = "_d";
      typeFsD = "_d";
      if(trivial)
        typeFsM = "_g_s";
      break;
    case Material::AlphaTest:
      typeVsM = "";
      typeFsM = "_g_at";
      typeVsD = "_d_at";
      typeFsD = "_d_at";
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
  return postEffect(name,name);
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

  auto sh = GothicShader::get("sky.vert.sprv");
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

  auto sh = GothicShader::get("underwater.vert.sprv");
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

  auto sh = GothicShader::get("water_reflection.vert.sprv");
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

  auto sh = GothicShader::get("ambient_light.vert.sprv");
  auto vs = device.shader(sh.data,sh.len);
  sh      = GothicShader::get(string_frm(name,".frag.sprv"));
  auto fs = device.shader(sh.data,sh.len);

  return device.pipeline(Triangles, state, vs, fs);
  }
