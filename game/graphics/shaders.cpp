#include "shaders.h"

#include <Tempest/Device>

#include "gothic.h"
#include "resources.h"

#include "shader.h"
#include "utils/string_frm.h"

using namespace Tempest;

static constexpr uint32_t defaultWg = 64;

Shaders* Shaders::instance = nullptr;

void Shaders::ShaderSet::load(Device &device, std::string_view tag, bool hasTesselation, bool hasMeshlets) {
  auto sh = GothicShader::get(string_frm(tag,'.',"frag",".sprv"));
  fs = device.shader(sh.data,sh.len);

  sh = GothicShader::get(string_frm(tag,'.',"vert",".sprv"));
  vs = device.shader(sh.data,sh.len);

  if(hasTesselation) {
    auto sh = GothicShader::get(string_frm(tag,'.',"tesc",".sprv"));
    tc = device.shader(sh.data,sh.len);

    sh = GothicShader::get(string_frm(tag,'.',"tese",".sprv"));
    te = device.shader(sh.data,sh.len);
    }

  if(hasMeshlets) {
    sh = GothicShader::get(string_frm(tag,'.',defaultWg,".mesh",".sprv"));
    me = device.shader(sh.data,sh.len);

    sh = GothicShader::get(string_frm(tag,'.',defaultWg,".task",".sprv"));
    ts = device.shader(sh.data,sh.len);
    }
  }

void Shaders::MaterialTemplate::load(Device &device, std::string_view tag, bool hasTesselation, bool hasMeshlets) {
  string_frm flnd = "lnd";
  string_frm fobj = "obj";
  string_frm fani = "ani";
  string_frm fmph = "mph";
  string_frm fpfx = "pfx";
  if(!tag.empty()) {
    flnd = string_frm("lnd_",tag);
    fobj = string_frm("obj_",tag);
    fani = string_frm("ani_",tag);
    fmph = string_frm("mph_",tag);
    fpfx = string_frm("pfx_",tag);
    }
  lnd.load(device,flnd,hasTesselation,hasMeshlets);
  obj.load(device,fobj,hasTesselation,hasMeshlets);
  ani.load(device,fani,hasTesselation,hasMeshlets);
  mph.load(device,fmph,hasTesselation,hasMeshlets);
  pfx.load(device,fpfx,hasTesselation,false);
  }

