#include "visualobjects.h"

#include <Tempest/Log>

#include "graphics/submesh/animmesh.h"
#include "graphics/dynamic/painter3d.h"
#include "utils/workers.h"
#include "rendererstorage.h"

using namespace Tempest;

VisualObjects::VisualObjects(const SceneGlobals& globals)
  :globals(globals), sky(globals) {
  }

ObjectsBucket& VisualObjects::getBucket(const Material& mat, ObjectsBucket::Type type) {
  for(auto& i:buckets)
    if(i.material()==mat && i.type()==type && i.size()<ObjectsBucket::CAPACITY)
      return i;

  if(type==ObjectsBucket::Type::Static)
    buckets.emplace_back(mat,globals,uboStatic,type); else
    buckets.emplace_back(mat,globals,uboDyn,   type);
  return buckets.back();
  }

ObjectsBucket::Item VisualObjects::get(const StaticMesh &mesh, const Material& mat,
                                       const Tempest::IndexBuffer<uint32_t>& ibo,
                                       bool staticDraw) {
  if(mat.tex==nullptr) {
    Log::e("no texture?!");
    return ObjectsBucket::Item();
    }
  auto&        bucket = getBucket(mat,staticDraw ? ObjectsBucket::Static : ObjectsBucket::Movable);
  if(bucket.size()==0)
    index.clear();
  const size_t id     = bucket.alloc(mesh.vbo,ibo,mesh.bbox);
  return ObjectsBucket::Item(bucket,id);
  }

ObjectsBucket::Item VisualObjects::get(const AnimMesh &mesh, const Material& mat,
                                       const Tempest::IndexBuffer<uint32_t> &ibo) {
  if(mat.tex==nullptr) {
    Tempest::Log::e("no texture?!");
    return ObjectsBucket::Item();
    }
  auto&        bucket = getBucket(mat,ObjectsBucket::Animated);
  if(bucket.size()==0)
    index.clear();
  const size_t id     = bucket.alloc(mesh.vbo,ibo,mesh.bbox);
  return ObjectsBucket::Item(bucket,id);
  }

ObjectsBucket::Item VisualObjects::get(Tempest::VertexBuffer<Resources::Vertex>& vbo, Tempest::IndexBuffer<uint32_t>& ibo,
                                       const Material& mat, const Bounds& bbox) {
  if(mat.tex==nullptr) {
    Tempest::Log::e("no texture?!");
    return ObjectsBucket::Item();
    }
  auto& bucket = getBucket(mat,ObjectsBucket::Static);
  if(bucket.size()==0)
    index.clear();
  const size_t id     = bucket.alloc(vbo,ibo,bbox);
  return ObjectsBucket::Item(bucket,id);
  }

ObjectsBucket::Item VisualObjects::get(const Tempest::VertexBuffer<Resources::Vertex>* vbo[], const Material& mat, const Bounds& bbox) {
  if(mat.tex==nullptr) {
    Tempest::Log::e("no texture?!");
    return ObjectsBucket::Item();
    }
  auto& bucket = getBucket(mat,ObjectsBucket::Movable);
  if(bucket.size()==0)
    index.clear();
  const size_t id     = bucket.alloc(vbo,bbox);
  return ObjectsBucket::Item(bucket,id);
  }

void VisualObjects::setupUbo() {
  for(auto& c:buckets)
    c.setupUbo();
  sky.setupUbo();
  }

void VisualObjects::preFrameUpdate(uint8_t fId) {
  for(auto& c:buckets)
    c.preFrameUpdate(fId);
  }

void VisualObjects::draw(Tempest::Encoder<Tempest::CommandBuffer>& enc, Painter3d& /*painter*/, uint8_t fId) {
  mkIndex();
  commitUbo(fId);

  sky.drawSky(enc,fId);
  for(size_t i=lastSolidBucket;i<index.size();++i) {
    auto c = index[i];
    c->draw(enc,fId);
    }
  sky.drawFog(enc,fId);
  }

void VisualObjects::drawGBuffer(Tempest::Encoder<CommandBuffer>& enc, Painter3d& painter, uint8_t fId) {
  mkIndex();

  Workers::parallelFor(index,[&painter](ObjectsBucket* c){
    c->visibilityPass(painter);
    });

  commitUbo(fId);

  for(size_t i=0;i<lastSolidBucket;++i) {
    auto c = index[i];
    c->drawGBuffer(enc,fId);
    }
  }

void VisualObjects::drawShadow(Tempest::Encoder<Tempest::CommandBuffer>& enc, Painter3d& painter, uint8_t fId, int layer) {
  if(layer+1==Resources::ShadowLayers) {
    mkIndex();
    Workers::parallelFor(index.data(),index.data()+lastSolidBucket,[&painter](ObjectsBucket* c){
      c->visibilityPass(painter);
      });
    commitUbo(fId);
    } else {
    Workers::parallelFor(index.data(),index.data()+lastSolidBucket,[&painter](ObjectsBucket* c){
      c->visibilityPassAnd(painter);
      });
    }

  for(size_t i=0;i<lastSolidBucket;++i) {
    auto c = index[i];
    c->drawShadow(enc,fId,layer);
    }
  }

void VisualObjects::setWorld(const World& world) {
  sky.setWorld(world);
  }

void VisualObjects::setDayNight(float dayF) {
  sky.setDayNight(dayF);
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

  std::sort(index.begin(),index.end(),[](const ObjectsBucket* l,const ObjectsBucket* r){
    auto& lm = l->material();
    auto& rm = r->material();

    if(lm.alphaOrder()<rm.alphaOrder())
      return true;
    if(lm.alphaOrder()>rm.alphaOrder())
      return false;

    if(l->avgPoligons()<r->avgPoligons())
      return false; //inverted
    if(l->avgPoligons()>r->avgPoligons())
      return true;
    return lm.tex < rm.tex;
    });
  lastSolidBucket = index.size();
  for(size_t i=0;i<index.size();++i) {
    auto c = index[i];
    if(c->material().alpha!=Material::Solid && c->material().alpha!=Material::AlphaTest) {
      lastSolidBucket = i;
      break;
      }
    }
  }

void VisualObjects::commitUbo(uint8_t fId) {
  bool st = uboStatic.commitUbo(globals.storage.device,fId);
  bool dn = uboDyn   .commitUbo(globals.storage.device,fId);

  if(!st && !dn)
    return;
  for(auto& c:buckets)
    c.invalidateUbo();
  }
