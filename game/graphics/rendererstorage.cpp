#include "rendererstorage.h"

#include <Tempest/Device>

#include "gothic.h"
#include "resources.h"

#include "shader.h"

using namespace Tempest;

void RendererStorage::ShaderPair::load(Device &device, const char *tag, const char *format) {
  char buf[256]={};

  std::snprintf(buf,sizeof(buf),format,tag,"vert");
  auto sh = GothicShader::get(buf);
  vs = device.shader(sh.data,sh.len);

  std::snprintf(buf,sizeof(buf),format,tag,"frag");
  sh = GothicShader::get(buf);
  fs = device.shader(sh.data,sh.len);
  }

void RendererStorage::ShaderPair::load(Device& device, const char* tag) {
  load(device,tag,"%s.%s.sprv");
  }

void RendererStorage::MaterialTemplate::load(Device &device, const char *tag) {
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
  obj.load(device,fobj,"%s.%s.sprv");
  ani.load(device,fani,"%s.%s.sprv");
  mph.load(device,fmph,"%s.%s.sprv");
  clr.load(device,fclr,"%s.%s.sprv");
  }

RendererStorage::RendererStorage(Gothic& gothic) {
  auto& device = Resources::device();

  solid   .load(device,"gbuffer");
  atest   .load(device,"gbuffer_at");
  water   .load(device,"water");
  ghost   .load(device,"ghost");
  emmision.load(device,"emi");

  solidF  .load(device,"");
  atestF  .load(device,"at");

  shadow  .load(device,"shadow");
  shadowAt.load(device,"shadow_at");

  RenderState stateAlpha;
  stateAlpha.setCullFaceMode(RenderState::CullMode::Front);
  stateAlpha.setBlendSource (RenderState::BlendMode::src_alpha); // premultiply in shader
  stateAlpha.setBlendDest   (RenderState::BlendMode::one_minus_src_alpha);
  stateAlpha.setZTestMode   (RenderState::ZTestMode::Less);
  stateAlpha.setZWriteEnabled(false);

  RenderState stateAdd;
  stateAdd.setCullFaceMode(RenderState::CullMode::Front);
  stateAdd.setBlendSource (RenderState::BlendMode::one);
  stateAdd.setBlendDest   (RenderState::BlendMode::one);
  stateAdd.setZTestMode   (RenderState::ZTestMode::Equal);
  stateAdd.setZWriteEnabled(false);

  RenderState stateObj;
  stateObj.setCullFaceMode(RenderState::CullMode::Front);
  stateObj.setZTestMode   (RenderState::ZTestMode::Less);

  RenderState stateFsq;
  stateFsq.setCullFaceMode(RenderState::CullMode::Front);
  stateFsq.setZTestMode   (RenderState::ZTestMode::LEqual);
  stateFsq.setZWriteEnabled(false);

  RenderState stateMAdd;
  stateMAdd.setCullFaceMode (RenderState::CullMode::Front);
  stateMAdd.setBlendSource  (RenderState::BlendMode::src_alpha);
  stateMAdd.setBlendDest    (RenderState::BlendMode::one);
  stateMAdd.setZTestMode    (RenderState::ZTestMode::Less);
  stateMAdd.setZWriteEnabled(false);

  {
  auto sh = GothicShader::get("copy.vert.sprv");
  auto vs = device.shader(sh.data,sh.len);
  sh      = GothicShader::get("copy.frag.sprv");
  auto fs = device.shader(sh.data,sh.len);
  pCopy = device.pipeline<Resources::VertexFsq>(Triangles,stateFsq,vs,fs);
  }

  {
  auto sh = GothicShader::get("shadow_compose.vert.sprv");
  auto vs = device.shader(sh.data,sh.len);
  sh      = GothicShader::get("shadow_compose.frag.sprv");
  auto fs = device.shader(sh.data,sh.len);
  pComposeShadow = device.pipeline<Resources::VertexFsq>(Triangles,stateFsq,vs,fs);
  }

  {
  RenderState state;
  state.setCullFaceMode (RenderState::CullMode::Front);
  state.setBlendSource  (RenderState::BlendMode::one);
  state.setBlendDest    (RenderState::BlendMode::one);
  state.setZTestMode    (RenderState::ZTestMode::Less);

  state.setZWriteEnabled(false);

  auto sh      = GothicShader::get("light.vert.sprv");
  auto vsLight = device.shader(sh.data,sh.len);
  sh           = GothicShader::get("light.frag.sprv");
  auto fsLight = device.shader(sh.data,sh.len);
  pLights      = device.pipeline<Vec3>(Triangles, state, vsLight, fsLight);
  }

  {
  RenderState state;
  state.setCullFaceMode (RenderState::CullMode::Front);
  state.setBlendSource  (RenderState::BlendMode::one);
  state.setBlendDest    (RenderState::BlendMode::one_minus_src_alpha);
  state.setZTestMode    (RenderState::ZTestMode::Greater);
  state.setZWriteEnabled(false);

  auto sh      = GothicShader::get("fog.vert.sprv");
  auto vsFog   = device.shader(sh.data,sh.len);
  sh           = GothicShader::get("fog.frag.sprv");
  auto fsFog   = device.shader(sh.data,sh.len);
  pFog         = device.pipeline<Resources::VertexFsq>(Triangles, state, vsFog, fsFog);
  }

  if(gothic.version().game==1) {
    auto sh    = GothicShader::get("sky_g1.vert.sprv");
    auto vsSky = device.shader(sh.data,sh.len);
    sh         = GothicShader::get("sky_g1.frag.sprv");
    auto fsSky = device.shader(sh.data,sh.len);
    pSky       = device.pipeline<Resources::VertexFsq>(Triangles, stateFsq, vsSky,  fsSky);
    } else {
    auto sh    = GothicShader::get("sky_g2.vert.sprv");
    auto vsSky = device.shader(sh.data,sh.len);
    sh         = GothicShader::get("sky_g2.frag.sprv");
    auto fsSky = device.shader(sh.data,sh.len);
    pSky       = device.pipeline<Resources::VertexFsq>(Triangles, stateFsq, vsSky,  fsSky);
    }
  }

const RenderPipeline* RendererStorage::materialPipeline(const Material& mat, ObjectsBucket::Type t, PipelineType pt) const {
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

      state.setBlendSource (RenderState::BlendMode::src_alpha); // premultiply in shader
      state.setBlendDest   (RenderState::BlendMode::one_minus_src_alpha);
      state.setZWriteEnabled(false);
      break;
    case Material::AdditiveLight:
      forward = &emmision;

      state.setBlendSource  (RenderState::BlendMode::src_alpha);
      state.setBlendDest    (RenderState::BlendMode::one);
      state.setZWriteEnabled(false);
      break;
    case Material::Multiply:
    case Material::Multiply2:
      forward  = &solidF;
      deffered = &solid;

      state.setBlendSource  (RenderState::BlendMode::src_alpha);
      state.setBlendDest    (RenderState::BlendMode::one);
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

template<class Vertex>
RenderPipeline RendererStorage::pipeline(RenderState& st, const ShaderPair &sh) const {
  return Resources::device().pipeline<Vertex>(Triangles,st,sh.vs,sh.fs);
  }
