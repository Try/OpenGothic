#include "visualobjects.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/animmesh.h"
#include "graphics/sky/sky.h"
#include "utils/workers.h"

using namespace Tempest;

VisualObjects::VisualObjects(const SceneGlobals& globals)
  :globals(globals), sky(new Sky(globals)) {
  }

VisualObjects::~VisualObjects() {
  }

ObjectsBucket& VisualObjects::getBucket(const Material& mat, const ProtoMesh* anim, ObjectsBucket::Type type) {
  const std::vector<ProtoMesh::Animation>* a = nullptr;
  if(anim!=nullptr && anim->morph.size()>0)
    a = &anim->morph;

  for(auto& i:buckets)
    if(i.material()==mat && i.morph()==a && i.type()==type && i.size()<ObjectsBucket::CAPACITY)
      return i;

  if(type==ObjectsBucket::Type::Static)
    buckets.emplace_back(mat,anim,*this,globals,uboStatic,type); else
    buckets.emplace_back(mat,anim,*this,globals,uboDyn,   type);
  return buckets.back();
  }

ObjectsBucket::Item VisualObjects::get(const StaticMesh &mesh, const Material& mat,
                                       size_t iboOffset, size_t iboLen,
                                       const ProtoMesh* anim,
                                       bool staticDraw) {
  if(mat.tex==nullptr) {
    Log::e("no texture?!");
    return ObjectsBucket::Item();
    }
  auto&        bucket = getBucket(mat,anim,staticDraw ? ObjectsBucket::Static : ObjectsBucket::Movable);
  const size_t id     = bucket.alloc(mesh.vbo,mesh.ibo,iboOffset,iboLen,mesh.bbox);
  return ObjectsBucket::Item(bucket,id);
  }

ObjectsBucket::Item VisualObjects::get(const AnimMesh &mesh, const Material& mat, const SkeletalStorage::AnimationId& anim, size_t ibo, size_t iboLen) {
  if(mat.tex==nullptr) {
    Tempest::Log::e("no texture?!");
    return ObjectsBucket::Item();
    }
  auto&        bucket = getBucket(mat,nullptr,ObjectsBucket::Animated);
  const size_t id     = bucket.alloc(mesh.vbo,mesh.ibo,ibo,iboLen,anim,mesh.bbox);
  return ObjectsBucket::Item(bucket,id);
  }

ObjectsBucket::Item VisualObjects::get(Tempest::VertexBuffer<Resources::Vertex>& vbo, Tempest::IndexBuffer<uint32_t>& ibo,
                                       const Material& mat, const Bounds& bbox, ObjectsBucket::Type type) {
  if(mat.tex==nullptr) {
    Tempest::Log::e("no texture?!");
    return ObjectsBucket::Item();
    }
  auto&        bucket = getBucket(mat,nullptr,type);
  const size_t id     = bucket.alloc(vbo,ibo,0,ibo.size(),bbox);
  return ObjectsBucket::Item(bucket,id);
  }

ObjectsBucket::Item VisualObjects::get(const Tempest::VertexBuffer<Resources::Vertex>* vbo[], const Material& mat, const Bounds& bbox) {
  if(mat.tex==nullptr) {
    Tempest::Log::e("no texture?!");
    return ObjectsBucket::Item();
    }
  auto&        bucket = getBucket(mat,nullptr,ObjectsBucket::Pfx);
  const size_t id     = bucket.alloc(vbo,bbox);
  return ObjectsBucket::Item(bucket,id);
  }

SkeletalStorage::AnimationId VisualObjects::getAnim(size_t boneCnt) {
  if(boneCnt==0)
    return SkeletalStorage::AnimationId();
  auto id = skinedAnim.alloc(boneCnt);
  return SkeletalStorage::AnimationId(skinedAnim,id,boneCnt);
  }

void VisualObjects::setupUbo() {
  for(auto& c:buckets)
    c.setupUbo();
  sky->setupUbo();
  }

void VisualObjects::preFrameUpdate(uint8_t fId) {
  for(auto& c:buckets)
    c.preFrameUpdate(fId);
  }

void VisualObjects::visibilityPass(const Frustrum fr[], const Tempest::Pixmap& hiZ) {
  for(auto& i:buckets)
    i.resetVis();
  visGroup.pass(fr,hiZ);
  }

void VisualObjects::draw(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId) {
  mkIndex();
  commitUbo(fId);

  sky->drawSky(enc,fId);
  for(size_t i=lastSolidBucket;i<index.size();++i) {
    auto c = index[i];
    c->draw(enc,fId);
    }
  sky->drawFog(enc,fId);
  }

void VisualObjects::drawGBufferOcc(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId) {
  mkIndex();
  commitUbo(fId);

  for(size_t i=0; i<lastOccluderBucket; ++i) {
    auto c = index[i];
    c->drawGBuffer(enc,fId);
    }
  }

void VisualObjects::drawGBuffer(Tempest::Encoder<CommandBuffer>& enc, uint8_t fId) {
  mkIndex();
  commitUbo(fId);

  for(size_t i=lastOccluderBucket; i<lastSolidBucket; ++i) {
    auto c = index[i];
    c->drawGBuffer(enc,fId);
    }
  }

void VisualObjects::drawShadow(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId, int layer) {
  if(layer==0) {
    mkIndex();
    commitUbo(fId);
    }

  for(size_t i=0; i<lastSolidBucket; ++i) {
    auto c = index[i];
    c->drawShadow(enc,fId,layer);
    }
  }

void VisualObjects::setWorld(const World& world) {
  sky->setWorld(world);
  }

void VisualObjects::setDayNight(float dayF) {
  sky->setDayNight(dayF);
  }

void VisualObjects::resetIndex() {
  index.clear();
  }

void VisualObjects::mkIndex() {
  if(index.size()!=0)
    return;
  index.reserve(buckets.size());
  index.resize(buckets.size());
  size_t id=0;
  for(auto& i:buckets) {
    if(i.size()==0)
      continue;
    index[id] = &i;
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

    const int lt = l->isOccluder() ? 0 : 1;
    const int rt = r->isOccluder() ? 0 : 1;

    if(lt<rt)
      return true;
    if(lt>rt)
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

  lastOccluderBucket = lastSolidBucket;
  for(size_t i=0;i<index.size();++i) {
    auto c = index[i];
    if(!c->isOccluder()) {
      lastOccluderBucket = i;
      break;
      }
    }
  }

void VisualObjects::commitUbo(uint8_t fId) {
  bool  sk = skinedAnim.commitUbo(fId);
  bool  st = uboStatic .commitUbo(fId);
  bool  dn = uboDyn    .commitUbo(fId);

  if(!st && !dn && !sk)
    return;

  for(auto& c:buckets)
    c.invalidateUbo();
  }

