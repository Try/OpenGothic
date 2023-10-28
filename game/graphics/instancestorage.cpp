#include "instancestorage.h"
#include "shaders.h"

#include <Tempest/Log>
#include <cstdint>
#include <atomic>

using namespace Tempest;

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

static void bitSet(std::vector<uint32_t>& b, size_t id) {
  static_assert(sizeof(std::atomic<uint32_t>)==sizeof(uint32_t));
  auto& bits = b[id/32];
  id %= 32;
  reinterpret_cast<std::atomic<uint32_t>&>(bits).fetch_or(1u << id, std::memory_order_relaxed);
  }

static bool bitAt(std::vector<uint32_t>& b, size_t id) {
  auto bits = b[id/32];
  id %= 32;
  return bits & (1u << id);
  }

using namespace Tempest;

InstanceStorage::Id::Id(Id&& other) noexcept
  :owner(other.owner), rgn(other.rgn) {
  other.owner = nullptr;
  }

InstanceStorage::Id& InstanceStorage::Id::operator = (Id&& other) noexcept {
  std::swap(owner, other.owner);
  std::swap(rgn,   other.rgn);
  return *this;
  }

InstanceStorage::Id::~Id() {
  if(owner!=nullptr)
    owner->free(rgn);
  }

void InstanceStorage::Id::set(const Tempest::Matrix4x4* mat) {
  if(owner==nullptr)
    return;

  auto data = reinterpret_cast<Matrix4x4*>(owner->dataCpu.data() + rgn.begin);
  std::memcpy(data, mat, rgn.asize);

  for(size_t i=0; i<rgn.asize; i+=blockSz)
    bitSet(owner->durty, (rgn.begin+i)/blockSz);
  }

void InstanceStorage::Id::set(const Tempest::Matrix4x4& obj, size_t offset) {
  if(owner==nullptr)
    return;

  auto data = reinterpret_cast<Matrix4x4*>(owner->dataCpu.data() + rgn.begin);
  if(data[offset] == obj)
    return;
  data[offset] = obj;
  bitSet(owner->durty, (rgn.begin+offset*sizeof(Matrix4x4))/blockSz);
  }

void InstanceStorage::Id::set(const void* data, size_t offset, size_t size) {
  if(owner==nullptr)
    return;

  auto src = reinterpret_cast<const uint8_t*>(data);
  auto dst = (owner->dataCpu.data() + rgn.begin + offset);

  if(std::memcmp(src, dst, size)==0)
    return;

  if((offset%blockSz)==0) {
    for(size_t i=0; i<size; i+=blockSz) {
      for(size_t r=0; r<blockSz && i+r<size; ++r) {
        dst[i+r] = src[i+r];
        }
      bitSet(owner->durty, (rgn.begin + offset + i)/blockSz);
      }
    } else {
    for(size_t i=0; i<size; ++i) {
      dst[i] = src[i];
      bitSet(owner->durty, (rgn.begin + offset + i)/blockSz);
      }
    }
  }


InstanceStorage::InstanceStorage() {
  dataCpu.reserve(131072);
  dataCpu.resize(sizeof(Matrix4x4));
  reinterpret_cast<Matrix4x4*>(dataCpu.data())->identity();

  patchCpu.reserve(4*1024*1024);
  patchBlock.reserve(1024);

  auto& device = Resources::device();
  for(auto& d:desc)
    d = device.descriptors(Shaders::inst().path);
  }

