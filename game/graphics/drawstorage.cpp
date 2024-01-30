#include "drawstorage.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/visualobjects.h"
#include "shaders.h"

#include "gothic.h"

using namespace Tempest;

template<class T>
static const T& dummy() {
  static T t = {};
  return t;
  }

static RtScene::Category toRtCategory(DrawStorage::Type t) {
  switch (t) {
    case DrawStorage::Landscape: return RtScene::Landscape;
    case DrawStorage::Static:    return RtScene::Static;
    case DrawStorage::Movable:   return RtScene::Movable;
    case DrawStorage::Animated:  return RtScene::None;
    case DrawStorage::Pfx:       return RtScene::None;
    case DrawStorage::Morph:     return RtScene::None;
    }
  return RtScene::None;
  }

template<class T>
size_t DrawStorage::FreeList<T>::alloc(size_t count) {
  size_t bestFit = size_t(-1), bfDiff = size_t(-1);
  for(size_t i=0; i<freeList.size(); ++i) {
    auto f  = freeList[i];
    auto sz = (f.end - f.begin);
    if(sz==count) {
      freeList.erase(freeList.begin()+int(i));
      return f.begin;
      }
    if(sz<count)
      continue;
    if(sz-count < bfDiff)
      bestFit = i;
    }

  if(bestFit != size_t(-1)) {
    auto& f = freeList[bestFit];
    f.end -= count;
    return f.end;
    }

  size_t ret = data.size();
  data.resize(data.size() + count);
  return ret;
  }

template<class T>
void DrawStorage::FreeList<T>::free(size_t id, size_t count) {
  Range r = {id, id+count};
  auto at = std::lower_bound(freeList.begin(),freeList.end(),r,[](const Range& l, const Range& r){
    return l.begin<r.begin;
    });
  at = freeList.insert(at,r);
  auto next = at+1;
  if(next!=freeList.end() && at->end==next->begin) {
    next->begin = at->begin;
    at = freeList.erase(at);
    }
  if(at!=freeList.begin() && at->begin==(at-1)->end) {
    auto prev = (at-1);
    prev->end = at->end;
    at = freeList.erase(at);
    }
  }

void DrawStorage::Item::setObjMatrix(const Tempest::Matrix4x4& pos) {
  if(owner!=nullptr) {
    auto& obj = owner->objects[id];
    if(obj.pos!=pos)
      owner->updateRtAs(id);
    obj.pos = pos;
    owner->updateInstance(id);
    }
  }

void DrawStorage::Item::setAsGhost(bool g) {
  if(owner!=nullptr) {
    owner->setAsGhost(id, g);
    }
  }

void DrawStorage::Item::setFatness(float f) {
  if(owner!=nullptr) {
    owner->objects[id].fatness = f;
    owner->updateInstance(id);
    }
  }

void DrawStorage::Item::setWind(phoenix::animation_mode m, float intensity) {
  if(owner==nullptr)
    return;

  if(intensity!=0 && m==phoenix::animation_mode::none) {
    m = phoenix::animation_mode::wind2;
    }

  auto& obj = owner->objects[id];

  const auto prev = obj.wind;
  obj.wind          = m;
  obj.windIntensity = intensity;

  if(prev==m)
    return;

  if(prev!=phoenix::animation_mode::none)
    owner->objectsWind.erase(id);
  if(m!=phoenix::animation_mode::none)
    owner->objectsWind.insert(id);
  }

void DrawStorage::Item::startMMAnim(std::string_view anim, float intensity, uint64_t timeUntil) {
  if(owner!=nullptr)
    owner->startMMAnim(id, anim, intensity, timeUntil);
  }

const Material& DrawStorage::Item::material() const {
  if(owner==nullptr)
    return dummy<Material>();
  auto b = owner->objects[id].bucketId;
  return owner->buckets[b].mat;
  }

const Bounds& DrawStorage::Item::bounds() const {
  if(owner==nullptr)
    return dummy<Bounds>();
  auto  bId = owner->objects[id].bucketId;
  auto& bx  = owner->buckets[bId];
  if(bx.staticMesh!=nullptr)
    return bx.staticMesh->bbox;
  if(bx.animMesh!=nullptr)
    return bx.animMesh->bbox;
  return dummy<Bounds>();
  }

Matrix4x4 DrawStorage::Item::position() const {
  if(owner!=nullptr)
    return owner->objects[id].pos;
  return dummy<Matrix4x4>();
  }

const StaticMesh* DrawStorage::Item::mesh() const {
  if(owner==nullptr)
    return nullptr;
  auto b = owner->objects[id].bucketId;
  return owner->buckets[b].staticMesh;
  }

std::pair<uint32_t, uint32_t> DrawStorage::Item::meshSlice() const {
  if(owner==nullptr)
    return std::pair<uint32_t, uint32_t>(0,0);
  auto& obj = owner->objects[id];
  return std::make_pair(obj.iboOff, obj.iboLen);
  }

bool DrawStorage::DrawCmd::isForwardShading() const {
  return Material::isForwardShading(alpha);
  }

bool DrawStorage::DrawCmd::isShadowmapRequired() const {
  return Material::isShadowmapRequired(alpha);
  }

bool DrawStorage::DrawCmd::isSceneInfoRequired() const {
  return Material::isSceneInfoRequired(alpha);
  }

