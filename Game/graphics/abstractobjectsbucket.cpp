#include "abstractobjectsbucket.h"

void AbstractObjectsBucket::Item::setObjMatrix(const Tempest::Matrix4x4 &mt) {
  owner->setObjectMatrix(id,mt);
  }

AbstractObjectsBucket::AbstractObjectsBucket(Tempest::Device &device, const Tempest::UniformsLayout &layout)
  :pfSize(device.maxFramesInFlight()) {
  pf.reset(new PerFrame[pfSize]);
  for(size_t i=0;i<pfSize;++i)
    pf[i].ubo = device.uniforms(layout);
  }

size_t AbstractObjectsBucket::alloc(const Tempest::VertexBuffer<Resources::Vertex> &vbo, const Tempest::IndexBuffer<uint32_t> &ibo){
  const size_t id=getNextId();
  markAsChanged();

  data[id].vbo = &vbo;
  data[id].ibo = &ibo;
  return data.size()-1;
  }

void AbstractObjectsBucket::free(const size_t objId) {
  markAsChanged();
  setObjectMatrix(objId,Tempest::Matrix4x4());
  freeList.emplace_back(objId);
  }

size_t AbstractObjectsBucket::getNextId() {
  if(freeList.size()>0){
    markAsChanged();
    size_t id=freeList.back();
    freeList.pop_back();
    return id;
    }
  onResizeStorage(data.size()+1);
  return data.size()-1;
  }

void AbstractObjectsBucket::markAsChanged() {
  for(uint32_t i=0;i<pfSize;++i)
    pf[i].uboChanged=true;
  }
