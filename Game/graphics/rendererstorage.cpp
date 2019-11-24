#include "rendererstorage.h"

#include <Tempest/Device>

#include "resources.h"

using namespace Tempest;

void RendererStorage::ShaderPair::load(Assets &asset, const char *tag, const char *format) {
  char buf[256]={};

  std::snprintf(buf,sizeof(buf),format,tag,"vert");
  vs = &asset[buf].get<Shader>();
  if(vs->isEmpty())
    throw std::runtime_error(std::string("unable to load shader: \"")+buf+"\"");

  std::snprintf(buf,sizeof(buf),format,tag,"frag");
  fs = &asset[buf].get<Shader>();
  if(fs->isEmpty())
    throw std::runtime_error(std::string("unable to load shader: \"")+buf+"\"");
  }

void RendererStorage::Material::load(Assets &asset, const char *f) {
  main.  load(asset,f,"%s.%s.sprv");
  shadow.load(asset,f,"%s_shadow.%s.sprv");
  }

RendererStorage::RendererStorage(Device &device)
  :device(device),shaders("shader",device) {
  land  .load(shaders,"land");
  object.load(shaders,"object");
  ani   .load(shaders,"anim");
  pfx   .load(shaders,"pfx");

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

  initPipeline();
  initShadow();
  }

template<class Vertex>
RenderPipeline RendererStorage::pipeline(RenderState& st, const UniformsLayout& ulay,const ShaderPair &sh) {
  return device.pipeline<Vertex>(Triangles,st,ulay,*sh.vs,*sh.fs);
  }

void RendererStorage::initPipeline() {
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

  auto& vsSky  = shaders["sky.vert.sprv"].get<Shader>();
  auto& fsSky  = shaders["sky.frag.sprv"].get<Shader>();

  auto& vsComp = shaders["shadow_compose.vert.sprv"].get<Shader>();
  auto& fsComp = shaders["shadow_compose.frag.sprv"].get<Shader>();
  
  pSky           = device.pipeline<Resources::VertexFsq>(Triangles,stateFsq,layoutSky, vsSky,  fsSky );
  pComposeShadow = device.pipeline<Resources::VertexFsq>(Triangles,stateFsq,layoutComp,vsComp, fsComp);
  
  pLandAlpha     = pipeline<Resources::Vertex> (stateAlpha,layoutLnd,land.main);
  pLand          = pipeline<Resources::Vertex> (stateLnd,  layoutLnd,land.main);

  pObject        = pipeline<Resources::Vertex> (stateObj,layoutObj,object.main);
  pAnim          = pipeline<Resources::VertexA>(stateObj,layoutAni,ani.main);

  pPfx           = pipeline<Resources::Vertex> (statePfx,layoutLnd,pfx.main);
  }

void RendererStorage::initShadow() {
  RenderState state;
  state.setZTestMode   (RenderState::ZTestMode::Less);
  state.setCullFaceMode(RenderState::CullMode::Back);
  //state.setCullFaceMode(RenderState::CullMode::Front);

  pLandSh   = pipeline<Resources::Vertex> (state,layoutLnd,land.shadow);
  pObjectSh = pipeline<Resources::Vertex> (state,layoutObj,object.shadow);
  pAnimSh   = pipeline<Resources::VertexA>(state,layoutAni,ani.shadow);
  }