bool DrawStorage::DrawCmd::isTextureInShadowPass() const {
  return Material::isTextureInShadowPass(alpha);
  }

void DrawStorage::InstanceDesc::setPosition(const Tempest::Matrix4x4& m) {
  for(int i=0; i<4; ++i)
    for(int r=0; r<3; ++r)
      pos[i][r] = m.at(i,r);
  }


DrawStorage::DrawStorage(VisualObjects& owner, const SceneGlobals& globals) : owner(owner), scene(globals) {
  tasks.clear();
  for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
    TaskCmd cmd;
    cmd.viewport = SceneGlobals::VisCamera(v);
    tasks.emplace_back(std::move(cmd));
    }
  objectsMorph.reserve(512);
  objectsFree.reserve(256);

  scratch.header.reserve(1024);
  scratch.patch .reserve(1024);
  }

DrawStorage::~DrawStorage() {
  }

DrawStorage::Item DrawStorage::alloc(const StaticMesh& mesh, const Material& mat,
                                     size_t iboOff, size_t iboLen, const PackedMesh::Cluster* cluster, Type type) {
  // return Item();
  // 64x64 meshlets
  assert(iboOff%PackedMesh::MaxInd==0);
  assert(iboLen%PackedMesh::MaxInd==0);

  const size_t id = implAlloc();

  Object& obj = objects[id];

  obj.type      = Type::Landscape;
  obj.iboOff    = uint32_t(iboOff);
  obj.iboLen    = uint32_t(iboLen);
  obj.bucketId  = bucketId(mat, mesh);
  obj.cmdId     = commandId(mat, type, obj.bucketId);
  obj.clusterId = clusterId(cluster, iboOff/PackedMesh::MaxInd, iboLen/PackedMesh::MaxInd, obj.bucketId, obj.cmdId);
  obj.alpha     = mat.alpha;

  if(obj.isEmpty())
    return Item(); // null command
  markClusters(obj.clusterId, iboLen/PackedMesh::MaxInd);
  updateRtAs(id);
  return Item(*this, id);
  }

DrawStorage::Item DrawStorage::alloc(const StaticMesh& mesh, const Material& mat,
                                     size_t iboOff, size_t iboLen, Type type) {
  // return Item();
  // 64x64 meshlets
  assert(iboOff%PackedMesh::MaxInd==0);
  assert(iboLen%PackedMesh::MaxInd==0);

  const size_t id = implAlloc();

  Object& obj = objects[id];

  if(mesh.morph.anim!=nullptr) {
    type = DrawStorage::Morph;
    }

  obj.type      = type;
  obj.iboOff    = uint32_t(iboOff);
  obj.iboLen    = uint32_t(iboLen);
  obj.bucketId  = bucketId(mat, mesh);
  obj.cmdId     = commandId(mat, obj.type, obj.bucketId);
  obj.clusterId = clusterId(buckets[obj.bucketId], iboOff/PackedMesh::MaxInd, iboLen/PackedMesh::MaxInd, obj.bucketId, obj.cmdId);
  obj.alpha     = mat.alpha;

  if(obj.isEmpty())
    return Item(); // null command

  obj.objInstance = owner.alloc(sizeof(InstanceDesc));
  clusters[obj.clusterId].instanceId = obj.objInstance.offsetId<InstanceDesc>();

  if(type==Morph) {
    obj.objMorphAnim = owner.alloc(sizeof(MorphDesc)*Resources::MAX_MORPH_LAYERS);
    obj.animPtr      = obj.objMorphAnim.offsetId<MorphDesc>();
    const MorphData d = {};
    obj.objMorphAnim.set(&d, 0, sizeof(d));
    }

  updateInstance(id);
  updateRtAs(id);
  return Item(*this, id);
  }

DrawStorage::Item DrawStorage::alloc(const AnimMesh& mesh, const Material& mat, const InstanceStorage::Id& anim,
                                     size_t iboOff, size_t iboLen) {
  // return Item();
  // 64x64 meshlets
  assert(iboOff%PackedMesh::MaxInd==0);
  assert(iboLen%PackedMesh::MaxInd==0);

  const size_t id = implAlloc();

  Object& obj = objects[id];

  obj.type      = Type::Animated;
  obj.iboOff    = uint32_t(iboOff);
  obj.iboLen    = uint32_t(iboLen);
  obj.bucketId  = bucketId(mat, mesh);
  obj.cmdId     = commandId(mat, obj.type, obj.bucketId);
  obj.clusterId = clusterId(buckets[obj.bucketId], iboOff/PackedMesh::MaxInd, iboLen/PackedMesh::MaxInd, obj.bucketId, obj.cmdId);
  obj.alpha     = mat.alpha;

  if(obj.isEmpty())
    return Item(); // null command
  obj.animPtr     = anim.offsetId<Matrix4x4>();
  obj.objInstance = owner.alloc(sizeof(InstanceDesc));
  clusters[obj.clusterId].instanceId = obj.objInstance.offsetId<InstanceDesc>();

  updateInstance(id);
  updateRtAs(id);
  return Item(*this, id);
  }

