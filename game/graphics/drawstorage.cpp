#include "drawstorage.h"

#include "graphics/mesh/submesh/packedmesh.h"
#include "shaders.h"

using namespace Tempest;

void DrawStorage::Item::setObjMatrix(const Tempest::Matrix4x4& mt) {
  }

uint32_t DrawStorage::Item::commandId() const {
  return owner->objects[id].cmdId;
  }

uint32_t DrawStorage::Item::bucketId() const {
  return owner->objects[id].bucketId;
  }


DrawStorage::DrawStorage(const SceneGlobals& globals) : globals(globals) {
  }

DrawStorage::~DrawStorage() {
  }

DrawStorage::Item DrawStorage::alloc(const StaticMesh& mesh, const Material& mat,
                                     size_t iboOff, size_t iboLen, const Tempest::StorageBuffer& desc, Type bucket) {
  // 64x64 meshlets
  assert(iboOff%PackedMesh::MaxInd==0);
  assert(iboLen%PackedMesh::MaxInd==0);
  commited = false;

  size_t id = objects.size();
  for(size_t i=0; i<objects.size(); ++i) {
    if(objects[i].isEmpty()) {
      id = i;
      break;
      }
    }

  if(id==objects.size()) {
    objects.resize(objects.size()+1);
    }

  Object& obj = objects[id];
  obj.iboOff     = uint32_t(iboOff);
  obj.iboLen     = uint32_t(iboLen);
  obj.bucketId   = bucketId(mat, mesh);
  obj.cmdId      = commandId(mat, &desc, obj.bucketId);

  cmd[obj.cmdId].maxClusters += (obj.iboLen/PackedMesh::MaxInd);
  return Item(*this, id);
  }

void DrawStorage::free(size_t id) {
  commited = false;

  Object& obj = objects[id];
  cmd[obj.cmdId].maxClusters -= (obj.iboLen/PackedMesh::MaxInd);
  obj.cmdId = uint32_t(-1);

  while(objects.size()>0) {
    if(!objects.back().isEmpty())
      break;
    objects.pop_back();
    }
  }

void DrawStorage::commit() {
  if(commited)
    return;
  commited = true;

  if(cmd.size()==0)
    return;

  uint32_t firstCluster = 0;
  for(auto& i:cmd) {
    i.firstCluster = firstCluster;
    firstCluster += i.maxClusters;
    }

  uint32_t writeOffset = 0;
  std::vector<IndirectCmd> cx(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i) {
    cx[i].vertexCount  = PackedMesh::MaxInd;
    cx[i].writeOffset  = writeOffset;
    writeOffset       += cmd[i].maxClusters;
    }

  cmdOrder.resize(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i)
    cmdOrder[i] = &cmd[i];
  std::sort(cmdOrder.begin(), cmdOrder.end(), [](const DrawCmd* l, const DrawCmd* r){
    return l->alpha < r->alpha;
    });

  auto& device = Resources::device();
  device.waitIdle();

  clusterTotal = uint32_t((cmd[0].clusters->byteSize())/sizeof(PackedMesh::Cluster));
  visClusters  = device.ssbo(nullptr, clusterTotal*sizeof(uint32_t));
  indirectCmd  = device.ssbo(cx.data(), sizeof(IndirectCmd)*cx.size());

  descInit = device.descriptors(Shaders::inst().clusterInit);
  descInit.set(L_Bucket,  indirectCmd);
  descInit.set(L_Payload, visClusters);

  descTask = device.descriptors(Shaders::inst().clusterTask);
  descTask.set(L_MeshDesc, *cmd[0].clusters); // FIXME
  descTask.set(L_Bucket,   indirectCmd);
  descTask.set(L_Payload,  visClusters);
  }

