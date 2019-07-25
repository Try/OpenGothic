#include "rendererstorage.h"

#include <Tempest/Device>

#include "resources.h"

using namespace Tempest;

void RendererStorage::Material::load(Tempest::Device &device,const char *f) {
  char buf[256]={};

  std::snprintf(buf,sizeof(buf),"shader/%s.%s.sprv",f,"vert");
  vs = device.loadShader(buf);

  std::snprintf(buf,sizeof(buf),"shader/%s.%s.sprv",f,"frag");
  fs = device.loadShader(buf);

  std::snprintf(buf,sizeof(buf),"shader/%s_shadow.%s.sprv",f,"vert");
  vsShadow = device.loadShader(buf);

  std::snprintf(buf,sizeof(buf),"shader/%s_shadow.%s.sprv",f,"frag");
  fsShadow = device.loadShader(buf);
  }

RendererStorage::RendererStorage(Tempest::Device &device)
  :device(device) {
  land  .load(device,"land");
  object.load(device,"object");
  ani   .load(device,"anim");

  vsSky = device.loadShader("shader/sky.vert.sprv");
  fsSky = device.loadShader("shader/sky.frag.sprv");

  vsComp = device.loadShader("shader/shadow_compose.vert.sprv");
  fsComp = device.loadShader("shader/shadow_compose.frag.sprv");

  layoutLnd.add(0,Tempest::UniformsLayout::UboDyn, Tempest::UniformsLayout::Vertex);
  layoutLnd.add(2,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);
  layoutLnd.add(3,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);

  layoutObj.add(0,Tempest::UniformsLayout::Ubo,    Tempest::UniformsLayout::Vertex);
  layoutObj.add(1,Tempest::UniformsLayout::UboDyn, Tempest::UniformsLayout::Vertex);
  layoutObj.add(2,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);
  layoutObj.add(3,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);

  layoutAni.add(0,Tempest::UniformsLayout::Ubo,    Tempest::UniformsLayout::Vertex);
  layoutAni.add(1,Tempest::UniformsLayout::UboDyn, Tempest::UniformsLayout::Vertex);
  layoutAni.add(2,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);
  layoutAni.add(3,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);

  layoutSky.add(0,Tempest::UniformsLayout::UboDyn, Tempest::UniformsLayout::Fragment);
  layoutSky.add(1,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);
  layoutSky.add(2,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);

  layoutComp.add(0,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);
  layoutComp.add(1,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);
  }

void RendererStorage::initPipeline(Tempest::RenderPass &pass) {
  renderPass=&pass;

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
  
  pSky           = device.pipeline<Resources::VertexFsq>(Triangles,stateFsq,layoutSky, vsSky,  fsSky );
  pComposeShadow = device.pipeline<Resources::VertexFsq>(Triangles,stateFsq,layoutComp,vsComp, fsComp);
  
  pLandAlpha = device.pipeline<Resources::Vertex>   (Triangles,stateAlpha,layoutLnd,land.vs,land.fs);
  pLand      = device.pipeline<Resources::Vertex>   (Triangles,stateLnd,  layoutLnd,land.vs,land.fs);

  pObject    = device.pipeline<Resources::Vertex>   (Triangles,stateObj,layoutObj,object.vs,object.fs);
  pAnim      = device.pipeline<Resources::VertexA>  (Triangles,stateObj,layoutAni,ani.vs,   ani.fs   );

  initShadow();
  }

void RendererStorage::initShadow() {
  RenderState state;
  state.setZTestMode   (RenderState::ZTestMode::Less);
  state.setCullFaceMode(RenderState::CullMode::Back);
  //state.setCullFaceMode(RenderState::CullMode::Front);

  pLandSh   = device.pipeline<Resources::Vertex> (Triangles,state,layoutLnd,land.vsShadow,  land.fsShadow  );
  pObjectSh = device.pipeline<Resources::Vertex> (Triangles,state,layoutObj,object.vsShadow,object.fsShadow);
  pAnimSh   = device.pipeline<Resources::VertexA>(Triangles,state,layoutAni,ani.vsShadow,   ani.fsShadow   );
  }