void DrawStorage::free(size_t id) {
  cmdDurtyBit = true;

  Object& obj = objects[id];
  const uint32_t meshletCount = (obj.iboLen/PackedMesh::MaxInd);
  cmd[obj.cmdId].maxPayload -= meshletCount;

  const uint32_t numCluster = (obj.type==Landscape ? meshletCount : 1);
  for(size_t i=0; i<numCluster; ++i) {
    clusters[obj.clusterId + i]              = Cluster();
    clusters[obj.clusterId + i].r            = -1;
    clusters[obj.clusterId + i].meshletCount = 0;
    markClusters(obj.clusterId + i);
    }
  clusters.free(obj.clusterId, numCluster);

  if(obj.wind==phoenix::animation_mode::none)
    objectsWind.erase(id);
  if(obj.type==Morph)
    objectsMorph.erase(id);

  obj = Object();
  while(objects.size()>0) {
    if(!objects.back().isEmpty())
      break;
    objects.pop_back();
    objectsFree.erase(objects.size());
    }
  if(id<objects.size())
    objectsFree.insert(id);
  }

void DrawStorage::updateInstance(size_t id, Matrix4x4* pos) {
  auto& obj = objects[id];
  if(obj.type==Landscape)
    return;

  InstanceDesc d;
  d.setPosition(pos==nullptr ? obj.pos : *pos);
  d.animPtr = obj.animPtr;
  d.fatness = obj.fatness;
  obj.objInstance.set(&d, 0, sizeof(d));

  auto cId  = obj.clusterId;
  auto npos = Vec3(obj.pos[3][0], obj.pos[3][1], obj.pos[3][2]);
  if(clusters[cId].pos != npos) {
    clusters[cId].pos = npos;
    markClusters(cId);
    }
  }

void DrawStorage::updateRtAs(size_t id) {
  auto& obj = objects[id];
  auto& mat = buckets[obj.bucketId].mat;

  bool useBlas = toRtCategory(obj.type)!=RtScene::None;
  if(mat.alpha==Material::Ghost)
    useBlas = false;
  if(mat.alpha!=Material::Solid && mat.alpha!=Material::AlphaTest && mat.alpha!=Material::Transparent)
    useBlas = false;

  if(!useBlas)
    return;

  auto& mesh = *buckets[obj.bucketId].staticMesh;
  if(auto b = mesh.blas(obj.iboOff, obj.iboLen)) {
    (void)b;
    owner.notifyTlas(mat, toRtCategory(obj.type));
    }
  }

void DrawStorage::markClusters(size_t id, size_t count) {
  // fixme: thread-safe
  for(size_t i=0; i<count; ++i) {
    clustersDurty[id/32] |= uint32_t(1u << (id%32));
    id++;
    }
  clustersDurtyBit = true;
  }

void DrawStorage::startMMAnim(size_t i, std::string_view animName, float intensity, uint64_t timeUntil) {
  auto& obj        = objects[i];
  auto  staticMesh = buckets[obj.bucketId].staticMesh;
  if(staticMesh==nullptr || staticMesh->morph.anim==nullptr)
    return;

  auto&  anim = *staticMesh->morph.anim;
  size_t id   = size_t(-1);
  for(size_t i=0; i<anim.size(); ++i)
    if(anim[i].name==animName) {
      id = i;
      break;
      }

  if(id==size_t(-1))
    return;

  objectsMorph.insert(i);

  auto& m = anim[id];
  if(timeUntil==uint64_t(-1) && m.duration>0)
    timeUntil = scene.tickCount + m.duration;

  // extend time of anim
  for(auto& i:obj.morphAnim) {
    if(i.id!=id || i.timeUntil<scene.tickCount)
      continue;
    i.timeUntil = timeUntil;
    i.intensity = intensity;
    return;
    }

  // find same layer
  for(auto& i:obj.morphAnim) {
    if(i.timeUntil<scene.tickCount)
      continue;
    if(anim[i.id].layer!=m.layer)
      continue;
    i.id        = id;
    i.timeUntil = timeUntil;
    i.intensity = intensity;
    return;
    }

  size_t nId = 0;
  for(size_t i=0; i<Resources::MAX_MORPH_LAYERS; ++i) {
    if(obj.morphAnim[nId].timeStart<=obj.morphAnim[i].timeStart)
      continue;
    nId = i;
    }

  auto& ani = obj.morphAnim[nId];
  ani.id        = id;
  ani.timeStart = scene.tickCount;
  ani.timeUntil = timeUntil;
  ani.intensity = intensity;
  }

void DrawStorage::setAsGhost(size_t id, bool g) {
  auto& obj= objects[id];
  if(obj.isGhost==g)
    return;

  auto& bx  = buckets[obj.bucketId];
  auto& cx  = cmd[obj.cmdId];
  auto  mat = bx.mat;

  const uint32_t meshletCount = (obj.iboLen/PackedMesh::MaxInd);
  cmd[obj.cmdId].maxPayload -= meshletCount;

  mat.alpha   = g ? Material::Ghost : obj.alpha;
  obj.isGhost = g;

  if(bx.staticMesh!=nullptr)
    obj.bucketId = bucketId(mat, *bx.staticMesh); else
    obj.bucketId = bucketId(mat, *bx.animMesh);

  obj.cmdId = commandId(mat, cx.type, obj.bucketId);
  cmd[obj.cmdId].maxPayload += meshletCount;

  cmdDurtyBit = true;
  const uint32_t numCluster = (obj.type==Landscape ? meshletCount : 1);
  for(size_t i=0; i<numCluster; ++i) {
    clusters[obj.clusterId + i].commandId = obj.cmdId;
    clusters[obj.clusterId + i].bucketId  = obj.bucketId;
    markClusters(obj.clusterId + i);
    }
  }

