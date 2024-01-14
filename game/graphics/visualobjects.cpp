#include "visualobjects.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/animmesh.h"

using namespace Tempest;

VisualObjects::VisualObjects(const SceneGlobals& globals, const std::pair<Vec3, Vec3>& bbox)
    : globals(globals), visGroup(bbox), drawMem(globals) {
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
                                       const InstanceStorage::Id& anim) {
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

DrawStorage::Item VisualObjects::getDr(const StaticMesh& mesh, const Material& mat,
                                       size_t iboOff, size_t iboLen, const Tempest::StorageBuffer& desc,
                                       DrawStorage::Type bucket) {
  return drawMem.alloc(mesh, mat, iboOff, iboLen, desc, bucket);
  }

InstanceStorage::Id VisualObjects::alloc(size_t size) {
  return instanceMem.alloc(size);
  }

bool VisualObjects::realloc(InstanceStorage::Id& id, size_t size) {
  return instanceMem.realloc(id, size);
  }

const Tempest::StorageBuffer& VisualObjects::instanceSsbo() const {
  return instanceMem.ssbo();
  }

void VisualObjects::prepareUniforms() {
  drawMem.prepareUniforms();
  for(auto& c:buckets)
    c->prepareUniforms();
  }

void VisualObjects::preFrameUpdate(uint8_t fId) {
  mkIndex();
  for(auto& c:buckets)
    c->preFrameUpdate(fId);
  }

void VisualObjects::visibilityPass(const Frustrum fr[]) {
  visGroup.pass(fr);
  }

void VisualObjects::visibilityPass (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t fId) {
  drawMem.visibilityPass(cmd, fId);
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

void VisualObjects::drawGBuffer(Tempest::Encoder<CommandBuffer>& enc, uint8_t fId, uint8_t pass) {
  if(pass==0) {
    drawMem.drawGBuffer(enc, fId);
    return;
    }

  for(size_t i=0;i<lastSolidBucket;++i) {
    auto c = index[i];
    c->drawGBuffer(enc,fId);
    }
  }

void VisualObjects::drawShadow(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId, int layer) {
  drawMem.drawShadow(enc, fId, layer);

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

void VisualObjects::notifyTlas(const Material& m, RtScene::Category cat) {
  globals.rtScene.notifyTlas(m,cat);
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

    const int lt    = l->type()==ObjectsBucket::Landscape ? 0 : 1;
    const int rt    = r->type()==ObjectsBucket::Landscape ? 0 : 1;

    const bool lpso = l->pso();
    const bool rpso = r->pso();

    const auto lv   = l->meshPointer();
    const auto rv   = r->meshPointer();

    return std::tie(lt, lpso, lv, lm.tex) < std::tie(rt, rpso, rv, rm.tex);
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

void VisualObjects::prepareGlobals(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  bool sk = instanceMem.commit(cmd, fId);
  if(!sk)
    return;
  for(auto& c:buckets)
    c->invalidateUbo(fId);
  }

void VisualObjects::postFrameupdate() {
  instanceMem.join();
  }

bool VisualObjects::updateRtScene(RtScene& out) {
  if(!out.isUpdateRequired())
    return false;

  for(auto& c:buckets)
    c->fillTlas(out);

  out.buildTlas();
  return true;
  }
