#include "visualobjects.h"

#include <Tempest/Log>

#include "graphics/submesh/animmesh.h"
#include "utils/workers.h"
#include "rendererstorage.h"

using namespace Tempest;

VisualObjects::VisualObjects(const SceneGlobals& globals)
  :globals(globals), sky(globals) {
  }

ObjectsBucket& VisualObjects::getBucket(const Material& mat, ObjectsBucket::Type type) {
  for(auto& i:buckets)
    if(i.material()==mat && i.type()==type)
      return i;

  index.clear();
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
  const size_t id     = bucket.alloc(mesh.vbo,ibo,mesh.bbox);
  return ObjectsBucket::Item(bucket,id);
  }

ObjectsBucket::Item VisualObjects::get(Tempest::VertexBuffer<Resources::Vertex>& vbo, Tempest::IndexBuffer<uint32_t>& ibo,
                                       const Material& mat, const Bounds& bbox) {
  if(mat.tex==nullptr) {
    Tempest::Log::e("no texture?!");
    return ObjectsBucket::Item();
    }
  auto&        bucket = getBucket(mat,ObjectsBucket::Static);
  const size_t id     = bucket.alloc(vbo,ibo,bbox);
  return ObjectsBucket::Item(bucket,id);
  }

ObjectsBucket::Item VisualObjects::get(const Tempest::VertexBuffer<Resources::Vertex>* vbo[], const Material& mat, const Bounds& bbox) {
  if(mat.tex==nullptr) {
    Tempest::Log::e("no texture?!");
    return ObjectsBucket::Item();
    }
  auto&        bucket = getBucket(mat,ObjectsBucket::Movable);
  const size_t id     = bucket.alloc(vbo,bbox);
  return ObjectsBucket::Item(bucket,id);
  }

void VisualObjects::setupUbo() {
  for(auto& c:buckets)
    c.setupUbo();
  }

void VisualObjects::preFrameUpdate(uint8_t fId) {
  for(auto& c:buckets)
    c.preFrameUpdate(fId);
  }

void VisualObjects::draw(Painter3d& painter, Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId) {
  commitUbo(fId);
  mkIndex();

  Workers::parallelFor(index,[&painter](ObjectsBucket* c){
    c->visibilityPass(painter);
    });

  size_t i=0;
  for(;i<index.size();++i) {
    auto c = index[i];
    if(c->material().alpha!=Material::Solid && c->material().alpha!=Material::AlphaTest)
      break;
    c->draw(enc,fId);
    }
  sky.draw(enc,fId);
  for(;i<index.size();++i) {
    auto c = index[i];
    c->draw(enc,fId);
    }

  for(auto c:index)
    c->drawLight(enc,fId);
  }

void VisualObjects::drawShadow(Painter3d& painter, Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId, int layer) {
  commitUbo(fId);
  mkIndex();

  Workers::parallelFor(index,[&painter](ObjectsBucket* c){
    c->visibilityPass(painter);
    });

  for(auto c:index)
    c->drawShadow(enc,fId,layer);
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
    index[id] = &i;
    ++id;
    }
  std::sort(index.begin(),index.end(),[](const ObjectsBucket* l,const ObjectsBucket* r){
    return l->material()<r->material();
    });
  }

void VisualObjects::commitUbo(uint8_t fId) {
  bool st = uboStatic.commitUbo(globals.storage.device,fId);
  bool dn = uboDyn   .commitUbo(globals.storage.device,fId);

  if(!st && !dn)
    return;
  for(auto& c:buckets)
    c.invalidateUbo();
  }
