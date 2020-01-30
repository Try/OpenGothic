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

RendererStorage::RendererStorage(Device &device,Gothic& gothic)
  :device(device) {
  land  .load(device,"land");
  landAt.load(device,"land_at");
  object.load(device,"object");
  ani   .load(device,"anim");
  pfx   .load(device,"pfx");

  layoutLnd.add(0,UniformsLayout::UboDyn, UniformsLayout::Vertex);
  layoutLnd.add(2,UniformsLayout::Texture,UniformsLayout::Fragment);
  layoutLnd.add(3,UniformsLayout::Texture,UniformsLayout::Fragment);

  layoutObj.add(0,UniformsLayout::Ubo,    UniformsLayout::Vertex);
  layoutObj.add(1,UniformsLayout::UboDyn, UniformsLayout::Vertex);
  layoutObj.add(2,UniformsLayout::Texture,UniformsLayout::Fragment);
  layoutObj.add(3,UniformsLayout::Texture,UniformsLayout::Fragment);

  layoutAni.add(0,UniformsLayout::Ubo,    UniformsLayout::Vertex);
  layoutAni.add(1,UniformsLayout::UboDyn, UniformsLayout::Vertex);
  layoutAni.add(2,UniformsLayout::Texture,UniformsLayout::Fragment);
  layoutAni.add(3,UniformsLayout::Texture,UniformsLayout::Fragment);

  layoutSky.add(0,UniformsLayout::UboDyn, UniformsLayout::Fragment);
  layoutSky.add(1,UniformsLayout::Texture,UniformsLayout::Fragment);
  layoutSky.add(2,UniformsLayout::Texture,UniformsLayout::Fragment);
  layoutSky.add(3,UniformsLayout::Texture,UniformsLayout::Fragment);
  layoutSky.add(4,UniformsLayout::Texture,UniformsLayout::Fragment);

  layoutComp.add(0,UniformsLayout::Texture,UniformsLayout::Fragment);
  layoutComp.add(1,UniformsLayout::Texture,UniformsLayout::Fragment);

  initPipeline(gothic);
  initShadow();
  }

template<class Vertex>
RenderPipeline RendererStorage::pipeline(RenderState& st, const UniformsLayout& ulay,const ShaderPair &sh) {
  return device.pipeline<Vertex>(Triangles,st,ulay,sh.vs,sh.fs);
  }

void RendererStorage::initPipeline(Gothic& gothic) {
  RenderState stateAlpha;
  stateAlpha.setBlendSource (RenderState::BlendMode::src_alpha);
  stateAlpha.setBlendDest   (RenderState::BlendMode::one_minus_src_alpha);
  stateAlpha.setZTestMode   (RenderState::ZTestMode::Less);
  stateAlpha.setCullFaceMode(RenderState::CullMode::Front);

  RenderState stateObj;
  stateObj.setZTestMode   (RenderState::ZTestMode::Less);
  stateObj.setCullFaceMode(RenderState::CullMode::Front);

  RenderState stateLnd;
  stateLnd.setZTestMode   (RenderState::ZTestMode::Less);
  stateLnd.setCullFaceMode(RenderState::CullMode::Front);

  RenderState stateFsq;
  stateFsq.setZTestMode   (RenderState::ZTestMode::LEqual);
  stateFsq.setCullFaceMode(RenderState::CullMode::Front);

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

  pComposeShadow = device.pipeline<Resources::VertexFsq>(Triangles,stateFsq,layoutComp,vsComp, fsComp);

  pLand          = pipeline<Resources::Vertex> (stateLnd,  layoutLnd,land.main);
  pLandAt        = pipeline<Resources::Vertex> (stateLnd,  layoutLnd,landAt.main);
  pLandAlpha     = pipeline<Resources::Vertex> (stateAlpha,layoutLnd,land.main);

  pObject        = pipeline<Resources::Vertex> (stateObj,layoutObj,object.main);
  pAnim          = pipeline<Resources::VertexA>(stateObj,layoutAni,ani.main);

  pPfx           = pipeline<Resources::Vertex> (statePfx,layoutLnd,pfx.main);

  if(gothic.version().game==1) {
    auto sh    = GothicShader::get("sky_g1.vert.sprv");
    auto vsSky = device.shader(sh.data,sh.len);
    sh         = GothicShader::get("sky_g1.frag.sprv");
    auto fsSky = device.shader(sh.data,sh.len);
    pSky       = device.pipeline<Resources::VertexFsq>(Triangles,stateFsq,layoutSky, vsSky,  fsSky );
    } else {
    auto sh    = GothicShader::get("sky.vert.sprv");
    auto vsSky = device.shader(sh.data,sh.len);
    sh         = GothicShader::get("sky.frag.sprv");
    auto fsSky = device.shader(sh.data,sh.len);
    pSky       = device.pipeline<Resources::VertexFsq>(Triangles,stateFsq,layoutSky, vsSky,  fsSky );
    }
  }

void RendererStorage::initShadow() {
  RenderState state;
  state.setZTestMode   (RenderState::ZTestMode::Less);
  state.setCullFaceMode(RenderState::CullMode::Back);
  //state.setCullFaceMode(RenderState::CullMode::Front);

  pLandSh   = pipeline<Resources::Vertex> (state,layoutLnd,land.shadow);
  pLandAtSh = pipeline<Resources::Vertex> (state,layoutLnd,landAt.shadow);
  pObjectSh = pipeline<Resources::Vertex> (state,layoutObj,object.shadow);
  pAnimSh   = pipeline<Resources::VertexA>(state,layoutAni,ani.shadow);
  }
