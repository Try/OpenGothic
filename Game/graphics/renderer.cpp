#include "renderer.h"

#include <Tempest/Color>
#include "gothic.h"

using namespace Tempest;

Renderer::Renderer(Tempest::Device &device)
  :device(device) {
  vsLand   = device.loadShader("shader/land.vert.sprv");
  fsLand   = device.loadShader("shader/land.frag.sprv");

  layout.add(0,Tempest::UniformsLayout::Ubo,    Tempest::UniformsLayout::Vertex);
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
    pLand = device.pipeline<Resources::LandVertex>(pass,w,h,Triangles,state,layout,vsLand,fsLand);
  return pLand;
  }

void Renderer::setDebugView(const PointF &spin, const float zoom) {
  this->spin=spin;
  this->zoom=zoom;
  }

void Renderer::setupShaderConstants(const FrameBuffer &window) {
  Matrix4x4 projective, view;

  projective.perspective( 45.0f, float(window.w())/float(window.h()), 0.1f, 100.0f );

  view.identity();
  view.translate(0,0,4);
  view.rotate(spin.y, 1, 0, 0);
  view.rotate(spin.x, 0, 1, 0);
  view.scale(zoom);

  uboCpu.mvp = projective;
  uboCpu.mvp.mul(view);
  //ubo.xtexture = texture;
  }

void Renderer::draw(CommandBuffer &cmd, uint32_t imgId, const Gothic &gothic) {
  const uint8_t frameId = device.frameId();
  FrameBuffer&  fbo     = fbo3d[imgId];

  setupShaderConstants(fbo);

  auto& world = gothic.world();
  auto& pland = landPipeline(mainPass,fbo.w(),fbo.h());

  uboGpu[frameId] = device.loadUbo(&uboCpu,sizeof(uboCpu));

  cmd.beginRenderPass(fbo,mainPass);

  uboArr.resize(world.landBlocks().size());
  for(size_t i=0;i<world.landBlocks().size();++i){
    auto& lnd=world.landBlocks()[i];
    auto& ubo=uboArr[i];

    if(ubo.isEmpty())
      ubo = device.uniforms(layout);
    if(!lnd.texture || lnd.texture->isEmpty())
      continue;

    ubo.set(0,uboGpu[device.frameId()],0,sizeof(uboCpu));
    ubo.set(1,*lnd.texture);

    cmd.setUniforms(pland,ubo);
    cmd.draw(world.landVbo(),lnd.ibo);
    }

  cmd.endRenderPass();
  }
