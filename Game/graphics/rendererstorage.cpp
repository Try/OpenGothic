#include "rendererstorage.h"

#include <Tempest/Device>

#include "resources.h"

using namespace Tempest;

RendererStorage::RendererStorage(Tempest::Device &device)
  :device(device) {
  vsLand   = device.loadShader("shader/land.vert.sprv");
  fsLand   = device.loadShader("shader/land.frag.sprv");

  vsObject = device.loadShader("shader/object.vert.sprv");
  fsObject = device.loadShader("shader/object.frag.sprv");

  layoutObj.add(0,Tempest::UniformsLayout::Ubo,    Tempest::UniformsLayout::Vertex);
  layoutObj.add(1,Tempest::UniformsLayout::UboDyn, Tempest::UniformsLayout::Vertex);
  layoutObj.add(2,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);

  layoutLnd.add(0,Tempest::UniformsLayout::UboDyn, Tempest::UniformsLayout::Vertex);
  layoutLnd.add(1,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);
  }

void RendererStorage::initPipeline(Tempest::RenderPass &pass, uint32_t w, uint32_t h) {
  if((pLand.w()==w && pLand.h()==h) || w==0 || h==0)
    return;
  renderPass=&pass;

  RenderState stateObj;
  stateObj.setZTestMode   (RenderState::ZTestMode::Less);
  stateObj.setCullFaceMode(RenderState::CullMode::Front);

  RenderState stateLnd;
  stateLnd.setZTestMode   (RenderState::ZTestMode::Less);
  stateLnd.setCullFaceMode(RenderState::CullMode::Front);

  pObject = device.pipeline<Resources::Vertex>(pass,w,h,Triangles,stateObj,layoutObj,vsObject,fsObject);
  pLand   = device.pipeline<Resources::Vertex>(pass,w,h,Triangles,stateLnd,layoutLnd,vsLand,  fsLand  );
  }