bool DrawStorage::commitCommands() {
  if(!cmdDurtyBit)
    return false;
  cmdDurtyBit = false;

  totalPayload = 0;
  for(auto& i:cmd) {
    i.firstPayload = uint32_t(totalPayload);
    totalPayload  += i.maxPayload;
    }

  std::vector<IndirectCmd> cx(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i) {
    cx[i].vertexCount = PackedMesh::MaxInd;
    cx[i].writeOffset = cmd[i].firstPayload;
    }

  ord.resize(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i)
    ord[i] = &cmd[i];
  std::sort(ord.begin(), ord.end(), cmpDraw);

  auto& device = Resources::device();
  for(auto& v:views) {
    Resources::recycle(std::move(v.visClusters));
    Resources::recycle(std::move(v.indirectCmd));
    Resources::recycle(std::move(v.descInit));

    v.visClusters = device.ssbo(nullptr, totalPayload*sizeof(uint32_t)*4);
    v.indirectCmd = device.ssbo(cx.data(), sizeof(IndirectCmd)*cx.size());

    v.descInit = device.descriptors(Shaders::inst().clusterInit);
    v.descInit.set(T_Indirect, v.indirectCmd);
    }

  return true;
  }

bool DrawStorage::commitBuckets() {
  if(!bucketsDurtyBit)
    return false;
  bucketsDurtyBit = false;

  std::vector<BucketGpu> bucket;
  for(auto& i:buckets) {
    BucketGpu bx;
    bx.texAniMapDirPeriod = i.mat.texAniMapDirPeriod;
    bx.waveMaxAmplitude   = i.mat.waveMaxAmplitude;
    bx.alphaWeight        = i.mat.alphaWeight;
    bx.envMapping         = i.mat.envMapping;
    if(i.staticMesh!=nullptr) {
      auto& bbox    = i.staticMesh->bbox.bbox;
      bx.bboxRadius = i.staticMesh->bbox.rConservative;
      bx.bbox[0]    = Vec4(bbox[0].x,bbox[0].y,bbox[0].z,0.f);
      bx.bbox[1]    = Vec4(bbox[1].x,bbox[1].y,bbox[1].z,0.f);
      }
    else if(i.animMesh!=nullptr) {
      auto& bbox    = i.animMesh->bbox.bbox;
      bx.bboxRadius = i.animMesh->bbox.rConservative;
      bx.bbox[0]    = Vec4(bbox[0].x,bbox[0].y,bbox[0].z,0.f);
      bx.bbox[1]    = Vec4(bbox[1].x,bbox[1].y,bbox[1].z,0.f);
      }
    bucket.push_back(bx);
    }

  auto& device = Resources::device();
  Resources::recycle(std::move(bucketsGpu));
  bucketsGpu = device.ssbo(bucket);
  return true;
  }

bool DrawStorage::commitClusters(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  if(!clustersDurtyBit)
    return false;
  clustersDurtyBit = false;

  auto& device = Resources::device();
  if(clustersGpu.byteSize() == clusters.size()*sizeof(clusters[0])) {
    patchClusters(cmd, fId);
    return false;
    }

  Resources::recycle(std::move(clustersGpu));
  clustersGpu  = device.ssbo(clusters.data);
  std::fill(clustersDurty.begin(), clustersDurty.end(), 0x0);
  return true;
  }

void DrawStorage::patchClusters(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  std::vector<uint32_t>& header = scratch.header;
  std::vector<Cluster>&  patch  = scratch.patch;

  header.clear();
  patch.clear();

  for(size_t i=0; i<clustersDurty.size(); ++i) {
    if(clustersDurty[i]==0x0)
      continue;
    const uint32_t mask = clustersDurty[i];
    clustersDurty[i] = 0x0;

    for(size_t r=0; r<32; ++r) {
      if((mask & (1u<<r))==0)
        continue;
      size_t idx = i*32 + r;
      if(idx>=clusters.size())
        continue;
      patch.push_back(clusters[idx]);
      header.push_back(uint32_t(idx));
      }
    }

  if(header.empty())
    return;

  auto& device = Resources::device();
  auto& p = this->patch[fId];
  if(p.desc.isEmpty())
    p.desc = device.descriptors(Shaders::inst().clusterPath);

  p.desc.set(0, clustersGpu);
  if(header.size()*sizeof(header[0]) < p.indices.byteSize()) {
    p.indices.update(header);
    } else {
    p.indices = device.ssbo(BufferHeap::Upload, header);
    p.desc.set(2, p.indices);
    }

  if(patch.size()*sizeof(patch[0]) < p.data.byteSize()) {
    p.data.update(patch);
    } else {
    p.data = device.ssbo(BufferHeap::Upload, patch);
    p.desc.set(1, p.data);
    }

  const uint32_t count = uint32_t(header.size());
  cmd.setFramebuffer({});
  cmd.setUniforms(Shaders::inst().clusterPath, p.desc, &count, sizeof(count));
  cmd.dispatchThreads(count);
  }

bool DrawStorage::commit(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  bool ret = false;
  ret |= commitCommands();
  ret |= commitBuckets();
  ret |= commitClusters(cmd, fId);
  return ret;
  }

void DrawStorage::prepareUniforms() {
  invalidateUbo();
  }

