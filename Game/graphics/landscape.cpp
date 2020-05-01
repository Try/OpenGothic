#include "landscape.h"

#include "rendererstorage.h"
#include "graphics/submesh/packedmesh.h"
#include "gothic.h"

using namespace Tempest;

Landscape::Landscape(const RendererStorage &storage, const PackedMesh &mesh)
  :storage(storage) {
  auto& device=storage.device;

  pf.reset(new PerFrame[device.maxFramesInFlight()]);
  for(size_t i=0;i<device.maxFramesInFlight();++i){
    pf[i].uboGpu[0] = device.ubo(uboCpu);
    pf[i].uboGpu[1] = device.ubo(uboCpu);
    }

  static_assert(sizeof(Resources::Vertex)==sizeof(ZenLoad::WorldVertex),"invalid landscape vertex format");
  const Resources::Vertex* vert=reinterpret_cast<const Resources::Vertex*>(mesh.vertices.data());
  vbo = Resources::vbo<Resources::Vertex>(vert,mesh.vertices.size());

  std::vector<uint32_t> solids;

  for(auto& i:mesh.subMeshes){
    Block b={};
    b.material = Resources::loadMaterial(i.material);
    if(b.material.alpha==Material::AdditiveLight || i.indices.size()==0)
      continue;
    if(b.material.alpha==Material::InvalidAlpha) {
      Log::i("unrecognized alpha func: ",i.material.alphaFunc);
      continue;
      }

    if(b.material.alpha==Material::Solid) {
      solids.insert(solids.end(), i.indices.begin(),i.indices.end());
      }

    if(b.material.tex==nullptr || b.material.tex->isEmpty())
      continue;
    b.ibo = Resources::ibo(i.indices.data(),i.indices.size());
    blocks.emplace_back(std::move(b));
    }

  iboSolid = Resources::ibo(solids.data(),solids.size());

  std::sort(blocks.begin(),blocks.end(),[](const Block& a,const Block& b){
    return a.material<b.material;
    });
  }

void Landscape::setMatrix(uint32_t frameId, const Matrix4x4 &mat, const Matrix4x4 *sh, size_t shCount) {
  assert(shCount==2);

  uboCpu.mvp    = mat;
  uboCpu.shadow = sh[1];
  pf[frameId].uboGpu[1].update(&uboCpu,0,1);

  uboCpu.mvp    = mat;
  uboCpu.shadow = sh[0];
  pf[frameId].uboGpu[0].update(&uboCpu,0,1);
  }

void Landscape::setLight(const Light &l, const Vec3 &ambient) {
  auto  d = l.dir();
  auto& c = l.color();
  uboCpu.lightDir = {-d[0],-d[1],-d[2]};
  uboCpu.lightCl  = {c.x,c.y,c.z,0.f};
  uboCpu.lightAmb = {ambient.x,ambient.y,ambient.z,0.f};
  }

void Landscape::invalidateCmd() {
  for(size_t i=0;i<storage.device.maxFramesInFlight();++i)
    pf[i].nToUpdate = true;
  }

bool Landscape::needToUpdateCommands(uint8_t frameId) const {
  return pf[frameId].nToUpdate;
  }

void Landscape::setAsUpdated(uint8_t frameId) const {
  pf[frameId].nToUpdate = false;
  }

void Landscape::commitUbo(uint8_t frameId, const Tempest::Texture2d& shadowMap) {
  PerFrame& pf      = this->pf[frameId];
  if(!pf.nToUpdate)
    return;

  auto&     uboLand = pf.ubo[0];
  auto&     uboSm0  = pf.ubo[1];
  auto&     uboSm1  = pf.ubo[2];

  uboLand.resize(blocks.size());
  uboSm0 .resize(blocks.size());
  uboSm1 .resize(blocks.size());

  for(size_t i=0;i<blocks.size();++i){
    auto& lnd  =blocks [i];
    auto& uboL =uboLand[i];
    auto& uboS0=uboSm0 [i];
    auto& uboS1=uboSm1 [i];

    if(uboL.isEmpty())
      uboL  = storage.device.uniforms(storage.uboLndLayout());
    if(uboS0.isEmpty())
      uboS0 = storage.device.uniforms(storage.uboLndLayout());
    if(uboS1.isEmpty())
      uboS1 = storage.device.uniforms(storage.uboLndLayout());

    uboL.set(0,pf.uboGpu[0],0,1);
    uboL.set(2,*lnd.material.tex);
    uboL.set(3,shadowMap);

    uboS0.set(0,pf.uboGpu[0],0,1);
    uboS0.set(2,*lnd.material.tex);
    uboS0.set(3,Resources::fallbackTexture());

    uboS1.set(0,pf.uboGpu[1],0,1);
    uboS1.set(2,*lnd.material.tex);
    uboS1.set(3,Resources::fallbackTexture());

    for(int r=0;r<2;++r) {
      if(pf.solidSh[r].isEmpty())
        pf.solidSh[r] = storage.device.uniforms(storage.uboLndLayout());
      pf.solidSh[r].set(0,pf.uboGpu[r],0,1);
      pf.solidSh[r].set(2,Resources::fallbackBlack());
      pf.solidSh[r].set(3,Resources::fallbackTexture());
      }
    }
  }

void Landscape::draw(Tempest::Encoder<CommandBuffer> &cmd, uint8_t frameId) {
  const RenderPipeline* ptable[Material::ApphaFunc::Last] = {};
  ptable[Material::ApphaFunc::AlphaTest  ] = &storage.pLandAt;
  ptable[Material::ApphaFunc::Transparent] = &storage.pLandAlpha;
  ptable[Material::ApphaFunc::Solid      ] = &storage.pLand;

  implDraw(cmd,ptable,0,frameId);
  }

void Landscape::drawShadow(Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId, int layer) {
  const RenderPipeline* ptable[Material::ApphaFunc::Last] = {};
  ptable[Material::ApphaFunc::AlphaTest  ] = &storage.pLandAtSh;
  ptable[Material::ApphaFunc::Transparent] = &storage.pLandSh;
  // ptable[Material::ApphaFunc::Solid      ] = &storage.pLandSh;

  uint32_t offset=0;
  cmd.setUniforms(storage.pLandSh,pf[frameId].solidSh[layer],1,&offset);
  cmd.draw(vbo,iboSolid);

  implDraw(cmd,ptable,uint8_t(1+layer),frameId);
  }

void Landscape::implDraw(Tempest::Encoder<Tempest::CommandBuffer> &cmd,
                         const RenderPipeline* p[],
                         uint8_t uboId, uint8_t frameId) {
  PerFrame& pf      = this->pf[frameId];
  auto&     uboLand = pf.ubo[uboId];

  for(size_t i=0;i<blocks.size();++i){
    auto& lnd=blocks [i];
    auto& ubo=uboLand[i];

    if(ubo.isEmpty())
      continue;

    auto pso = p[lnd.material.alpha];
    if(pso==nullptr)
      continue;

    uint32_t offset=0;
    cmd.setUniforms(*pso,ubo,1,&offset);
    cmd.draw(vbo,lnd.ibo);
    }
  }
