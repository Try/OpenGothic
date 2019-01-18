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
  state.setZTestMode   (RenderState::ZTestMode::Less);
  state.setCullFaceMode(RenderState::CullMode::Front);

  if(pLand.w()!=w || pLand.h()!=h)
    pLand = device.pipeline<Resources::Vertex>(pass,w,h,Triangles,state,land.uboLayout(),vsLand,fsLand);
  return pLand;
  }

RenderPipeline &Renderer::objPipeline(RenderPass &pass, uint32_t w, uint32_t h) {
  RenderState state;
  state.setZTestMode   (RenderState::ZTestMode::Less);
  state.setCullFaceMode(RenderState::CullMode::Front);

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
  view.scale(-1,-1,-1);

  auto viewProj=projective;
  viewProj.mul(view);

  for(size_t i=0;i<objStatic.size();++i){
    auto mt = viewProj;
    mt.mul(objStatic[i].objMat);
    objStatic[i].obj.setMatrix(mt);
    }

  land    .setMatrix(imgId,viewProj);
  vobGroup.setMatrix(imgId);
  }

void Renderer::draw(CommandBuffer &cmd, uint32_t imgId, const Gothic &gothic) {
  FrameBuffer& fbo = fbo3d[imgId];
  draw(cmd,fbo,gothic);
  }

void Renderer::draw(CommandBuffer &cmd, FrameBuffer &fbo, const Gothic &gothic) {
  const uint32_t fId=device.frameId();

  landPipeline(mainPass,fbo.w(),fbo.h());
  objPipeline (mainPass,fbo.w(),fbo.h());

  updateUbo(fbo,gothic,fId);

  cmd.beginSecondaryPasses(fbo,mainPass);
  if(!cmdLand.empty())
    cmd.exec(cmdLand[fId]);
  cmd.endRenderPass();

  /*
  cmd.beginRenderPass(fbo,mainPass);
  //land.draw(cmd,pLand,fId,gothic);
  for(auto& i:objStatic){
    vobGroup.setUniforms(cmd,pObject,fId,i.obj);
    cmd.draw(i.obj.vbo(),i.obj.ibo());
    }
  cmd.endRenderPass();*/
  }

CommandBuffer& Renderer::probuilt() {
  return cmdLand[device.frameId()];
  }

void Renderer::prebuiltCmdBuf() {
  cmdLand.clear();

  for(size_t i=0;i<device.maxFramesInFlight();++i){
    auto cmd=device.commandSecondaryBuffer();

    land    .commitUbo(i);
    vobGroup.commitUbo(i);

    cmd.begin(mainPass);
    land.draw(cmd,pLand,i,gothic);
    for(auto& r:objStatic){
      vobGroup.setUniforms(cmd,pObject,i,r.obj);
      cmd.draw(r.obj.vbo(),r.obj.ibo());
      }
    cmd.end();

    cmdLand.emplace_back(std::move(cmd));
    }
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
