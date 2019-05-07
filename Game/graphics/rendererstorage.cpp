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
  }

void RendererStorage::initPipeline(Tempest::RenderPass &pass,uint32_t w, uint32_t h) {
  if((pLand.w()==w && pLand.h()==h) || w==0 || h==0)
    return;
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

  RenderState stateSky;
  stateSky.setZTestMode   (RenderState::ZTestMode::LEqual);
  stateSky.setCullFaceMode(RenderState::CullMode::Front);
  
  pSky       = device.pipeline<Resources::VertexFsq>(pass,w,h,Triangles,stateSky,layoutSky,vsSky,    fsSky    );
  
  pLandAlpha = device.pipeline<Resources::Vertex>   (pass,  w,h,Triangles,stateAlpha,layoutLnd,land.vs,land.fs);
  pLand      = device.pipeline<Resources::Vertex>   (pass,  w,h,Triangles,stateLnd,  layoutLnd,land.vs,land.fs);

  pObject    = device.pipeline<Resources::Vertex>   (pass,w,h,Triangles,stateObj,layoutObj,object.vs,object.fs);
  pAnim      = device.pipeline<Resources::VertexA>  (pass,w,h,Triangles,stateObj,layoutAni,ani.vs,   ani.fs   );
  }

void RendererStorage::initShadow(RenderPass &shadow, uint32_t w, uint32_t h) {
  if((pLandSh.w()==w && pLandSh.h()==h) || w==0 || h==0)
    return;

  RenderState state;
  state.setZTestMode   (RenderState::ZTestMode::Less);
  state.setCullFaceMode(RenderState::CullMode::Back);
  //state.setCullFaceMode(RenderState::CullMode::Front);

  pLandSh    = device.pipeline<Resources::Vertex> (shadow,w,h,Triangles,state,layoutLnd,land.vsShadow,  land.fsShadow  );
  pObjectSh  = device.pipeline<Resources::Vertex> (shadow,w,h,Triangles,state,layoutObj,object.vsShadow,object.fsShadow);
  pAnimSh    = device.pipeline<Resources::VertexA>(shadow,w,h,Triangles,state,layoutAni,ani.vsShadow,   ani.fsShadow   );
  }