void DrawStorage::invalidateUbo() {
  if(owner.instanceSsbo().isEmpty())
    return;

  auto& device = Resources::device();
  device.waitIdle(); // TODO

  std::vector<const Tempest::Texture2d*>     tex;
  std::vector<const Tempest::StorageBuffer*> vbo, ibo;
  std::vector<const Tempest::StorageBuffer*> morphId, morph;
  for(auto& i:buckets) {
    tex.push_back(i.mat.tex);
    if(i.staticMesh!=nullptr) {
      ibo    .push_back(&i.staticMesh->ibo8);
      vbo    .push_back(&i.staticMesh->vbo);
      morphId.push_back(i.staticMesh->morph.index);
      morph  .push_back(i.staticMesh->morph.samples);
      } else {
      ibo    .push_back(&i.animMesh->ibo8);
      vbo    .push_back(&i.animMesh->vbo);
      morphId.push_back(nullptr);
      morph  .push_back(nullptr);
      }
    }

  for(auto& i:tasks) {
    Resources::recycle(std::move(i.desc));
    if(i.viewport==SceneGlobals::V_Main)
      i.desc = device.descriptors(Shaders::inst().clusterTaskHiZ);
    else if(i.viewport==SceneGlobals::V_HiZ)
      i.desc = device.descriptors(Shaders::inst().clusterTaskHiZCr);
    else
      i.desc = device.descriptors(Shaders::inst().clusterTask);
    i.desc.set(T_Clusters, clustersGpu);
    i.desc.set(T_Indirect, views[i.viewport].indirectCmd);
    i.desc.set(T_Payload,  views[i.viewport].visClusters);

    i.desc.set(T_Scene,    scene.uboGlobal[i.viewport]);
    i.desc.set(T_Instance, owner.instanceSsbo());
    i.desc.set(T_Bucket,   bucketsGpu);
    i.desc.set(T_HiZ,      *scene.hiZ);
    }

  for(auto& i:cmd) {
    for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
      if(i.desc[v].isEmpty())
        continue;
      auto& mem = (i.type==Type::Landscape) ? clustersGpu : owner.instanceSsbo();

      i.desc[v].set(L_Scene,    scene.uboGlobal[v]);
      i.desc[v].set(L_Instance, mem);
      i.desc[v].set(L_Ibo,      ibo);
      i.desc[v].set(L_Vbo,      vbo);
      i.desc[v].set(L_Diffuse,  tex);
      i.desc[v].set(L_Bucket,   bucketsGpu);
      i.desc[v].set(L_Payload,  views[v].visClusters);
      i.desc[v].set(L_Sampler,  Sampler::anisotrophy());

      if(v==SceneGlobals::V_Main || i.isTextureInShadowPass()) {
        //i.desc[v].set(L_Diffuse, tex);
        }

      if(v==SceneGlobals::V_Main && i.isShadowmapRequired()) {
        i.desc[v].set(L_Shadow0, *scene.shadowMap[0],Resources::shadowSampler());
        i.desc[v].set(L_Shadow1, *scene.shadowMap[1],Resources::shadowSampler());
        }

      if(i.type==Morph) {
        i.desc[v].set(L_MorphId,  morphId);
        i.desc[v].set(L_Morph,    morph);
        }

      if(v==SceneGlobals::V_Main && i.isSceneInfoRequired()) {
        auto smp = Sampler::bilinear();
        smp.setClamping(ClampMode::MirroredRepeat);
        i.desc[v].set(L_SceneClr, *scene.sceneColor, smp);

        smp = Sampler::nearest();
        smp.setClamping(ClampMode::MirroredRepeat);
        i.desc[v].set(L_GDepth, *scene.sceneDepth, smp);
        }
      }
    }
  }

void DrawStorage::fillTlas(RtScene& out) {
  for(auto& obj:objects) {
    if(obj.isEmpty())
      continue;
    auto& bucket = buckets[obj.bucketId];
    auto* mesh   = bucket.staticMesh;
    auto& mat    = bucket.mat;
    if(mesh==nullptr)
      continue;
    if(auto blas = mesh->blas(obj.iboOff, obj.iboLen)) {
      out.addInstance(obj.pos, *blas, mat, *mesh, obj.iboOff, obj.iboLen, toRtCategory(obj.type));
      }
    }
  }

void DrawStorage::visibilityPass(Encoder<CommandBuffer>& cmd, uint8_t frameId, int pass) {
  static bool freeze = false;
  if(freeze)
    return;

  cmd.setFramebuffer({});
  if(pass==0) {
    for(auto& v:views) {
      if(this->cmd.empty())
        continue;
      cmd.setUniforms(Shaders::inst().clusterInit, v.descInit);
      cmd.dispatchThreads(this->cmd.size());
      }
    }

  for(auto& i:tasks) {
    if(i.viewport==SceneGlobals::V_HiZ && pass!=0)
      continue;
    if(i.viewport!=SceneGlobals::V_HiZ && pass==0)
      continue;
    struct Push { uint32_t firstMeshlet; uint32_t meshletCount; float znear; } push = {};
    push.firstMeshlet = 0;
    push.meshletCount = uint32_t(clusters.size());
    push.znear        = scene.znear;

    auto* pso = &Shaders::inst().clusterTask;
    if(i.viewport==SceneGlobals::V_Main)
      pso = &Shaders::inst().clusterTaskHiZ;
    else if(i.viewport==SceneGlobals::V_HiZ)
      pso = &Shaders::inst().clusterTaskHiZCr;
    cmd.setUniforms(*pso, i.desc, &push, sizeof(push));
    cmd.dispatchThreads(push.meshletCount);
    }
  }

