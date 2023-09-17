#include "instancestorage.h"

#include <cstdint>

static uint32_t nextPot(uint32_t v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
  }

static uint32_t alignAs(uint32_t sz, uint32_t alignment) {
  return ((sz+alignment-1)/alignment)*alignment;
  }

using namespace Tempest;

InstanceStorage::Id::Id(Id&& other) noexcept
  :heapPtr(other.heapPtr), rgn(other.rgn) {
  other.heapPtr = nullptr;
  }

InstanceStorage::Id& InstanceStorage::Id::operator =(Id&& other) noexcept {
  std::swap(heapPtr, other.heapPtr);
  std::swap(rgn,     other.rgn);
  return *this;
  }

InstanceStorage::Id::~Id() {
  if(heapPtr!=nullptr)
    heapPtr->owner->free(*heapPtr, rgn);
  }

void InstanceStorage::Id::set(const Tempest::Matrix4x4* mat) {
  if(heapPtr!=nullptr) {
    auto data = reinterpret_cast<Matrix4x4*>(heapPtr->data.data() + rgn.begin);
    std::memcpy(data, mat, rgn.asize);
    for(uint8_t i=0; i<Resources::MaxFramesInFlight; ++i)
      heapPtr->durty[i].store(true);
    }
  }

void InstanceStorage::Id::set(const Tempest::Matrix4x4& obj, size_t offset) {
  if(heapPtr==nullptr)
    return;

  auto data = reinterpret_cast<Matrix4x4*>(heapPtr->data.data() + rgn.begin);
  data[offset] = obj;

  if(heapPtr==&heapPtr->owner->upload)
    return;
  for(uint8_t i=0; i<Resources::MaxFramesInFlight; ++i)
    heapPtr->durty[i].store(true);
  }

const StorageBuffer& InstanceStorage::Id::ssbo(uint8_t fId) const {
  if(heapPtr!=nullptr)
    return heapPtr->gpu[fId];
  static StorageBuffer ssbo;
  return ssbo;
  }

BufferHeap InstanceStorage::Id::heap() const {
  if(heapPtr!=nullptr)
    return heapPtr==&heapPtr->owner->device ? BufferHeap::Device : BufferHeap::Upload;
  return BufferHeap::Upload;
  }


InstanceStorage::InstanceStorage() {
  upload.data.reserve(131072);
  upload.data.resize(sizeof(Matrix4x4));
  reinterpret_cast<Matrix4x4*>(upload.data.data())->identity();
  upload.owner = this;

  device.data.reserve(131072);
  device.data.resize(sizeof(Matrix4x4));
  reinterpret_cast<Matrix4x4*>(device.data.data())->identity();
  device.owner = this;
  }

bool InstanceStorage::commit(uint8_t fId) {
  bool ret = commit(upload,fId);
  if(device.durty[fId].load()) {
    ret |= commit(device,fId);
    device.durty[fId].store(false);
    }
  return ret;
  }

bool InstanceStorage::commit(Heap& heap, uint8_t fId) {
  auto&  obj = heap.gpu[fId];
  size_t sz  = heap.data.size();
  if(obj.byteSize()==sz) {
    obj.update(heap.data);
    return false;
    }
  auto  bh     = (&heap==&upload ? BufferHeap::Upload : BufferHeap::Device);
  auto& device = Resources::device();
  obj = device.ssbo(bh,heap.data.data(),sz);
  return true;
  }

InstanceStorage::Id InstanceStorage::alloc(BufferHeap heap, const size_t size) {
  if(size==0)
    return Id(upload,Range());

  const auto nsize = alignAs(nextPot(uint32_t(size)), alignment);

  auto& h = (heap==BufferHeap::Upload ? upload : device);
  for(size_t i=0; i<h.rgn.size(); ++i) {
    if(h.rgn[i].size==nsize) {
      auto ret = h.rgn[i];
      ret.asize = size;
      h.rgn.erase(h.rgn.begin()+intptr_t(i));
      return Id(h,ret);
      }
    }
  size_t retId = size_t(-1);
  for(size_t i=0; i<h.rgn.size(); ++i) {
    if(h.rgn[i].size>nsize && (retId==size_t(-1) || h.rgn[i].size<h.rgn[retId].size)) {
      retId = i;
      }
    }
  if(retId!=size_t(-1)) {
    Range ret = h.rgn[retId];
    ret.size  = nsize;
    ret.asize = size;
    h.rgn[retId].begin += nsize;
    h.rgn[retId].size  -= nsize;
    return Id(h,ret);
    }
  Range r;
  r.begin = h.data.size();
  r.size  = nsize;
  r.asize = size;
  h.data.resize(h.data.size()+nsize);
  return Id(h,r);
  }

const Tempest::StorageBuffer& InstanceStorage::ssbo(Tempest::BufferHeap heap, uint8_t fId) const {
  if(heap==BufferHeap::Upload)
    return upload.gpu[fId];
  return device.gpu[fId];
  }

void InstanceStorage::free(Heap& heap, const Range& r) {
  for(auto& i:heap.rgn) {
    if(i.begin+i.size==r.begin) {
      i.size  += r.size;
      return;
      }
    if(r.begin+r.size==i.begin) {
      i.begin -= r.size;
      i.size  += r.size;
      return;
      }
    }
  auto at = std::lower_bound(heap.rgn.begin(),heap.rgn.end(),r,[](const Range& l, const Range& r){
    return l.begin<r.begin;
    });
  heap.rgn.insert(at,r);
  }
