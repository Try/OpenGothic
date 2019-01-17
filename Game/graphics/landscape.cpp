#include "landscape.h"

#include "gothic.h"

using namespace Tempest;

Landscape::Landscape(Tempest::Device &device)
  :device(device) {
  layout.add(0,Tempest::UniformsLayout::UboDyn, Tempest::UniformsLayout::Vertex);
  layout.add(1,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);

  landPF.reset(new PerFrame[device.maxFramesInFlight()]);
  for(size_t i=0;i<device.maxFramesInFlight();++i){
    landPF[i].uboGpu = device.loadUbo(&uboCpu,sizeof(uboCpu));
    }
  }

void Landscape::commitUbo(const Tempest::Matrix4x4 &mat, uint32_t imgId) {
  uboCpu.mvp = mat;
  landPF[imgId].uboGpu.update(&uboCpu,0,sizeof(uboCpu));
  }

void Landscape::draw(Tempest::CommandBuffer &cmd, const RenderPipeline& pLand, uint32_t imgId,const Gothic& gothic) {
  auto& world = gothic.world();

  PerFrame& pf      = landPF[imgId];
  auto&     uboLand = pf.uboLand;

  uboLand.resize(world.landBlocks().size());
  const Texture2d* prev=nullptr;
  for(size_t i=0;i<world.landBlocks().size();++i){
    auto& lnd=world.landBlocks()[i];
    auto& ubo=uboLand[i];

    if(ubo.isEmpty())
      ubo = device.uniforms(layout);
    if(!lnd.texture || lnd.texture->isEmpty())
      continue;

    if(lnd.texture!=prev) {
      prev=lnd.texture;
      ubo.set(0,pf.uboGpu,0,sizeof(uboCpu));
      ubo.set(1,*lnd.texture);

      uint32_t offset=0;
      cmd.setUniforms(pLand,ubo,1,&offset);
      }
    cmd.draw(world.landVbo(),lnd.ibo);
    }
  }
