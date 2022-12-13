#include "visualobjects.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/animmesh.h"
#include "gothic.h"

using namespace Tempest;

VisualObjects::VisualObjects(const SceneGlobals& globals, const std::pair<Vec3, Vec3>& bbox)
  : globals(globals), visGroup(bbox) {
  }

VisualObjects::~VisualObjects() {
  }

ObjectsBucket& VisualObjects::getBucket(ObjectsBucket::Type type, const Material& mat,
                                        const StaticMesh* st, const AnimMesh* anim, const StorageBuffer* desc) {
  for(auto& i:buckets)
    if(i->size()<ObjectsBucket::CAPACITY && i->isCompatible(type,mat,st,anim,desc))
      return *i;
  buckets.emplace_back(ObjectsBucket::mkBucket(type,mat,*this,globals,st,anim,desc));
  return *buckets.back();
  }

ObjectsBucket::Item VisualObjects::get(const StaticMesh& mesh, const Material& mat,
                                       size_t iboOffset, size_t iboLength,
                                       bool staticDraw) {
  if(mat.tex==nullptr) {
    Log::e("no texture?!");
    return ObjectsBucket::Item();
    }

  const ObjectsBucket::Type type = (staticDraw ? ObjectsBucket::Static : ObjectsBucket::Movable);

  auto&        bucket = getBucket(type,mat,&mesh,nullptr,nullptr);
  const size_t id     = bucket.alloc(mesh,iboOffset,iboLength,mesh.bbox,mat);
  return ObjectsBucket::Item(bucket,id);
  }

ObjectsBucket::Item VisualObjects::get(const StaticMesh& mesh, const Material& mat,
                                       size_t iboOff, size_t iboLen,
                                       const Tempest::StorageBuffer& desc,
                                       const Bounds& bbox, ObjectsBucket::Type type) {
  if(mat.tex==nullptr) {
    Tempest::Log::e("no texture?!");
    return ObjectsBucket::Item();
    }
  auto&        bucket = getBucket(type,mat,&mesh,nullptr,&desc);
  const size_t id     = bucket.alloc(mesh,iboOff,iboLen,bbox,mat);
  return ObjectsBucket::Item(bucket,id);
  }

ObjectsBucket::Item VisualObjects::get(const AnimMesh &mesh, const Material& mat,
                                       size_t iboOff, size_t iboLen,
                                       const MatrixStorage::Id& anim) {
  if(mat.tex==nullptr) {
    Tempest::Log::e("no texture?!");
    return ObjectsBucket::Item();
    }
  auto&        bucket = getBucket(ObjectsBucket::Animated,mat,nullptr,&mesh,nullptr);
  const size_t id     = bucket.alloc(mesh,iboOff,iboLen,anim);
  return ObjectsBucket::Item(bucket,id);
  }

ObjectsBucket::Item VisualObjects::get(const Material& mat) {
  if(mat.tex==nullptr) {
    Tempest::Log::e("no texture?!");
    return ObjectsBucket::Item();
    }
  auto&        bucket = getBucket(ObjectsBucket::Pfx,mat,nullptr,nullptr,nullptr);
  const size_t id     = bucket.alloc(Bounds());
  return ObjectsBucket::Item(bucket,id);
  }

MatrixStorage::Id VisualObjects::getMatrixes(BufferHeap heap, size_t boneCnt) {
  return matrix.alloc(heap, boneCnt);
  }

const Tempest::StorageBuffer& VisualObjects::matrixSsbo(Tempest::BufferHeap heap, uint8_t fId) const {
  return matrix.ssbo(heap, fId);
  }

void VisualObjects::setupUbo() {
  for(auto& c:buckets)
    c->setupUbo();
  }

void VisualObjects::preFrameUpdate(uint8_t fId) {
  recycledId = fId;
  recycled[fId].clear();

  //mkTlas(fId);
  mkIndex();
  for(auto& c:buckets)
    c->preFrameUpdate(fId);
  commitUbo(fId);
  }

void VisualObjects::visibilityPass(const Frustrum fr[]) {
  visGroup.pass(fr);
  }

void VisualObjects::drawTranslucent(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId) {
  for(size_t i=lastSolidBucket;i<index.size();++i) {
    auto c = index[i];
    if(c->material().alpha==Material::AlphaFunc::Water)
      continue;
    c->draw(enc,fId);
    }
  }

void VisualObjects::drawWater(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId) {
  for(size_t i=lastSolidBucket;i<index.size();++i) {
    auto c = index[i];
    if(c->material().alpha!=Material::AlphaFunc::Water)
      continue;
    c->draw(enc,fId);
    }
  }