Shaders::Shaders() {
  instance = this;
  auto& device = Resources::device();

  const bool meshlets = Gothic::inst().doMeshShading();

  solid   .load(device,"gbuffer",   false,meshlets);
  atest   .load(device,"gbuffer_at",false,meshlets);
  ghost   .load(device,"ghost",     false,meshlets);
  emmision.load(device,"emi",       false,meshlets);
  multiply.load(device,"mul",       false,meshlets);

  water   .load(device,"water",device.properties().tesselationShader,false);

  solidF  .load(device,"",  false,meshlets);
  atestF  .load(device,"at",false,meshlets);

  shadow  .load(device,"shadow",   false,meshlets);
  shadowAt.load(device,"shadow_at",false,meshlets);

  copy               = postEffect("copy");

  ssao               = computeShader("ssao.comp.sprv");
  ssaoCompose        = postEffect("ssao_compose");
  if(Gothic::inst().doRayQuery())
    ssaoRq = computeShader("ssao_rq.comp.sprv");

  shadowResolve      = postEffect("shadow_resolve");
  shadowResolveSh    = postEffect("shadow_resolve", "shadow_resolve_sh");
  if(Gothic::inst().doRayQuery() && Resources::device().properties().bindless.nonUniformIndexing)
    shadowResolveRq = postEffect("shadow_resolve", "shadow_resolve_rq");

  cloudsLut          = computeShader("clouds_lut.comp.sprv");
  skyTransmittance   = postEffect("sky_transmittance");
  skyMultiScattering = postEffect("sky_multi_scattering");
  skyViewLut         = postEffect("sky_view_lut");

  fogViewLut         = postEffect("fog_view_lut");
  fogViewLut3dLQ     = computeShader("fog_view_lut_lq.comp.sprv");
  fogViewLut3dHQ     = computeShader("fog_view_lut_hq.comp.sprv");
  shadowDownsample   = computeShader("shadow_downsample.comp.sprv");
  fogOcclusion       = computeShader("fog3d.comp.sprv");

  sky                = postEffect("sky");
  fog                = fogShader ("fog");
  fog3dLQ            = fogShader ("fog3d_lq");
  fog3dHQ            = fogShader ("fog3d_hq");

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
  if(Gothic::inst().doRayQuery()) {
    if(Resources::device().properties().bindless.nonUniformIndexing) {
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

  if(meshlets) {
    hiZRaw = computeShader("hiZRaw.comp.sprv");
    hiZPot = computeShader("hiZPot.comp.sprv");
    hiZMip = computeShader("hiZMip.comp.sprv");
    }

  if(meshlets) {
    RenderState state;
    state.setCullFaceMode(RenderState::CullMode::Front);
    state.setZTestMode   (RenderState::ZTestMode::Less);

    auto sh = GothicShader::get(string_frm("lnd_hiz.",defaultWg,".mesh.sprv"));
    auto ms = device.shader(sh.data,sh.len);

    sh      = GothicShader::get(string_frm("lnd_hiz.",defaultWg,".task.sprv"));
    auto ts = device.shader(sh.data,sh.len);

    sh      = GothicShader::get("lnd_hiz.frag.sprv");
    auto fs = device.shader(sh.data,sh.len);

    lndPrePass = device.pipeline(state,ts,ms,fs);
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

const RenderPipeline* Shaders::materialPipeline(const Material& mat, ObjectsBucket::Type t, PipelineType pt) const {
  const auto alpha = (mat.isGhost ? Material::Ghost : mat.alpha);

  for(auto& i:materials) {
    if(i.alpha==alpha && i.type==t && i.pipelineType==pt)
      return &i.pipeline;
    }

  const MaterialTemplate* forward  = nullptr;
  const MaterialTemplate* deffered = nullptr;
  const MaterialTemplate* shadow   = nullptr;

  RenderState state;
  state.setCullFaceMode(RenderState::CullMode::Front);
  state.setZTestMode   (RenderState::ZTestMode::LEqual);
  //state.setZTestMode   (RenderState::ZTestMode::Less);

  if(pt==PipelineType::T_Shadow) {
    state.setZTestMode(RenderState::ZTestMode::Greater); //FIXME
    }

  switch(alpha) {
    case Material::Solid:
      forward  = &solidF;
      deffered = &solid;
      shadow   = &this->shadow;
      break;
    case Material::AlphaTest:
      forward  = &atestF;
      deffered = &atest;
      shadow   = &shadowAt;
      break;
    case Material::Water:
      forward  = &water;
      break;
    case Material::Ghost:
      forward  = &ghost;
      break;
    case Material::Transparent:
      forward  = &solidF;
      deffered = nullptr;

      state.setBlendSource  (RenderState::BlendMode::SrcAlpha); // premultiply in shader
      state.setBlendDest    (RenderState::BlendMode::OneMinusSrcAlpha);
      state.setZWriteEnabled(false);
      break;
    case Material::AdditiveLight:
      forward = &emmision;

      state.setBlendSource  (RenderState::BlendMode::SrcAlpha);
      state.setBlendDest    (RenderState::BlendMode::One);
      state.setZWriteEnabled(false);
      break;
    case Material::Multiply:
    case Material::Multiply2:
      forward = &multiply;

      state.setBlendSource  (RenderState::BlendMode::DstColor);
      state.setBlendDest    (RenderState::BlendMode::SrcColor);
      state.setZWriteEnabled(false);
      break;
    }

  static bool overdrawDbg = false;
  if(overdrawDbg &&
     (alpha==Material::Solid || alpha==Material::AlphaTest) && t!=ObjectsBucket::Landscape && t!=ObjectsBucket::LandscapeShadow && pt!=T_Shadow) {
    state.setBlendSource(RenderState::BlendMode::One);
    state.setBlendDest  (RenderState::BlendMode::One);
    state.setZWriteEnabled(false);
    state.setZTestMode(RenderState::ZTestMode::Always);
    }

  const MaterialTemplate* temp = nullptr;
  switch(pt) {
    case T_Forward:
      temp = forward;
      break;
    case T_Deffered:
      temp = deffered;
      break;
    case T_Shadow:
      temp = shadow;
      break;
    }

  if(temp==nullptr)
    return nullptr;

  materials.emplace_front();
  auto& b = materials.front();
  b.alpha        = alpha;
  b.type         = t;
  b.pipelineType = pt;
  switch(t) {
    case ObjectsBucket::Landscape:
    case ObjectsBucket::LandscapeShadow:
      b.pipeline = pipeline(state,temp->lnd);
      break;
    case ObjectsBucket::Static:
    case ObjectsBucket::Movable:
      b.pipeline = pipeline(state,temp->obj);
      break;
    case ObjectsBucket::Morph:
      b.pipeline = pipeline(state,temp->mph);
      break;
    case ObjectsBucket::Animated:
      b.pipeline = pipeline(state,temp->ani);
      break;
    case ObjectsBucket::Pfx:
      b.pipeline = pipeline(state,temp->pfx);
      break;
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

  auto sh = GothicShader::get(string_frm(name,".vert.sprv"));
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

RenderPipeline Shaders::pipeline(RenderState& st, const ShaderSet &sh) const {
  if(!sh.me.isEmpty() && !sh.ts.isEmpty()) {
    return Resources::device().pipeline(st,sh.ts,sh.me,sh.fs);
    }
  if(!sh.me.isEmpty()) {
    return Resources::device().pipeline(st,Shader(),sh.me,sh.fs);
    }
  if(!sh.tc.isEmpty() && !sh.te.isEmpty()) {
    return Resources::device().pipeline(Triangles,st,sh.vs,sh.tc,sh.te,sh.fs);
    }
  return Resources::device().pipeline(Triangles,st,sh.vs,sh.fs);
  }