void DrawStorage::drawHiZ(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  // return;
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};

  auto  viewId = SceneGlobals::V_HiZ;
  auto& view   = views[viewId];
  for(size_t i=0; i<ord.size(); ++i) {
    auto& cx = *ord[i];
    if(cx.desc[viewId].isEmpty())
      continue;
    if(cx.alpha!=Material::Solid && cx.alpha!=Material::AlphaTest)
      continue;
    if(cx.type!=Landscape && cx.type!=Static)
      continue;
    auto id  = size_t(std::distance(this->cmd.data(), &cx));
    push.firstMeshlet = cx.firstPayload;
    push.meshletCount = cx.maxPayload;

    cmd.setUniforms(*cx.pHiZ, cx.desc[viewId], &push, sizeof(push));
    cmd.drawIndirect(view.indirectCmd, sizeof(IndirectCmd)*id);
    }
  }

void DrawStorage::drawGBuffer(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  // return;
  for(auto i:{Material::Solid, Material::AlphaTest}) {
    drawCommon(cmd, fId, SceneGlobals::V_Main, i);
    drawCommon(cmd, fId, SceneGlobals::V_Main, i);
    }
  }

void DrawStorage::drawShadow(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, int layer) {
  // return;
  auto view = SceneGlobals::VisCamera(SceneGlobals::V_Shadow0 + layer);
  for(auto i:{Material::Solid, Material::AlphaTest}) {
    drawCommon(cmd, fId, view, i);
    drawCommon(cmd, fId, view, i);
    }
  }

void DrawStorage::drawTranslucent(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  // return;
  for(auto i:{Material::Multiply, Material::Multiply2, Material::Transparent, Material::AdditiveLight, Material::Ghost}) {
    drawCommon(cmd, fId, SceneGlobals::V_Main, i);
    }
  }

void DrawStorage::drawWater(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  // return;
  drawCommon(cmd, fId, SceneGlobals::V_Main, Material::Water);
  }

void DrawStorage::drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, SceneGlobals::VisCamera viewId, Material::AlphaFunc func) {
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};

  auto& view = views[viewId];
  // auto  b    = std::lower_bound(ord.begin(), ord.end(), cmpDraw);
  // auto  e    = std::upper_bound(ord.begin(), ord.end(), cmpDraw);
  for(size_t i=0; i<ord.size(); ++i) {
    auto& cx = *ord[i];
    if(cx.desc[viewId].isEmpty())
      continue;
    if(cx.alpha!=func)
      continue;

    const RenderPipeline* pso = nullptr;
    switch(viewId) {
      case SceneGlobals::V_Shadow0:
      case SceneGlobals::V_Shadow1:
        pso = cx.pShadow;
        break;
      case SceneGlobals::V_Main:
        pso = cx.pMain;
        break;
      case SceneGlobals::V_HiZ:
      case SceneGlobals::V_Count:
        break;
      }
    if(pso==nullptr)
      continue;

    auto id  = size_t(std::distance(this->cmd.data(), &cx));
    push.firstMeshlet = cx.firstPayload;
    push.meshletCount = cx.maxPayload;

    cmd.setUniforms(*pso, cx.desc[viewId], &push, sizeof(push));
    cmd.drawIndirect(view.indirectCmd, sizeof(IndirectCmd)*id);
    }
  }

bool DrawStorage::cmpDraw(const DrawCmd* l, const DrawCmd* r) {
  return l->alpha < r->alpha;
  }

void DrawStorage::preFrameUpdate(uint8_t fId) {
  preFrameUpdateWind(fId);
  preFrameUpdateMorph(fId);
  }

void DrawStorage::preFrameUpdateWind(uint8_t fId) {
  if(!scene.zWindEnabled)
    return;

  for(auto id:objectsWind) {
    auto& i     = objects[id];
    auto  pos   = i.pos;
    float shift = i.pos[3][0]*scene.windDir.x + i.pos[3][2]*scene.windDir.y;

    static const uint64_t period = scene.windPeriod;
    float a = float(scene.tickCount%period)/float(period);
    a = a*2.f-1.f;
    a = std::cos(float(a*M_PI) + shift*0.0001f);

    switch(i.wind) {
      case phoenix::animation_mode::wind:
        // tree. note: mods tent to bump Intensity to insane values
        if(i.windIntensity>0.f)
          a *= 0.03f; else
          a *= 0;
        break;
      case phoenix::animation_mode::wind2:
        // grass
        if(i.windIntensity<=1.0)
          a *= i.windIntensity * 0.1f; else
          a *= 0;
        break;
      case phoenix::animation_mode::none:
      default:
        // error
        a = 0.f;
        break;
      }
    pos[1][0] += scene.windDir.x*a;
    pos[1][2] += scene.windDir.y*a;

    updateInstance(id, &pos);
    }
  }

