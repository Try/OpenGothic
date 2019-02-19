#include "landscape.h"

#include "rendererstorage.h"
#include "gothic.h"

using namespace Tempest;

Landscape::Landscape(const RendererStorage &storage, const ZenLoad::PackedMesh &mesh)
  :storage(storage) {
  auto& device=storage.device;

  pf.reset(new PerFrame[device.maxFramesInFlight()]);
  for(size_t i=0;i<device.maxFramesInFlight();++i){
    pf[i].uboGpu = device.loadUbo(&uboCpu,sizeof(uboCpu));
    }

  static_assert(sizeof(Resources::Vertex)==sizeof(ZenLoad::WorldVertex),"invalid landscape vertex format");
  const Resources::Vertex* vert=reinterpret_cast<const Resources::Vertex*>(mesh.vertices.data());
  vbo = Resources::loadVbo<Resources::Vertex>(vert,mesh.vertices.size());

  for(auto& i:mesh.subMeshes){
    if(i.material.alphaFunc==Resources::AdditiveLight)
      continue;

    blocks.emplace_back();
    Block& b = blocks.back();
    if(i.material.alphaFunc==Resources::Transparent)
      b.alpha=true;
    if(i.material.alphaFunc!=Resources::NoAlpha &&
       i.material.alphaFunc!=Resources::Transparent)
      Log::i("unrecognized alpha func: ",i.material.alphaFunc);

    b.ibo     = Resources::loadIbo(i.indices.data(),i.indices.size());
    b.texture = Resources::loadTexture(i.material.texture);
    }

  std::sort(blocks.begin(),blocks.end(),[](const Block& a,const Block& b){
    return std::tie(a.alpha,a.texture)<std::tie(b.alpha,b.texture);
    });
  }

void Landscape::setMatrix(uint32_t frameId, const Matrix4x4 &mat) {
  uboCpu.mvp = mat;
  pf[frameId].uboGpu.update(&uboCpu,0,sizeof(uboCpu));
  }

void Landscape::commitUbo(uint32_t /*frameId*/) {
  }

void Landscape::draw(Tempest::CommandBuffer &cmd, uint32_t frameId) {
  PerFrame& pf      = this->pf[frameId];
  auto&     uboLand = pf.uboLand;

  uboLand.resize(blocks.size());
  const Texture2d* prev=nullptr;
  for(size_t i=0;i<blocks.size();++i){
    auto& lnd=blocks [i];
    auto& ubo=uboLand[i];

    if(ubo.isEmpty())
      ubo = storage.device.uniforms(storage.uboLndLayout());
    if(!lnd.texture || lnd.texture->isEmpty())
      continue;

    if(lnd.texture!=prev) {
      prev=lnd.texture;
      ubo.set(0,pf.uboGpu,0,sizeof(uboCpu));
      ubo.set(1,*lnd.texture);

      uint32_t offset=0;
      if(lnd.alpha)
        cmd.setUniforms(storage.pLandAlpha,ubo,1,&offset); else
        cmd.setUniforms(storage.pLand,ubo,1,&offset);
      }
    cmd.draw(vbo,lnd.ibo);
    }
  }
