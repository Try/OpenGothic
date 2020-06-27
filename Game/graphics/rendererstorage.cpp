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

void RendererStorage::Material::load(Device &device, const char *f) {
  main.  load(device,f,"%s.%s.sprv");
  shadow.load(device,f,"%s_shadow.%s.sprv");
  light .load(device,f,"%s_light.%s.sprv");
  }

RendererStorage::RendererStorage(Device& device, Gothic& gothic)
  :device(device) {
  obj      .load(device,"obj");
  objAt    .load(device,"obj_at");
  objEmi   .load(device,"obj_emi");
  ani      .load(device,"ani");
  aniAt    .load(device,"ani_at");
  aniEmi   .load(device,"ani_emi");

  initPipeline(gothic);
  initShadow();
  }

template<class Vertex>
RenderPipeline RendererStorage::pipeline(RenderState& st, const ShaderPair &sh) {
  return device.pipeline<Vertex>(Triangles,st,sh.vs,sh.fs);
  }

void RendererStorage::initPipeline(Gothic& gothic) {
  RenderState stateAlpha;
  stateAlpha.setCullFaceMode(RenderState::CullMode::Front);
  stateAlpha.setBlendSource (RenderState::BlendMode::src_alpha);
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

  auto sh     = GothicShader::get("shadow_compose.vert.sprv");
  auto vsComp = device.shader(sh.data,sh.len);
  sh          = GothicShader::get("shadow_compose.frag.sprv");
  auto fsComp = device.shader(sh.data,sh.len);

  pComposeShadow = device.pipeline<Resources::VertexFsq>(Triangles,stateFsq,vsComp, fsComp);

  pAnim          = pipeline<Resources::VertexA>(stateObj,   ani.main);
  pAnimAt        = pipeline<Resources::VertexA>(stateObj,   aniAt.main);
  pAnimLt        = pipeline<Resources::VertexA>(stateAdd,   ani.light);
  pAnimAtLt      = pipeline<Resources::VertexA>(stateAdd,   aniAt.light);

  pObject        = pipeline<Resources::Vertex> (stateObj,   obj.main);
  pObjectAt      = pipeline<Resources::Vertex> (stateObj,   objAt.main);
  pObjectLt      = pipeline<Resources::Vertex> (stateAdd,   obj.light);
  pObjectAtLt    = pipeline<Resources::Vertex> (stateAdd,   objAt.light);

  pObjectAlpha   = pipeline<Resources::Vertex> (stateAlpha, obj.main);
  pAnimAlpha     = pipeline<Resources::VertexA>(stateAlpha, ani.main);

  pObjectMAdd    = pipeline<Resources::Vertex> (stateMAdd,  objEmi.main);
  pAnimMAdd      = pipeline<Resources::VertexA>(stateMAdd,  aniEmi.main);

  if(gothic.version().game==1) {
    auto sh    = GothicShader::get("sky_g1.vert.sprv");
    auto vsSky = device.shader(sh.data,sh.len);
    sh         = GothicShader::get("sky_g1.frag.sprv");
    auto fsSky = device.shader(sh.data,sh.len);
    pSky       = device.pipeline<Resources::VertexFsq>(Triangles, stateFsq, vsSky,  fsSky);
    } else {
    auto sh    = GothicShader::get("sky.vert.sprv");
    auto vsSky = device.shader(sh.data,sh.len);
    sh         = GothicShader::get("sky.frag.sprv");
    auto fsSky = device.shader(sh.data,sh.len);
    pSky       = device.pipeline<Resources::VertexFsq>(Triangles, stateFsq, vsSky,  fsSky);
    }
  }

void RendererStorage::initShadow() {
  RenderState state;
  state.setZTestMode   (RenderState::ZTestMode::Less);
  state.setCullFaceMode(RenderState::CullMode::Back);
  //state.setCullFaceMode(RenderState::CullMode::Front);

  pObjectSh   = pipeline<Resources::Vertex> (state,obj.shadow);
  pObjectAtSh = pipeline<Resources::Vertex> (state,objAt.shadow);
  pAnimSh     = pipeline<Resources::VertexA>(state,ani.shadow);
  pAnimAtSh   = pipeline<Resources::VertexA>(state,aniAt.shadow);
  }
