#include "abstractobjectsbucket.h"

void AbstractObjectsBucket::Item::setObjMatrix(const Tempest::Matrix4x4 &mt) {
  owner->markAsChanged();
  owner->setObjectMatrix(id,mt);
  }

void AbstractObjectsBucket::Item::setSkeleton(const Skeleton *sk) {
  owner->markAsChanged();
  owner->setSkeleton(id,sk);
  }

AbstractObjectsBucket::AbstractObjectsBucket(Tempest::Device &device, const Tempest::UniformsLayout &layout)
  :pfSize(device.maxFramesInFlight()) {
  pf.reset(new PerFrame[pfSize]);
  for(size_t i=0;i<pfSize;++i)
    pf[i].ubo = device.uniforms(layout);
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

  const size_t size=storageSize();
  onResizeStorage(size+1);
  return size;
  }

void AbstractObjectsBucket::markAsChanged() {
  for(uint32_t i=0;i<pfSize;++i)
    pf[i].uboChanged=true;
  }
