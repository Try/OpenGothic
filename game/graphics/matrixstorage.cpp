#include "matrixstorage.h"

#include <cstdint>

#include "graphics/mesh/pose.h"

using namespace Tempest;

MatrixStorage::Id::Id(Id&& other)
  :heapPtr(other.heapPtr), rgn(other.rgn) {
  other.heapPtr = nullptr;
  }

MatrixStorage::Id& MatrixStorage::Id::operator =(Id&& other) {
  std::swap(heapPtr, other.heapPtr);
  std::swap(rgn,     other.rgn);
  return *this;
  }

MatrixStorage::Id::~Id() {
  if(heapPtr!=nullptr)
    heapPtr->owner->free(*heapPtr, rgn);
  }

void MatrixStorage::Id::set(const Tempest::Matrix4x4* mat) {
  if(heapPtr!=nullptr) {
    std::memcpy(heapPtr->data.data()+rgn.begin, mat, rgn.size*sizeof(Tempest::Matrix4x4));
    for(uint8_t i=0; i<Resources::MaxFramesInFlight; ++i)
      heapPtr->durty[i].store(true);
    }
  }

void MatrixStorage::Id::set(const Tempest::Matrix4x4& obj, size_t offset) {
  if(heapPtr==nullptr)
    return;
  heapPtr->data[rgn.begin+offset] = obj;

  if(heapPtr==&heapPtr->owner->upload)
    return;
  for(uint8_t i=0; i<Resources::MaxFramesInFlight; ++i)
    heapPtr->durty[i].store(true);
  }

const StorageBuffer& MatrixStorage::Id::ssbo(uint8_t fId) const {
  if(heapPtr!=nullptr)
    return heapPtr->gpu[fId];
  static StorageBuffer ssbo;
  return ssbo;
  }

BufferHeap MatrixStorage::Id::heap() const {
  if(heapPtr!=nullptr)
    return heapPtr==&heapPtr->owner->device ? BufferHeap::Device : BufferHeap::Upload;
  return BufferHeap::Upload;
  }


MatrixStorage::MatrixStorage() {
  upload.data.reserve(2048);
  upload.data.resize(1);
  upload.data[0].identity();
  upload.owner = this;

  device.data.reserve(2048);
  device.data.resize(1);
  device.data[0].identity();
  device.owner = this;
  }

bool MatrixStorage::commit(uint8_t fId) {
  bool ret = commit(upload,fId);
  if(device.durty[fId].load()) {
    ret |= commit(device,fId);
    device.durty[fId].store(false);
    }
  return ret;
  }

bool MatrixStorage::commit(Heap& heap, uint8_t fId) {
  auto&  obj = heap.gpu[fId];
  size_t sz  = heap.data.size() * sizeof(Tempest::Matrix4x4);
  if(obj.byteSize()==sz) {
    obj.update(heap.data);
    return false;
    }
  auto  bh     = (&heap==&upload ? BufferHeap::Upload : BufferHeap::Device);
  auto& device = Resources::device();
  obj = device.ssbo(bh,heap.data.data(),sz);
  return true;
  }

MatrixStorage::Id MatrixStorage::alloc(BufferHeap heap, size_t nbones) {
  if(nbones==0)
    return Id(upload,Range());

  auto& h = (heap==BufferHeap::Upload ? upload : device);
  for(size_t i=0; i<h.rgn.size(); ++i) {
    if(h.rgn[i].size==nbones) {
      auto ret = h.rgn[i];
      h.rgn.erase(h.rgn.begin()+intptr_t(i));
      return Id(h,ret);
      }
    }
  size_t retId = size_t(-1);
  for(size_t i=0; i<h.rgn.size(); ++i) {
    if(h.rgn[i].size>nbones && (retId==size_t(-1) || h.rgn[i].size<h.rgn[retId].size)) {
      retId = i;
      }
    }
  if(retId!=size_t(-1)) {
    auto ret = h.rgn[retId];
    ret.size = nbones;
    h.rgn[retId].begin += nbones;
    h.rgn[retId].size  -= nbones;
    return Id(h,ret);
    }
  Range r;
  r.begin = h.data.size();
  r.size  = nbones;
  h.data.resize(h.data.size()+r.size);
  return Id(h,r);
  }

const Tempest::StorageBuffer& MatrixStorage::ssbo(Tempest::BufferHeap heap, uint8_t fId) const {
  if(heap==BufferHeap::Upload)
    return upload.gpu[fId];
  return device.gpu[fId];
  }

void MatrixStorage::free(Heap& heap, const Range& r) {
  for(auto& i:heap.rgn) {
    if(i.begin+i.size==r.begin) {
      i.size+=r.size;
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