void VisualObjects::drawGBuffer(Tempest::Encoder<CommandBuffer>& enc, uint8_t fId) {
  for(size_t i=0;i<lastSolidBucket;++i) {
    auto c = index[i];
    c->drawGBuffer(enc,fId);
    }
  }

void VisualObjects::drawShadow(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId, int layer) {
  for(size_t i=0;i<lastSolidBucket;++i) {
    auto c = index[i];
    c->drawShadow(enc,fId,layer);
    }
  }

void VisualObjects::drawHiZ(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId) {
  for(size_t i=0;i<lastSolidBucket;++i) {
    auto c = index[i];
    if(c->type()!=ObjectsBucket::LandscapeShadow)
      continue;
    c->drawHiZ(enc,fId);
    return;
    }
  }

void VisualObjects::resetIndex() {
  index.clear();
  }

void VisualObjects::resetTlas() {
  needtoInvalidateTlas = true;
  }

void VisualObjects::recycle(Tempest::DescriptorSet&& del) {
  if(del.isEmpty())
    return;
  recycled[recycledId].emplace_back(std::move(del));
  }

void VisualObjects::setLandscapeBlas(const Tempest::AccelerationStructure* blas) {
  landBlas             = blas;
  needtoInvalidateTlas = true;
  }

void VisualObjects::mkIndex() {
  if(index.size()!=0)
    return;
  index.reserve(buckets.size());
  index.resize(buckets.size());
  size_t id=0;
  for(auto& i:buckets) {
    if(i->size()==0)
      continue;
    index[id] = i.get();
    ++id;
    }
  index.resize(id);

  std::sort(index.begin(),index.end(),[](const ObjectsBucket* l,const ObjectsBucket* r) {
    auto& lm = l->material();
    auto& rm = r->material();

    if(lm.alphaOrder()<rm.alphaOrder())
      return true;
    if(lm.alphaOrder()>rm.alphaOrder())
      return false;

    const int lt = l->type()==ObjectsBucket::Landscape ? 0 : 1;
    const int rt = r->type()==ObjectsBucket::Landscape ? 0 : 1;

    if(lt<rt)
      return true;
    if(lt>rt)
      return false;

    auto lv = l->meshPointer();
    auto rv = r->meshPointer();
    if(lv<rv)
      return true;
    if(lv>rv)
      return false;

    return lm.tex < rm.tex;
    });
  lastSolidBucket = index.size();
  for(size_t i=0;i<index.size();++i) {
    auto c = index[i];
    if(!c->material().isSolid()) {
      lastSolidBucket = i;
      break;
      }
    }
  visGroup.buildVSetIndex(index);
  /*
  std::unordered_set<std::string> uniqTex;
  std::unordered_set<const void*> uniqMesh;
  for(size_t i=0; i<index.size(); ++i) {
    auto& b = index[i];
    const char* name = "ObjectsBucket   ";
    if(dynamic_cast<ObjectsBucketDyn*>(b)!=nullptr)
      name = "ObjectsBucketDyn";
    char ind[32] = {};
    std::snprintf(ind,32,"%04d",int(i));
    char size[32] = {};
    std::snprintf(size,32,"%03d",int(b->size()));
    Log::d(name,"[",ind,"] size = ",size," ",b->meshPointer()," ",b->material().debugHint);
    uniqTex .insert(b->material().debugHint);
    uniqMesh.insert(b->meshPointer());
    }
  Log::d("uniqTex: ",uniqTex.size()," uniqMesh:",uniqMesh.size());
  */
  }

void VisualObjects::commitUbo(uint8_t fId) {
  bool sk = matrix.commit(fId);
  if(!sk)
    return;
  for(auto& c:buckets)
    c->invalidateUbo(fId);
  }

void VisualObjects::updateTlas(Bindless& out, uint8_t fId) {
  if(!needtoInvalidateTlas || !globals.tlasEnabled)
    return;
  needtoInvalidateTlas = false;

  if(!Gothic::inst().doRayQuery())
    return;

  std::vector<Tempest::RtInstance> inst;
  std::vector<uint32_t>            iboOff;
  out.tex.clear();
  out.vbo.clear();
  out.ibo.clear();
  if(landBlas!=nullptr) {
    Tempest::RtInstance ix;
    ix.mat  = Matrix4x4::mkIdentity();
    ix.blas = landBlas;
    inst.push_back(ix);
    out.tex.push_back(&Resources::fallbackBlack());
    out.vbo.push_back(nullptr);
    out.ibo.push_back(nullptr);
    iboOff.push_back(0);
    }
  for(auto& c:buckets)
    c->fillTlas(inst,iboOff,out);

  auto& device = Resources::device();
  device.waitIdle();

  out.iboOffset = device.ssbo(iboOff);
  tlas = device.tlas(inst);

  onTlasChanged(&tlas);
  }
