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

void RendererStorage::Material::load(Device &device, const char *tag) {
  char fobj[256]={};
  char fani[256]={};
  if(tag==nullptr || tag[0]=='\0') {
    std::snprintf(fobj,sizeof(fobj),"obj");
    std::snprintf(fani,sizeof(fani),"ani");
    } else {
    std::snprintf(fobj,sizeof(fobj),"obj_%s",tag);
    std::snprintf(fani,sizeof(fani),"ani_%s",tag);
    }
  obj.load(device,fobj,"%s.%s.sprv");
  ani.load(device,fani,"%s.%s.sprv");
  }

RendererStorage::RendererStorage(Device& device, Gothic& gothic)
  :device(device) {
  Material obj, objAt, objG, objAtG, objEmi, objShadow, objShadowAt, objWater;
  obj        .load(device,"");
  objG       .load(device,"gbuffer");
  objAt      .load(device,"at");
  objAtG     .load(device,"at_gbuffer");
  objEmi     .load(device,"emi");
  objShadow  .load(device,"shadow");
  objShadowAt.load(device,"shadow_at");
  objWater   .load(device,"water");

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

  pAnim          = pipeline<Resources::VertexA>(stateObj,   obj.ani);
  pAnimG         = pipeline<Resources::VertexA>(stateObj,   objG.ani);
  pAnimAt        = pipeline<Resources::VertexA>(stateObj,   objAt.ani);
  pAnimAtG       = pipeline<Resources::VertexA>(stateObj,   objAtG.ani);
  pAnimLt        = pipeline<Resources::VertexA>(stateAdd,   obj.ani);
  pAnimAtLt      = pipeline<Resources::VertexA>(stateAdd,   objAt.ani);

  pObject        = pipeline<Resources::Vertex> (stateObj,   obj.obj);
  pObjectG       = pipeline<Resources::Vertex> (stateObj,   objG.obj);
  pObjectAt      = pipeline<Resources::Vertex> (stateObj,   objAt.obj);
  pObjectAtG     = pipeline<Resources::Vertex> (stateObj,   objAtG.obj);
  pObjectLt      = pipeline<Resources::Vertex> (stateAdd,   obj.obj);
  pObjectAtLt    = pipeline<Resources::Vertex> (stateAdd,   objAt.obj);

  pObjectAlpha   = pipeline<Resources::Vertex> (stateAlpha, obj.obj);
  pAnimAlpha     = pipeline<Resources::VertexA>(stateAlpha, obj.ani);

  pObjectMAdd    = pipeline<Resources::Vertex> (stateMAdd,  objEmi.obj);
  pAnimMAdd      = pipeline<Resources::VertexA>(stateMAdd,  objEmi.ani);

  pObjectWater   = pipeline<Resources::Vertex> (stateObj,   objWater.obj);
  pAnimWater     = pipeline<Resources::VertexA>(stateObj,   objWater.ani);

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
  pLights      = device.pipeline<Resources::VertexL>(Triangles, state, vsLight, fsLight);
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

  RenderState state;
  state.setZTestMode   (RenderState::ZTestMode::Less);
  state.setCullFaceMode(RenderState::CullMode::Back);
  //state.setCullFaceMode(RenderState::CullMode::Front);

  pObjectSh   = pipeline<Resources::Vertex> (state,objShadow  .obj);
  pObjectAtSh = pipeline<Resources::Vertex> (state,objShadowAt.obj);
  pAnimSh     = pipeline<Resources::VertexA>(state,objShadow  .ani);
  pAnimAtSh   = pipeline<Resources::VertexA>(state,objShadowAt.ani);
  }

template<class Vertex>
RenderPipeline RendererStorage::pipeline(RenderState& st, const ShaderPair &sh) {
  return device.pipeline<Vertex>(Triangles,st,sh.vs,sh.fs);
  }