void DrawStorage::prepareUniforms() {
  if(cmd.size()==0)
    return;

  commit();
  descTask.set(L_Scene,  globals.uboGlobal[SceneGlobals::V_Main]);
  descTask.set(L_HiZ,   *globals.hiZ);

  std::vector<const Tempest::Texture2d*> tex;
  for(auto& i:buckets) {
    tex.push_back(i.mat.tex);
    }

  for(auto& i:cmd) {
    i.desc.set(L_Scene,    globals.uboGlobal[SceneGlobals::V_Main]);
    i.desc.set(L_MeshDesc, *i.clusters);                 // landscape only
    i.desc.set(L_Ibo,      buckets[0].staticMesh->ibo8); // FIXME
    i.desc.set(L_Vbo,      buckets[0].staticMesh->vbo);
    i.desc.set(L_Diffuse,  tex);
    i.desc.set(L_Bucket,   indirectCmd);
    i.desc.set(L_Payload,  visClusters);
    i.desc.set(L_Sampler,  Sampler::anisotrophy());
    }
  }

void DrawStorage::visibilityPass(Encoder<CommandBuffer>& cmd, uint8_t frameId) {
  if(this->cmd.size()==0)
    return;
  cmd.setUniforms(Shaders::inst().clusterInit, descInit);
  cmd.dispatchThreads(this->cmd.size());

  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};
  push.firstMeshlet = 0;
  push.meshletCount = clusterTotal;

  cmd.setUniforms(Shaders::inst().clusterTask, descTask, &push, sizeof(push));
  cmd.dispatchThreads(clusterTotal);
  }

void DrawStorage::drawGBuffer(Encoder<CommandBuffer>& cmd, uint8_t frameId) {
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {0, clusterTotal};

  for(size_t i=0; i<cmdOrder.size(); ++i) {
    auto& cx = *cmdOrder[i];
    auto id  = size_t(std::distance(this->cmd.data(), &cx));
    push.firstMeshlet = cx.firstCluster;
    push.meshletCount = cx.maxClusters;

    cmd.setUniforms(*cx.pso, cx.desc, &push, sizeof(push));
    cmd.drawIndirect(indirectCmd, sizeof(IndirectCmd)*id);
    }
  }

void DrawStorage::drawShadow(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId, int layer) {

  }

const RenderPipeline* DrawStorage::pipeline(const Material& m) {
  switch (m.alpha) {
    case Material::Solid:
      return &Shaders::inst().clusterGBuf;
    case Material::AlphaTest:
      return &Shaders::inst().clusterGBufAt;
    case Material::Water:
    case Material::Ghost:
    case Material::Multiply:
    case Material::Multiply2:
    case Material::Transparent:
    case Material::AdditiveLight:
      break;
    }
  return nullptr;
  }

uint32_t DrawStorage::commandId(const Material& m, const Tempest::StorageBuffer* clusters, uint32_t bucketId) {
  auto pMain = pipeline(m);
  if(pMain==nullptr)
    return uint32_t(-1);

  const bool bindless = true;

  for(size_t i=0; i<cmd.size(); ++i) {
    if(cmd[i].pso!=pMain)
      continue;
    if(cmd[i].clusters!=clusters)
      continue;
    if(!bindless && cmd[i].bucketId != bucketId)
      continue;
    return uint32_t(i);
    }

  auto ret = uint32_t(cmd.size());

  auto& device = Resources::device();
  DrawCmd cx;
  cx.pso         = pMain;
  cx.clusters    = clusters;
  cx.bucketId    = bucketId;
  cx.desc        = device.descriptors(*cx.pso);
  cx.maxClusters = 0;
  cx.alpha       = m.alpha;
  cmd.push_back(std::move(cx));
  return ret;
  }

uint32_t DrawStorage::bucketId(const Material& mat, const StaticMesh& mesh) {
  for(size_t i=0; i<buckets.size(); ++i) {
    auto& b = buckets[i];
    if(b.staticMesh==&mesh && b.mat==mat)
      return uint32_t(i);
    }

  Bucket bx;
  bx.staticMesh = &mesh;
  bx.mat        = mat;
  buckets.emplace_back(std::move(bx));
  return uint32_t(buckets.size()-1);
  }