void DrawStorage::preFrameUpdateMorph(uint8_t fId) {
  for(auto it=objectsMorph.begin(); it!=objectsMorph.end(); ) {
    auto& obj = objects[*it];
    it = objectsMorph.erase(it);

    MorphData data = {};
    for(size_t i=0; i<Resources::MAX_MORPH_LAYERS; ++i) {
      auto&    ani  = obj.morphAnim[i];
      auto&    bk   = buckets[obj.bucketId];
      auto&    anim = (*bk.staticMesh->morph.anim)[ani.id];
      uint64_t time = (scene.tickCount-ani.timeStart);

      float    alpha     = float(time%anim.tickPerFrame)/float(anim.tickPerFrame);
      float    intensity = ani.intensity;

      if(scene.tickCount>ani.timeUntil) {
        data.morph[i].intensity = 0;
        continue;
        }

      const uint32_t samplesPerFrame = uint32_t(anim.samplesPerFrame);
      data.morph[i].indexOffset = uint32_t(anim.index);
      data.morph[i].sample0     = uint32_t((time/anim.tickPerFrame+0)%anim.numFrames)*samplesPerFrame;
      data.morph[i].sample1     = uint32_t((time/anim.tickPerFrame+1)%anim.numFrames)*samplesPerFrame;
      data.morph[i].alpha       = uint16_t(alpha*uint16_t(-1));
      data.morph[i].intensity   = uint16_t(intensity*uint16_t(-1));
      }

    obj.objMorphAnim.set(&data, 0, sizeof(data));
    }
  }

size_t DrawStorage::implAlloc() {
  if(!objectsFree.empty()) {
    auto id = *objectsFree.begin();
    objectsFree.erase(objectsFree.begin());
    return id;
    }

  objects.resize(objects.size()+1);
  return objects.size()-1;
  }

uint16_t DrawStorage::bucketId(const Material& mat, const StaticMesh& mesh) {
  for(size_t i=0; i<buckets.size(); ++i) {
    auto& b = buckets[i];
    if(b.staticMesh==&mesh && b.mat==mat)
      return uint16_t(i);
    }

  Bucket bx;
  bx.staticMesh = &mesh;
  bx.mat        = mat;
  buckets.emplace_back(std::move(bx));
  bucketsDurtyBit = true;
  return uint16_t(buckets.size()-1);
  }

uint16_t DrawStorage::bucketId(const Material& mat, const AnimMesh& mesh) {
  for(size_t i=0; i<buckets.size(); ++i) {
    auto& b = buckets[i];
    if(b.animMesh==&mesh && b.mat==mat)
      return uint16_t(i);
    }

  Bucket bx;
  bx.animMesh = &mesh;
  bx.mat      = mat;

  bucketsDurtyBit = true;
  buckets.emplace_back(std::move(bx));
  return uint16_t(buckets.size()-1);
  }

uint16_t DrawStorage::commandId(const Material& m, Type type, uint32_t bucketId) {
  auto pMain   = Shaders::inst().materialPipeline(m, type, Shaders::T_Main);
  auto pShadow = Shaders::inst().materialPipeline(m, type, Shaders::T_Shadow);
  auto pHiZ    = Shaders::inst().materialPipeline(m, type, Shaders::T_Depth);
  if(pMain==nullptr && pShadow==nullptr && pHiZ==nullptr)
    return uint16_t(-1);

  const bool bindless = true;

  for(size_t i=0; i<cmd.size(); ++i) {
    if(cmd[i].pMain!=pMain || cmd[i].pShadow!=pShadow || cmd[i].pHiZ!=pHiZ)
      continue;
    if(!bindless && cmd[i].bucketId != bucketId)
      continue;
    return uint16_t(i);
    }

  auto ret = uint16_t(cmd.size());

  auto& device = Resources::device();
  DrawCmd cx;
  cx.pMain       = pMain;
  cx.pShadow     = pShadow;
  cx.pHiZ        = pHiZ;
  cx.bucketId    = bindless ? 0xFFFFFFFF : bucketId;
  cx.type        = type;
  cx.alpha       = m.alpha;
  if(cx.pMain!=nullptr) {
    cx.desc[SceneGlobals::V_Main] = device.descriptors(*cx.pMain);
    }
  if(cx.pShadow!=nullptr) {
    cx.desc[SceneGlobals::V_Shadow0] = device.descriptors(*cx.pShadow);
    cx.desc[SceneGlobals::V_Shadow1] = device.descriptors(*cx.pShadow);
    }
  if(cx.pHiZ!=nullptr) {
    cx.desc[SceneGlobals::V_HiZ] = device.descriptors(*cx.pHiZ);
    }
  cmd.push_back(std::move(cx));
  cmdDurtyBit = true;
  return ret;
  }

uint32_t DrawStorage::clusterId(const PackedMesh::Cluster* cx, size_t firstMeshlet, size_t meshletCount, uint16_t bucketId, uint16_t commandId) {
  if(commandId==uint16_t(-1))
    return uint32_t(-1);

  const auto ret = clusters.alloc(meshletCount);
  for(size_t i=0; i<meshletCount; ++i) {
    Cluster c;
    c.pos          = cx[i].pos;
    c.r            = cx[i].r;
    c.bucketId     = bucketId;
    c.commandId    = commandId;
    c.firstMeshlet = uint32_t(firstMeshlet + i);
    c.meshletCount = 1;
    c.instanceId   = uint32_t(-1);

    clusters[ret+i] = c;
    }

  clustersDurty.resize((clusters.size() + 32 - 1)/32);
  markClusters(ret, meshletCount);

  cmd[commandId].maxPayload  += uint32_t(meshletCount);
  return uint32_t(ret);
  }