bool InstanceStorage::commit(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  auto& device = Resources::device();

  std::atomic_thread_fence(std::memory_order_acquire);
  if(dataGpu.byteSize()!=dataCpu.size()) {
    Resources::recycle(std::move(dataGpu));
    dataGpu = device.ssbo(BufferHeap::Device,dataCpu);
    std::memset(durty.data(), 0, durty.size()*sizeof(uint32_t));
    for(auto& i:resizeBit)
      i = true;
    resizeBit[fId] = false;
    return true;
    }

  const bool resized = resizeBit[fId];
  resizeBit[fId] = false;

  patchBlock.clear();
  size_t payloadSize = 0;
  for(size_t i = 0, len = durty.size(); i<len; ++i) {
    if(!bitAt(durty,i))
      continue;
    auto begin = i; ++i;
    while(i<len) {
      if(!bitAt(durty,i))
        break;
      ++i;
      }

    uint32_t size    = uint32_t((i-begin)*blockSz);
    uint32_t chunkSz = 256;

    Path p = {};
    p.dst  = uint32_t(begin*blockSz);
    p.src  = uint32_t(payloadSize);
    while(size>0) {
      p.size = std::min<uint32_t>(size, chunkSz);
      size        -= p.size;
      patchBlock.push_back(p);

      payloadSize += p.size;
      p.dst       += p.size;
      p.src       += p.size;
      }
    }
  std::memset(durty.data(), 0, durty.size()*sizeof(durty[0]));

  if(patchBlock.size()==0)
    return resized;

  auto& path = patchGpu[fId];
  const size_t headerSize = patchBlock.size()*sizeof(Path);
  if(path.byteSize() < headerSize + payloadSize) {
    path = device.ssbo(BufferHeap::Upload, nullptr, headerSize + payloadSize);
    }

  patchCpu.resize(headerSize + payloadSize);
  for(auto& i:patchBlock) {
    i.src += uint32_t(headerSize);
    std::memcpy(patchCpu.data()+i.src, dataCpu.data() + i.dst, i.size);

    // uint's in shader
    i.src  /= 4;
    i.dst  /= 4;
    i.size /= 4;
    }
  std::memcpy(patchCpu.data(), patchBlock.data(), headerSize);
  path.update(patchCpu);

  auto& d = desc[fId];
  d.set(0, dataGpu);
  d.set(1, path);

  cmd.setFramebuffer({});
  cmd.setUniforms(Shaders::inst().path, d);
  cmd.dispatch(patchBlock.size());
  return resized;
  }

InstanceStorage::Id InstanceStorage::alloc(const size_t size) {
  if(size==0)
    return Id(*this,Range());

  const auto nsize = alignAs(nextPot(uint32_t(size)), alignment);
  for(size_t i=0; i<rgn.size(); ++i) {
    if(rgn[i].size==nsize) {
      auto ret = rgn[i];
      ret.asize = size;
      rgn.erase(rgn.begin()+intptr_t(i));
      return Id(*this,ret);
      }
    }
  size_t retId = size_t(-1);
  for(size_t i=0; i<rgn.size(); ++i) {
    if(rgn[i].size>nsize && (retId==size_t(-1) || rgn[i].size<rgn[retId].size)) {
      retId = i;
      }
    }
  if(retId!=size_t(-1)) {
    Range ret = rgn[retId];
    ret.size  = nsize;
    ret.asize = size;
    rgn[retId].begin += nsize;
    rgn[retId].size  -= nsize;
    return Id(*this,ret);
    }
  Range r;
  r.begin = dataCpu.size();
  r.size  = nsize;
  r.asize = size;
  dataCpu.resize(dataCpu.size()+nsize);

  durty.resize((dataCpu.size()+blockSz-1)/blockSz, uint32_t(-1));
  return Id(*this,r);
  }

bool InstanceStorage::realloc(Id& id, const size_t size) {
  if(size==0) {
    if(id.isEmpty())
      return false;
    id = Id(*this,Range());
    return true;
    }

  if(size<=id.rgn.size) {
    id.rgn.asize = size;
    return false;
    }

  auto next = alloc(size);

  auto data = dataCpu.data();
  std::memcpy(data+next.rgn.begin, data+id.rgn.begin, id.rgn.asize);
  for(size_t i=0; i<id.rgn.asize; ++i) {
    bitSet(durty, (next.rgn.begin + i)/blockSz);
    }
  id = std::move(next);
  return true;
  }

const Tempest::StorageBuffer& InstanceStorage::ssbo() const {
  return dataGpu;
  }

void InstanceStorage::free(const Range& r) {
  for(auto& i:rgn) {
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
  auto at = std::lower_bound(rgn.begin(),rgn.end(),r,[](const Range& l, const Range& r){
    return l.begin<r.begin;
    });
  rgn.insert(at,r);
  }
