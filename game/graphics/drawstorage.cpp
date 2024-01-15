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
  obj.iboOff   = uint32_t(iboOff);
  obj.iboLen   = uint32_t(iboLen);
  obj.bucketId = bucketId(mat, mesh);
  obj.cmdId    = commandId(mat, &desc, obj.bucketId);

  cmd[obj.cmdId].maxClusters += (obj.iboLen/PackedMesh::MaxInd);
  return Item(*this, id);
  }

void DrawStorage::free(size_t id) {
  commited = false;

  Object& obj = objects[id];
  cmd[obj.cmdId].maxClusters -= (obj.iboLen/PackedMesh::MaxInd);

  obj = Object();
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

  ord.resize(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i)
    ord[i] = &cmd[i];
  std::sort(ord.begin(), ord.end(), [](const DrawCmd* l, const DrawCmd* r){
    return l->alpha < r->alpha;
  });

  auto& device = Resources::device();
  device.waitIdle();

  clusterTotal = 0;
  tasks.clear();
  for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
    for(auto& i:cmd) {
      bool exists = false;
      for(auto& r:tasks)
        if(r.clusters==i.clusters && r.viewport==SceneGlobals::VisCamera(v)) {
          exists = true;
          break;
          }
      if(exists)
        continue;

      TaskCmd cmd;
      cmd.viewport  = SceneGlobals::VisCamera(v);
      cmd.clusters  = i.clusters;
      clusterTotal += uint32_t(cmd.clusters->byteSize()/sizeof(PackedMesh::Cluster));
      tasks.emplace_back(std::move(cmd));
      }
    }

  for(auto& v:views) {
    if(clusterTotal==0)
      continue;
    v.visClusters  = device.ssbo(nullptr, clusterTotal*sizeof(uint32_t));
    v.indirectCmd  = device.ssbo(cx.data(), sizeof(IndirectCmd)*cx.size());

    v.descInit = device.descriptors(Shaders::inst().clusterInit);
    v.descInit.set(L_Bucket,  v.indirectCmd);
    v.descInit.set(L_Payload, v.visClusters);
    }

  for(auto& cmd:tasks) {
    if(cmd.viewport==SceneGlobals::V_Main)
      cmd.desc = device.descriptors(Shaders::inst().clusterTaskHiZ); else
      cmd.desc = device.descriptors(Shaders::inst().clusterTask);
    cmd.desc.set(L_MeshDesc, *cmd.clusters);
    cmd.desc.set(L_Bucket,   views[cmd.viewport].indirectCmd);
    cmd.desc.set(L_Payload,  views[cmd.viewport].visClusters);
    }
  }

void DrawStorage::prepareUniforms() {
  if(cmd.size()==0)
    return;

  commit();

  std::vector<const Tempest::Texture2d*> tex;
  for(auto& i:buckets) {
    tex.push_back(i.mat.tex);
    }

  for(auto& i:tasks) {
    i.desc.set(L_Scene,  globals.uboGlobal[i.viewport]);
    //if(i.viewport==SceneGlobals::V_Main)
    i.desc.set(L_HiZ,   *globals.hiZ);
    }

  for(auto& i:cmd) {
    for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
      i.desc[v].set(L_Scene,    globals.uboGlobal[v]);
      i.desc[v].set(L_MeshDesc, *i.clusters);                 // landscape only
      i.desc[v].set(L_Ibo,      buckets[0].staticMesh->ibo8); // FIXME
      i.desc[v].set(L_Vbo,      buckets[0].staticMesh->vbo);
      i.desc[v].set(L_Diffuse,  tex);
      i.desc[v].set(L_Bucket,   views[v].indirectCmd);
      i.desc[v].set(L_Payload,  views[v].visClusters);
      i.desc[v].set(L_Sampler,  Sampler::anisotrophy());
      }
    }
  }

void DrawStorage::visibilityPass(Encoder<CommandBuffer>& cmd, uint8_t frameId) {
  for(auto& v:views) {
    // if(v.clusterTotal==0)
    //   continue;
    cmd.setUniforms(Shaders::inst().clusterInit, v.descInit);
    cmd.dispatchThreads(this->cmd.size());
    }

  for(auto& i:tasks) {
    struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};
    push.firstMeshlet = 0;
    push.meshletCount = uint32_t((i.clusters->byteSize())/sizeof(PackedMesh::Cluster));

    cmd.setUniforms(Shaders::inst().clusterTask, i.desc, &push, sizeof(push));
    cmd.dispatchThreads(push.meshletCount);
    }
  }

void DrawStorage::drawGBuffer(Encoder<CommandBuffer>& cmd, uint8_t frameId) {
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};

  auto& main = views[SceneGlobals::V_Main];
  for(size_t i=0; i<ord.size(); ++i) {
    auto& cx = *ord[i];
    auto id  = size_t(std::distance(this->cmd.data(), &cx));
    push.firstMeshlet = cx.firstCluster;
    push.meshletCount = cx.maxClusters;

    cmd.setUniforms(*cx.psoColor, cx.desc[SceneGlobals::V_Main], &push, sizeof(push));
    cmd.drawIndirect(main.indirectCmd, sizeof(IndirectCmd)*id);
    }
  }

void DrawStorage::drawShadow(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, int layer) {
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};

  auto  viewId = (SceneGlobals::V_Shadow0+layer);
  auto& view   = views[viewId];
  for(size_t i=0; i<ord.size(); ++i) {
    auto& cx = *ord[i];
    auto id  = size_t(std::distance(this->cmd.data(), &cx));
    push.firstMeshlet = cx.firstCluster;
    push.meshletCount = cx.maxClusters;

    cmd.setUniforms(*cx.psoDepth, cx.desc[viewId], &push, sizeof(push));
    cmd.drawIndirect(view.indirectCmd, sizeof(IndirectCmd)*id);
    }
  }

const RenderPipeline* DrawStorage::pipelineColor(const Material& m) {
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

const RenderPipeline* DrawStorage::pipelineDepth(const Material& m) {
  switch (m.alpha) {
    case Material::Solid:
      return &Shaders::inst().clusterDepth;
    case Material::AlphaTest:
      return &Shaders::inst().clusterDepthAt;
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

uint32_t DrawStorage::commandId(const Material& m, const Tempest::StorageBuffer* clusters, uint32_t bucketId) {
  auto pMain  = pipelineColor(m);
  auto pDepth = pipelineDepth(m);
  if(pMain==nullptr && pDepth==nullptr)
    return uint32_t(-1);

  const bool bindless = true;

  for(size_t i=0; i<cmd.size(); ++i) {
    if(cmd[i].psoColor!=pMain || cmd[i].psoDepth!=pDepth)
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
  cx.psoColor    = pMain;
  cx.psoDepth    = pDepth;
  cx.clusters    = clusters;
  cx.bucketId    = bucketId;
  cx.maxClusters = 0;
  cx.alpha       = m.alpha;
  if(cx.psoColor!=nullptr) {
    cx.desc[SceneGlobals::V_Main] = device.descriptors(*cx.psoColor);
    }
  if(cx.psoDepth!=nullptr) {
    cx.desc[SceneGlobals::V_Shadow0] = device.descriptors(*cx.psoDepth);
    cx.desc[SceneGlobals::V_Shadow1] = device.descriptors(*cx.psoDepth);
    }
  cmd.push_back(std::move(cx));
  return ret;
  }