uint32_t DrawStorage::clusterId(const Bucket& bucket, size_t firstMeshlet, size_t meshletCount, uint16_t bucketId, uint16_t commandId) {
  if(commandId==uint16_t(-1))
    return uint32_t(-1);

  const auto ret = clusters.alloc(1);

  Cluster& c = clusters[ret];
  if(bucket.staticMesh!=nullptr)
    c.r = bucket.staticMesh->bbox.rConservative + bucket.mat.waveMaxAmplitude; else
    c.r = bucket.animMesh->bbox.rConservative;
  c.bucketId     = bucketId;
  c.commandId    = commandId;
  c.firstMeshlet = uint32_t(firstMeshlet);
  c.meshletCount = uint32_t(meshletCount);
  c.instanceId   = uint32_t(-1);

  clustersDurty.resize((clusters.size() + 32 - 1)/32);
  markClusters(ret, 1);

  cmd[commandId].maxPayload  += uint32_t(meshletCount);
  return uint32_t(ret);
  }

void DrawStorage::dbgDraw(Painter& p, Vec2 wsz) {
  auto cam = Gothic::inst().camera();
  if(cam==nullptr)
    return;

  /*
  for(auto& c:clusters) {
    dbgDraw(p, wsz, *cam, c);
    }
  */

  /*
  if(auto pl = Gothic::inst().player()) {
    Cluster cx;
    cx.pos = pl->position();
    cx.r   = 100;
    dbgDraw(p, wsz, *cam, cx);
    }
  */
  }

void DrawStorage::dbgDraw(Tempest::Painter& p, Vec2 wsz, const Camera& cam, const Cluster& cx) {
  auto  c       = cx.pos;
  auto  project = cam.projective();

  cam.view().project(c);
  //const vec3  c     = (scene.view * vec4(sphere.xyz, 1)).xyz;
  const float R     = cx.r;
  const float znear = cam.zNear();
  if(c.z - R < znear)
    ;//return;

  // depthMin = znear / (c.z + r);
  // float z = c.z + R;
  // depthMin  = scene.project[3][2]/z + scene.project[2][2];

  float P00 = project[0][0];
  float P11 = project[1][1];

  Vec3  cr   = c * R;
  float czr2 = c.z * c.z - R * R;

  float vx   = std::sqrt(c.x * c.x + czr2);
  float minx = (vx * c.x - cr.z) / (vx * c.z + cr.x);
  float maxx = (vx * c.x + cr.z) / (vx * c.z - cr.x);

  float vy   = std::sqrt(c.y * c.y + czr2);
  float miny = (vy * c.y - cr.z) / (vy * c.z + cr.y);
  float maxy = (vy * c.y + cr.z) / (vy * c.z - cr.y);

  Vec4 aabb;
  aabb = Vec4(minx * P00, miny * P11, maxx * P00, maxy * P11);
  aabb = aabb*0.5 + Vec4(0.5);

  aabb.x = aabb.x * wsz.x;
  aabb.z = aabb.z * wsz.x;

  aabb.y = aabb.y * wsz.y;
  aabb.w = aabb.w * wsz.y;

  if(aabb.x>=aabb.z)
    Log::d("");
  if(aabb.y>=aabb.w)
    Log::d("");

  p.setBrush(Color(0,0,1,0.1f));
  p.drawRect(int(aabb.x), int(aabb.y), int(aabb.z-aabb.x), int(aabb.w-aabb.y));
  }

void DrawStorage::dbgDrawBBox(Tempest::Painter& p, Tempest::Vec2 wsz, const Camera& cam, const Cluster& c) {
  /*
  auto& b       = buckets[c.bucketId];
  auto  project = cam.viewProj();

  Vec4  aabb     = Vec4(1, 1, -1, -1);
  float depthMin = 1;
  for(uint32_t i=0; i<8; ++i) {
    float x = b[i&0x1 ? 1 : 0].x;
    float y = b[i&0x2 ? 1 : 0].y;
    float z = b[i&0x4 ? 1 : 0].z;

    const Vec3 pos = Vec3(x, y, z);
    Vec4 trPos = Vec4(pos,1.0);
    trPos = Vec4(obj.mat*trPos, 1.0);
    trPos = scene.viewProject*trPos;
    if(trPos.w<znear || false) {
      depthMin = 0;
      aabb     = vec4(0,0,1,1);
      return true;
      }

    vec3 bp = trPos.xyz/trPos.w;

    aabb.xy  = min(aabb.xy,  bp.xy);
    aabb.zw  = max(aabb.zw,  bp.xy);
    depthMin = min(depthMin, bp.z);
    }
  aabb = aabb*0.5 + Vec4(0.5);

  aabb.x = aabb.x * wsz.x;
  aabb.z = aabb.z * wsz.x;

  aabb.y = aabb.y * wsz.y;
  aabb.w = aabb.w * wsz.y;

  if(aabb.x>=aabb.z)
    Log::d("");
  if(aabb.y>=aabb.w)
    Log::d("");

  p.setBrush(Color(0,0,1,0.1f));
  p.drawRect(int(aabb.x), int(aabb.y), int(aabb.z-aabb.x), int(aabb.w-aabb.y));
  */
  }

