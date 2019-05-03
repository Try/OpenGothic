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

    Block b={};
    b.ibo     = Resources::loadIbo(i.indices.data(),i.indices.size());
    b.texture = Resources::loadTexture(i.material.texture);

    if(i.material.alphaFunc==Resources::Transparent)
      b.alpha=true;
    if(i.material.alphaFunc!=Resources::NoAlpha &&
       i.material.alphaFunc!=Resources::Transparent)
      Log::i("unrecognized alpha func: ",i.material.alphaFunc);

    if(b.texture==nullptr || b.texture->isEmpty())
      continue;
    blocks.emplace_back(std::move(b));
    }

  std::sort(blocks.begin(),blocks.end(),[](const Block& a,const Block& b){
    return std::tie(a.alpha,a.texture)<std::tie(b.alpha,b.texture);
    });
  }

void Landscape::setMatrix(uint32_t frameId, const Matrix4x4 &mat, const Matrix4x4 &sh) {
  uboCpu.mvp    = mat;
  uboCpu.shadow = sh;
  pf[frameId].uboGpu.update(&uboCpu,0,sizeof(uboCpu));
  }

void Landscape::setLight(const std::array<float,3> &l) {
  uboCpu.lightDir = {-l[0],-l[1],-l[2]};
  }

void Landscape::commitUbo(uint32_t frameId,const Tempest::Texture2d& shadowMap) {
  PerFrame& pf      = this->pf[frameId];
  auto&     uboLand = pf.ubo[0];
  auto&     uboSm   = pf.ubo[1];

  const Texture2d* prev =nullptr;
  bool             alpha=false;

  uboLand.resize(blocks.size());
  uboSm  .resize(blocks.size());

  for(size_t i=0;i<blocks.size();++i){
    auto& lnd =blocks [i];
    auto& uboL=uboLand[i];
    auto& uboS=uboSm  [i];

    if(prev==lnd.texture && alpha==lnd.alpha)
      continue;
    if(uboL.isEmpty())
      uboL = storage.device.uniforms(storage.uboLndLayout());
    if(uboS.isEmpty())
      uboS = storage.device.uniforms(storage.uboLndLayout());

    uboL.set(0,pf.uboGpu,0,sizeof(uboCpu));
    uboL.set(2,*lnd.texture);
    uboL.set(3,shadowMap);

    uboS.set(0,pf.uboGpu,0,sizeof(uboCpu));
    uboS.set(2,*lnd.texture);
    uboS.set(3,Resources::fallbackTexture());
    }
  }

void Landscape::draw(Tempest::CommandBuffer &cmd, uint32_t frameId) {
  implDraw(cmd,storage.pLand,  storage.pLandAlpha,0,frameId);
  }

void Landscape::drawShadow(CommandBuffer &cmd, uint32_t frameId) {
  implDraw(cmd,storage.pLandSh,storage.pLandSh,   1,frameId);
  }

void Landscape::implDraw(CommandBuffer &cmd,const RenderPipeline &p,const RenderPipeline &alpha,uint8_t uboId,uint32_t frameId) {
  PerFrame& pf      = this->pf[frameId];
  auto&     uboLand = pf.ubo[uboId];

  for(size_t i=0;i<blocks.size();++i){
    auto& lnd=blocks [i];
    auto& ubo=uboLand[i];

    if(!ubo.isEmpty()){
      uint32_t offset=0;
      if(lnd.alpha)
        cmd.setUniforms(alpha,ubo,1,&offset); else
        cmd.setUniforms(p,ubo,1,&offset);
      }
    cmd.draw(vbo,lnd.ibo);
    }
  }
