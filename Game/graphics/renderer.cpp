#include "renderer.h"

#include "gothic.h"

using namespace Tempest;

Renderer::Renderer(Tempest::Device &device)
  :device(device) {
  mainPass = device.pass(FboMode::PreserveOut,FboMode::Discard);
  vsLand   = device.loadShader("shader/land.vert.sprv");
  fsLand   = device.loadShader("shader/land.frag.sprv");
  }

RenderPipeline& Renderer::landPipeline(RenderPass &pass,int w,int h) {
  RenderState             state;
  Tempest::UniformsLayout layout;

  if(pLand.w()!=uint32_t(w) || pLand.h()!=uint32_t(h))
    pLand = device.pipeline<Resources::LandVertex>(pass,uint32_t(w),uint32_t(h),Triangles,state,layout,vsLand,fsLand);
  return pLand;
  }

void Renderer::draw(CommandBuffer &cmd, const Widget &window, const Gothic &gothic) {
  auto& world = gothic.world();
  auto& pland = landPipeline(mainPass,window.w(),window.h());

  cmd.setUniforms(pland);
  cmd.draw(world.landVbo());
  }
