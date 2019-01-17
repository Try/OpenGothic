#include "staticobjects.h"

StaticObjects::StaticObjects(Tempest::Device& device)
  :device(device) {
  layout.add(0,Tempest::UniformsLayout::UboDyn, Tempest::UniformsLayout::Vertex);
  layout.add(1,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);
  }

StaticObjects::Obj StaticObjects::get(const Tempest::Texture2d *mat, const StaticMesh &mesh, const Tempest::IndexBuffer<uint32_t>& ibo) {
  for(auto& i:chunks)
    if(i.tex==mat)
      return get(i,mesh,ibo);

  chunks.emplace_back(Chunk(mat,device.caps().minUboAligment));
  auto& last = chunks.back();
  last.pf.reset(new PerFrame[device.maxFramesInFlight()]);

  for(size_t i=0;i<device.maxFramesInFlight();++i)
    last.pf[i].ubo = device.uniforms(layout);
  return get(last,mesh,ibo);
  }

void StaticObjects::commitUbo(uint32_t imgId) {
  for(auto& i:chunks){
    auto& frame=i.pf[imgId];
    size_t sz = i.obj.byteSize();
    if(frame.uboData.size()!=sz)
      frame.uboData = device.loadUbo(i.obj.data(),sz); else
      frame.uboData.update(i.obj.data(),0,sz);

    frame.ubo.set(0,frame.uboData,0,i.obj.elementSize());
    frame.ubo.set(1,*i.tex);
    }
  }

void StaticObjects::setUniforms(Tempest::CommandBuffer &cmd,Tempest::RenderPipeline& pipe,
                                uint32_t imgId,const StaticObjects::Obj &obj) {
  auto& frame=obj.owner->pf[imgId];

  uint32_t offset=obj.id*obj.owner->obj.elementSize();
  cmd.setUniforms(pipe,frame.ubo,1,&offset);
  }

StaticObjects::Obj StaticObjects::get(StaticObjects::Chunk &owner,const StaticMesh& mesh, const Tempest::IndexBuffer<uint32_t>& ibo) {
  owner.obj.resize(owner.obj.size()+1);
  return Obj(owner,owner.obj.size()-1,&mesh,&ibo);
  }

void StaticObjects::Obj::setMatrix(const Tempest::Matrix4x4 &mt) {
  owner->obj[id].mat=mt;
  }
