#include "landscape.h"

#include "rendererstorage.h"
#include "gothic.h"

using namespace Tempest;

Landscape::Landscape(const RendererStorage &storage, const ZenLoad::PackedMesh &mesh)
  :storage(storage) {
  auto& device=storage.device;

  pf.reset(new PerFrame[device.maxFramesInFlight()]);
  for(size_t i=0;i<device.maxFramesInFlight();++i){
    pf[i].uboGpu[0] = device.loadUbo(&uboCpu,sizeof(uboCpu));
    pf[i].uboGpu[1] = device.loadUbo(&uboCpu,sizeof(uboCpu));
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
    auto fa = a.texture->format();
    auto fb = b.texture->format();
    return std::tie(a.alpha,a.texture,fa)<std::tie(b.alpha,b.texture,fb);
    });
  }

void Landscape::setMatrix(uint32_t frameId, const Matrix4x4 &mat, const Matrix4x4 *sh, size_t shCount) {
  assert(shCount==2);

  uboCpu.mvp    = mat;
  uboCpu.shadow = sh[1];
  pf[frameId].uboGpu[1].update(&uboCpu,0,sizeof(uboCpu));

  uboCpu.mvp    = mat;
  uboCpu.shadow = sh[0];
  pf[frameId].uboGpu[0].update(&uboCpu,0,sizeof(uboCpu));
  }

void Landscape::setLight(const std::array<float,3> &l) {
  uboCpu.lightDir = {-l[0],-l[1],-l[2]};
  }

void Landscape::commitUbo(uint32_t frameId,const Tempest::Texture2d& shadowMap) {
  PerFrame& pf      = this->pf[frameId];
  auto&     uboLand = pf.ubo[0];
  auto&     uboSm0  = pf.ubo[1];
  auto&     uboSm1  = pf.ubo[2];

  const Texture2d* prev =nullptr;
  bool             alpha=false;

  uboLand.resize(blocks.size());
  uboSm0 .resize(blocks.size());
  uboSm1 .resize(blocks.size());

  for(size_t i=0;i<blocks.size();++i){
    auto& lnd  =blocks [i];
    auto& uboL =uboLand[i];
    auto& uboS0=uboSm0 [i];
    auto& uboS1=uboSm1 [i];

    if(prev==lnd.texture && alpha==lnd.alpha)
      continue; //HINT: usless :(
    prev  = lnd.texture;
    alpha = lnd.alpha;

    if(uboL.isEmpty())
      uboL = storage.device.uniforms(storage.uboLndLayout());
    if(uboS0.isEmpty())
      uboS0 = storage.device.uniforms(storage.uboLndLayout());
    if(uboS1.isEmpty())
      uboS1 = storage.device.uniforms(storage.uboLndLayout());

    uboL.set(0,pf.uboGpu[0],0,sizeof(uboCpu));
    uboL.set(2,*lnd.texture);
    uboL.set(3,shadowMap);

    uboS0.set(0,pf.uboGpu[0],0,sizeof(uboCpu));
    uboS0.set(2,*lnd.texture);
    uboS0.set(3,Resources::fallbackTexture());

    uboS1.set(0,pf.uboGpu[1],0,sizeof(uboCpu));
    uboS1.set(2,*lnd.texture);
    uboS1.set(3,Resources::fallbackTexture());
    }
  }

void Landscape::draw(Tempest::CommandBuffer &cmd, uint32_t frameId) {
  implDraw(cmd,storage.pLand,  storage.pLandAlpha,0,frameId);
  }

void Landscape::drawShadow(CommandBuffer &cmd, uint32_t frameId, int layer) {
  PerFrame& pf      = this->pf[frameId];
  auto&     uboLand = pf.ubo[1+layer];

  uint8_t tex=255;
  for(size_t i=0;i<blocks.size();++i){
    auto& lnd=blocks [i];
    auto& ubo=uboLand[i];

    if(ubo.isEmpty())
      continue;

    uint8_t tx = (lnd.texture && Tempest::TextureFormat::DXT1==lnd.texture->format()) ? 1 : 0;
    if(tx!=tex){
      tex = tx;
      uint32_t offset=0;
      cmd.setUniforms(storage.pLandSh,ubo,1,&offset);
      }
    cmd.draw(vbo,lnd.ibo);
    }
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
