#include "renderer.h"

#include <Tempest/Color>

#include "graphics/staticmesh.h"
#include "gothic.h"

using namespace Tempest;

Renderer::Renderer(Tempest::Device &device)
  :device(device) {
  vsLand   = device.loadShader("shader/land.vert.sprv");
  fsLand   = device.loadShader("shader/land.frag.sprv");

  layout.add(0,Tempest::UniformsLayout::Ubo,    Tempest::UniformsLayout::Vertex);
//  layout.add(0,Tempest::UniformsLayout::UboDyn, Tempest::UniformsLayout::Vertex);
  layout.add(1,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);
  }

void Renderer::initSwapchain(uint32_t w,uint32_t h) {
  const size_t imgC=device.swapchainImageCount();
  fbo3d.clear();

  zbuffer  = device.createTexture(TextureFormat::Depth16,w,h,false);
  mainPass = device.pass(Color(0.0),1.f,zbuffer.format());

  for(size_t i=0;i<imgC;++i) {
    Tempest::Frame frame=device.frame(i);
    fbo3d.emplace_back(device.frameBuffer(frame,zbuffer,mainPass));
    }
  }

RenderPipeline& Renderer::landPipeline(RenderPass &pass,uint32_t w,uint32_t h) {
  RenderState state;
  state.setZTestMode(RenderState::ZTestMode::Less);

  if(pLand.w()!=w || pLand.h()!=h)
    pLand = device.pipeline<Resources::Vertex>(pass,w,h,Triangles,state,layout,vsLand,fsLand);
  return pLand;
  }

void Renderer::setDebugView(const PointF &spin, const float zoom) {
  this->spin=spin;
  this->zoom=zoom;
  }

void Renderer::drawLand(CommandBuffer &cmd,const Gothic &gothic,uint32_t frameId) {
  auto& world = gothic.world();

  uboLand.resize(world.landBlocks().size());
  for(size_t i=0;i<world.landBlocks().size();++i){
    auto& lnd=world.landBlocks()[i];
    auto& ubo=uboLand[i];

    if(ubo.isEmpty())
      ubo = device.uniforms(layout);
    if(!lnd.texture || lnd.texture->isEmpty())
      continue;

    ubo.set(0,uboGpu[frameId],0,sizeof(uboCpu));
    ubo.set(1,*lnd.texture);

    cmd.setUniforms(pLand,ubo);
    cmd.draw(world.landVbo(),lnd.ibo);
    }
  }

void Renderer::updateUbo(const FrameBuffer& fbo,const Gothic &gothic,uint32_t frameId) {
  auto& world = gothic.world();

  projective.perspective( 45.0f, float(fbo.w())/float(fbo.h()), 0.1f, 100.0f );
  view.identity();
  view.translate(0,0,4);
  view.rotate(spin.y, 1, 0, 0);
  view.rotate(spin.x, 0, 1, 0);
  view.scale(zoom);

  auto viewProj=projective;
  viewProj.mul(view);

  uboCpu.mvp = viewProj;

  size_t uboCount=0;
  for(auto& i:world.staticObj)
    uboCount += i.mesh ? i.mesh->sub.size() : 0;

  uboDodads.resize(uboCount);
  uboObj.resize(uboCount);
  for(size_t i=0, objI=0;i<world.staticObj.size();++i){
    for(auto& sub:world.staticObj[i].mesh->sub) {
      (void)sub;
      uboObj[objI].mvp = viewProj;
      uboObj[objI].mvp.mul(world.staticObj[i].objMat);
      ++objI;
      }
    }

  uboObjGpu[frameId] = device.loadUbo(uboObj.data(),uboObj.size()*sizeof(uboObj[0]));
  uboGpu[frameId]    = device.loadUbo(&uboCpu,sizeof(uboCpu));
  }

void Renderer::drawObjects(CommandBuffer &cmd,const Gothic &gothic,uint32_t frameId) {
  auto& world = gothic.world();

  size_t uboCount=0;
  for(auto& obj:world.staticObj){
    for(auto& i:obj.mesh->sub) {
      size_t objId=uboCount;
      auto& ubo=uboDodads[objId];
      uboCount++;

      if(!i.texture || i.texture->isEmpty())
        continue;
      if(ubo.isEmpty())
        ubo = device.uniforms(layout);

      //ubo.set(0,uboObjGpu[frameId],0,uboObjGpu[frameId].size());
      ubo.set(0,uboObjGpu[frameId],objId*sizeof(UboLand),sizeof(UboLand));
      ubo.set(1,*i.texture);
      }
    }

  uboCount=0;
  for(auto& obj:world.staticObj){
    for(auto& i:obj.mesh->sub) {
      auto& ubo=uboDodads[uboCount];
      uboCount++;
      if(ubo.isEmpty())
        continue;
      cmd.setUniforms(pLand,ubo);
      cmd.draw(obj.mesh->vbo,i.ibo);
      }
    }
  }

void Renderer::draw(CommandBuffer &cmd, uint32_t imgId, const Gothic &gothic) {
  const uint8_t frameId = device.frameId();
  FrameBuffer&  fbo     = fbo3d[imgId];

  landPipeline(mainPass,fbo.w(),fbo.h());
  updateUbo(fbo,gothic,frameId);

  cmd.beginRenderPass(fbo,mainPass);

  drawLand   (cmd,gothic,frameId);
  // drawObjects(cmd,gothic,frameId);

  cmd.endRenderPass();
  }
