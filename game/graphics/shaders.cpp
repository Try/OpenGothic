#include "shaders.h"

#include <Tempest/Device>

#include "gothic.h"
#include "resources.h"

#include "shader.h"

using namespace Tempest;

Shaders* Shaders::instance = nullptr;

void Shaders::ShaderPair::load(Device &device, const char *tag, const char *format, bool hasTesselation) {
  char buf[256]={};

  std::snprintf(buf,sizeof(buf),format,tag,"vert");
  auto sh = GothicShader::get(buf);
  vs = device.shader(sh.data,sh.len);

  std::snprintf(buf,sizeof(buf),format,tag,"frag");
  sh = GothicShader::get(buf);
  fs = device.shader(sh.data,sh.len);

  if(hasTesselation) {
    std::snprintf(buf,sizeof(buf),format,tag,"tesc");
    auto sh = GothicShader::get(buf);
    tc = device.shader(sh.data,sh.len);

    std::snprintf(buf,sizeof(buf),format,tag,"tese");
    sh = GothicShader::get(buf);
    te = device.shader(sh.data,sh.len);
    }
  }

void Shaders::ShaderPair::load(Device& device, const char* tag, bool hasTesselation) {
  load(device,tag,"%s.%s.sprv",hasTesselation);
  }

void Shaders::MaterialTemplate::load(Device &device, const char *tag, bool hasTesselation) {
  char fobj[256]={};
  char fani[256]={};
  char fmph[256]={};
  char fclr[256]={};
  if(tag==nullptr || tag[0]=='\0') {
    std::snprintf(fobj,sizeof(fobj),"obj");
    std::snprintf(fani,sizeof(fani),"ani");
    std::snprintf(fmph,sizeof(fani),"mph");
    std::snprintf(fclr,sizeof(fclr),"clr");
    } else {
    std::snprintf(fobj,sizeof(fobj),"obj_%s",tag);
    std::snprintf(fani,sizeof(fani),"ani_%s",tag);
    std::snprintf(fmph,sizeof(fmph),"mph_%s",tag);
    std::snprintf(fclr,sizeof(fclr),"clr_%s",tag);
    }
  obj.load(device,fobj,"%s.%s.sprv",hasTesselation);
  ani.load(device,fani,"%s.%s.sprv",hasTesselation);
  mph.load(device,fmph,"%s.%s.sprv",hasTesselation);
  clr.load(device,fclr,"%s.%s.sprv",hasTesselation);
  }

Shaders::Shaders() {
  instance = this;
  auto& device = Resources::device();

  solid   .load(device,"gbuffer");
  atest   .load(device,"gbuffer_at");
  water   .load(device,"water",device.properties().tesselationShader);
  ghost   .load(device,"ghost");
  emmision.load(device,"emi");

  solidF  .load(device,"");
  atestF  .load(device,"at");

  shadow  .load(device,"shadow");
  shadowAt.load(device,"shadow_at");

  copy          = postEffect("copy");
  ssao          = postEffect("ssao");
  ssaoCompose   = postEffect("ssao_compose");
  bilateralBlur = postEffect("bilateral");

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
  lights       = device.pipeline<Vec3>(Triangles, state, vsLight, fsLight);
  }

  {
  RenderState state;
  state.setCullFaceMode (RenderState::CullMode::Front);
  state.setBlendSource  (RenderState::BlendMode::One);
  state.setBlendDest    (RenderState::BlendMode::OneMinusSrcAlpha);
  state.setZTestMode    (RenderState::ZTestMode::Greater);
  state.setZWriteEnabled(false);

  auto sh      = GothicShader::get("fog.vert.sprv");
  auto vsFog   = device.shader(sh.data,sh.len);
  sh           = GothicShader::get("fog.frag.sprv");
  auto fsFog   = device.shader(sh.data,sh.len);
  fog          = device.pipeline<Resources::VertexFsq>(Triangles, state, vsFog, fsFog);
  }

  if(Gothic::inst().version().game==1) {
    sky = postEffect("sky_g1");
    } else {
    sky = postEffect("sky_g2");
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
  state.setZTestMode   (RenderState::ZTestMode::Less);

  if(pt==PipelineType::T_Shadow) {
    ;//state.setCullFaceMode(RenderState::CullMode::Back);
    state.setZTestMode(RenderState::ZTestMode::Greater);
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

      state.setBlendSource (RenderState::BlendMode::SrcAlpha); // premultiply in shader
      state.setBlendDest   (RenderState::BlendMode::OneMinusSrcAlpha);
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
      forward  = &solidF;
      deffered = &solid;

      state.setBlendSource  (RenderState::BlendMode::SrcAlpha);
      state.setBlendDest    (RenderState::BlendMode::One);
      state.setZWriteEnabled(false);
      break;
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
    case ObjectsBucket::Static:
    case ObjectsBucket::Movable:
      b.pipeline = pipeline<Resources::Vertex> (state,temp->obj);
      break;
    case ObjectsBucket::Morph:
      b.pipeline = pipeline<Resources::Vertex> (state,temp->mph);
      break;
    case ObjectsBucket::Animated:
      b.pipeline = pipeline<Resources::VertexA>(state,temp->ani);
      break;
    case ObjectsBucket::Pfx:
      b.pipeline = pipeline<Resources::Vertex>(state,temp->clr);
      break;
    }

  return &b.pipeline;
  }

RenderPipeline Shaders::postEffect(std::string_view name) {
  auto& device = Resources::device();

  RenderState stateFsq;
  stateFsq.setCullFaceMode(RenderState::CullMode::Front);
  stateFsq.setZTestMode   (RenderState::ZTestMode::LEqual);
  stateFsq.setZWriteEnabled(false);

  char buf[256] = {};
  std::snprintf(buf,sizeof(buf),"%.*s.vert.sprv",int(name.size()),name.data());
  auto sh = GothicShader::get(buf);
  auto vs = device.shader(sh.data,sh.len);

  std::snprintf(buf,sizeof(buf),"%.*s.frag.sprv",int(name.size()),name.data());
  sh      = GothicShader::get(buf);
  auto fs = device.shader(sh.data,sh.len);
  return device.pipeline<Resources::VertexFsq>(Triangles,stateFsq,vs,fs);
  }

template<class Vertex>
RenderPipeline Shaders::pipeline(RenderState& st, const ShaderPair &sh) const {
  if(!sh.tc.isEmpty() && !sh.te.isEmpty()) {
    return Resources::device().pipeline<Vertex>(Triangles,st,sh.vs,sh.tc,sh.te,sh.fs);
    }
  return Resources::device().pipeline<Vertex>(Triangles,st,sh.vs,sh.fs);
  }
