#include "renderer.h"

#include <Tempest/Color>

#include "graphics/staticmesh.h"
#include "gothic.h"

using namespace Tempest;

Renderer::Renderer(Tempest::Device &device,Gothic& gothic)
  :device(device),gothic(gothic),land(device),vobGroup(device) {
  vsLand   = device.loadShader("shader/land.vert.sprv");
  fsLand   = device.loadShader("shader/land.frag.sprv");

  vsObject = device.loadShader("shader/object.vert.sprv");
  fsObject = device.loadShader("shader/object.frag.sprv");

  gothic.onWorldChanged.bind(this,&Renderer::initWorld);
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

  projective.perspective( 45.0f, float(w)/float(h), 0.1f, 100.0f );
  view.identity();
  view.translate(0,0,4);
  view.rotate(spin.y, 1, 0, 0);
  view.rotate(spin.x, 0, 1, 0);
  view.scale(zoom);

  auto viewProj=projective;
  viewProj.mul(view);

  prebuiltCmdBuf();
  }

RenderPipeline& Renderer::landPipeline(RenderPass &pass,uint32_t w,uint32_t h) {
  RenderState state;
  state.setZTestMode(RenderState::ZTestMode::Less);

  if(pLand.w()!=w || pLand.h()!=h)
    pLand = device.pipeline<Resources::Vertex>(pass,w,h,Triangles,state,land.uboLayout(),vsLand,fsLand);
  return pLand;
  }

RenderPipeline &Renderer::objPipeline(RenderPass &pass, uint32_t w, uint32_t h) {
  RenderState state;
  state.setZTestMode(RenderState::ZTestMode::Less);

  if(pObject.w()!=w || pObject.h()!=h)
    pObject = device.pipeline<Resources::Vertex>(pass,w,h,Triangles,state,vobGroup.uboLayout(),vsObject,fsObject);
  return pObject;
  }

void Renderer::setDebugView(const PointF &spin, const float zoom) {
  if(this->spin==spin && this->zoom==zoom)
    ;//return;

  this->spin=spin;
  this->zoom=zoom;
  std::fill(needToUpdateUbo,needToUpdateUbo+3,true);
  }

void Renderer::updateUbo(const FrameBuffer& fbo,const Gothic &,uint32_t imgId) {
  if(!needToUpdateUbo[imgId])
    return;
  needToUpdateUbo[imgId]=false;

  projective.perspective( 45.0f, float(fbo.w())/float(fbo.h()), 0.1f, 100.0f );
  view.identity();
  view.translate(0,0,4);
  view.rotate(spin.y, 1, 0, 0);
  view.rotate(spin.x, 0, 1, 0);
  view.scale(zoom);

  auto viewProj=projective;
  viewProj.mul(view);

  for(size_t i=0;i<objStatic.size();++i){
    auto mt = viewProj;
    mt.mul(objStatic[i].objMat);
    objStatic[i].obj.setMatrix(mt);
    }

  land    .commitUbo(viewProj,imgId);
  vobGroup.commitUbo(imgId);
  }

void Renderer::draw(CommandBuffer &cmd, uint32_t imgId, const Gothic &gothic) {
  FrameBuffer&  fbo = fbo3d[imgId];

  landPipeline(mainPass,fbo.w(),fbo.h());
  objPipeline (mainPass,fbo.w(),fbo.h());
  updateUbo(fbo,gothic,device.frameId());

  //cmd.exec(cmdLand[device.frameId()]);

  cmd.beginRenderPass(fbo,mainPass);
  land.draw(cmd,pLand,imgId,gothic);
  for(auto& i:objStatic){
    vobGroup.setUniforms(cmd,pObject,imgId,i.obj);
    cmd.draw(i.obj.vbo(),i.obj.ibo());
    }
  cmd.endRenderPass();
  }

void Renderer::initWorld() {
  auto&  world = gothic.world();
  Object obj;

  std::fill(needToUpdateUbo,needToUpdateUbo+3,true);

  objStatic.clear();
  for(auto& v:world.staticObj){
    for(auto& s:v.mesh->sub) {
      if(objStatic.size()>=20000)
        break;

      if(!s.texture || s.texture->isEmpty() || !v.mesh)
        continue;

      obj.obj     = vobGroup.get(s.texture,*v.mesh,s.ibo);
      obj.objMat  = v.objMat;
      obj.obj.setMatrix(v.objMat);

      objStatic.push_back(std::move(obj));
      }
    }

  std::sort(objStatic.begin(),objStatic.end(),[](const Object& l,const Object& r){
    return l.obj.orderId()<r.obj.orderId();
    });

  prebuiltCmdBuf();
  }

void Renderer::prebuiltCmdBuf() {
  cmdLand.clear();

  /*
  for(size_t i=0;i<device.maxFramesInFlight();++i)
    if(landPF[i].uboGpu.size()==0)
      landPF[i].uboGpu = device.loadUbo(&uboCpu,sizeof(uboCpu));

  for(size_t i=0;i<fbo3d.size();++i){
    auto cmd=device.commandSecondaryBuffer();

    cmd.begin(fbo3d[i],mainPass);
    prebuiltCmdBuf(cmd,landPF[i]);
    cmd.end();

    cmdLand.emplace_back(std::move(cmd));
    }*/
  }
