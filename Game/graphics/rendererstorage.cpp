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
  }

RendererStorage::RendererStorage(Device& device, Gothic& gothic)
  :device(device) {
  object  .load(device,"object");
  objectAt.load(device,"object_at");
  ani     .load(device,"anim");
  pfx     .load(device,"pfx");
  initPipeline(gothic);
  initShadow();
  }

template<class Vertex>
RenderPipeline RendererStorage::pipeline(RenderState& st, const ShaderPair &sh) {
  return device.pipeline<Vertex>(Triangles,st,sh.vs,sh.fs);
  }

void RendererStorage::initPipeline(Gothic& gothic) {
  RenderState stateAlpha;
  stateAlpha.setBlendSource (RenderState::BlendMode::src_alpha);
  stateAlpha.setBlendDest   (RenderState::BlendMode::one_minus_src_alpha);
  stateAlpha.setZTestMode   (RenderState::ZTestMode::Less);
  stateAlpha.setCullFaceMode(RenderState::CullMode::Front);
  stateAlpha.setZWriteEnabled(false);

  RenderState stateObj;
  stateObj.setZTestMode   (RenderState::ZTestMode::Less);
  stateObj.setCullFaceMode(RenderState::CullMode::Front);

  RenderState stateObjDec = stateObj;
  stateObjDec.setZTestMode(RenderState::ZTestMode::LEqual);

  RenderState stateLnd;
  stateLnd.setZTestMode   (RenderState::ZTestMode::Less);
  stateLnd.setCullFaceMode(RenderState::CullMode::Front);

  RenderState stateFsq;
  stateFsq.setZTestMode   (RenderState::ZTestMode::LEqual);
  stateFsq.setCullFaceMode(RenderState::CullMode::Front);
  stateFsq.setZWriteEnabled(false);

  RenderState statePfx;
  statePfx.setZTestMode    (RenderState::ZTestMode::LEqual);
  statePfx.setZWriteEnabled(false);
  statePfx.setCullFaceMode (RenderState::CullMode::Front);
  statePfx.setBlendSource  (RenderState::BlendMode::src_alpha);
  statePfx.setBlendDest    (RenderState::BlendMode::one);

  auto sh     = GothicShader::get("shadow_compose.vert.sprv");
  auto vsComp = device.shader(sh.data,sh.len);
  sh          = GothicShader::get("shadow_compose.frag.sprv");
  auto fsComp = device.shader(sh.data,sh.len);

  pComposeShadow = device.pipeline<Resources::VertexFsq>(Triangles,stateFsq,vsComp, fsComp);

  pObject        = pipeline<Resources::Vertex> (stateObj,   object.main);
  pObjectAt      = pipeline<Resources::Vertex> (stateObj,   objectAt.main);
  pObjectAlpha   = pipeline<Resources::Vertex> (stateAlpha, object.main);
  pAnim          = pipeline<Resources::VertexA>(stateObj,   ani.main);

  pPfx           = pipeline<Resources::Vertex> (statePfx,pfx.main);

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

  pObjectSh   = pipeline<Resources::Vertex> (state,object.shadow);
  pObjectAtSh = pipeline<Resources::Vertex> (state,objectAt.shadow);
  pAnimSh     = pipeline<Resources::VertexA>(state,ani.shadow);
  }
