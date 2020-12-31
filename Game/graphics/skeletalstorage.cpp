#include "skeletalstorage.h"

using namespace Tempest;

struct SkeletalStorage::Impl {
  virtual ~Impl() = default;

  virtual size_t alloc(size_t bonesCount) = 0;
  virtual void   free (const size_t objId, const size_t bonesCount) = 0;
  virtual void   bind(Uniforms& ubo, uint8_t bind, uint8_t fId, size_t id) = 0;
  virtual bool   commitUbo(Tempest::Device &device, uint8_t fId) = 0;
  virtual Matrix4x4* get(size_t id) = 0;
  virtual void   reserve(size_t n) = 0;

  void           markAsChanged(size_t/*elt*/) {
    for(auto& i:pf)
      i.uboChanged=true;
    }

  struct PerFrame final {
    std::atomic_bool uboChanged{false};  // invalidate ubo array
    };
  PerFrame           pf[Resources::MaxFramesInFlight];
  };

template<size_t BlkSz>
struct SkeletalStorage::FreeList<Resources::MAX_NUM_SKELETAL_NODES,BlkSz> {
  FreeList(){ freeList.reserve(2); }

  size_t alloc(size_t /*bonesCount*/) {
    if(freeList.size()>0){
      size_t id=freeList.back();
      freeList.pop_back();
      return id;
      }
    return size_t(-1);
    }

  void free(const size_t objId, const size_t /*bonesCount*/) {
    freeList.push_back(objId);
    }

  size_t blockCount(const size_t /*bonesCount*/) const {
    return Resources::MAX_NUM_SKELETAL_NODES/BlkSz;
    }

  std::vector<size_t> freeList;
  };

template<size_t sz, size_t BlkSz>
struct SkeletalStorage::FreeList : FreeList<sz*2,BlkSz> {
  FreeList(){ freeList.reserve(2); }

  size_t alloc(size_t bonesCount) {
    if(bonesCount>sz)
      return FreeList<sz*2,BlkSz>::alloc(bonesCount);
    if(freeList.size()>0){
      size_t id=freeList.back();
      freeList.pop_back();
      return id;
      }
    // size_t ret = FreeList<sz*2,BlkSz>::alloc(bonesCount);
    // if(ret!=size_t(-1))
    //   freeList.push_back(ret+sz/BlkSz);
    return size_t(-1);
    }

  void free(const size_t objId, const size_t bonesCount) {
    if(bonesCount>sz)
      return FreeList<sz*2,BlkSz>::free(objId,bonesCount);
    freeList.push_back(objId);
    }

  size_t blockCount(const size_t bonesCount) const {
    if(bonesCount>sz)
      return FreeList<sz*2,BlkSz>::blockCount(bonesCount);
    return sz/BlkSz;
    }

  std::vector<size_t> freeList;
  };

template<size_t BlkSz>
struct SkeletalStorage::TImpl : Impl {
  struct Block {
    Tempest::Matrix4x4 mat[BlkSz];
    };

  enum {
    // do padding in the end, to make shader access to a valid mat4[MAX_NUM_SKELETAL_NODES] array
    Padding = Resources::MAX_NUM_SKELETAL_NODES-BlkSz
    };

  TImpl() {
    obj.resize(Padding);
    }

  size_t alloc(size_t bonesCount) override {
    size_t ret = freeList.alloc(bonesCount);
    if(ret!=size_t(-1))
      return ret;
    markAsChanged(ret);
    ret = obj.size()-Padding;

    const size_t increment = freeList.blockCount(bonesCount);
    obj.resize(obj.size()+increment);
    return ret;
    }

  void   free(const size_t objId, const size_t bonesCount) override {
    freeList.free(objId, bonesCount);
    auto m = &obj[objId];
    for (size_t i=0; i<bonesCount; i++) {
      m[i] = Block();
    }
    }

  void   bind(Uniforms& ubo, uint8_t bind, uint8_t fId, size_t id) override {
    auto& v = uboData[fId];
    ubo.set(bind,v,id);
    }

  bool   commitUbo(Tempest::Device &device, uint8_t fId) override {
    auto&        frame   = pf[fId];
    if(!frame.uboChanged)
      return false;

    const bool realloc = uboData[fId].size()!=obj.size();
    if(realloc)
      uboData[fId] = device.ubo<Block>(obj.data(),obj.size()); else
      uboData[fId].update(obj.data(),0,obj.size());
    frame.uboChanged = false;
    return realloc;
    }

  Matrix4x4* get(size_t id) override {
    return obj[id].mat;
    }

  void   reserve(size_t n) override {
    obj.reserve(n);
    }

  FreeList<BlkSz,BlkSz>           freeList;
  std::vector<Block>              obj;
  Tempest::UniformBuffer<Block>   uboData[Resources::MaxFramesInFlight];
  };

SkeletalStorage::SkeletalStorage(Tempest::Device& device) {
  if(tryInit<Resources::MAX_NUM_SKELETAL_NODES/16>(device))
    return;
  if(tryInit<Resources::MAX_NUM_SKELETAL_NODES/8>(device))
    return;
  if(tryInit<Resources::MAX_NUM_SKELETAL_NODES/4>(device))
    return;
  if(tryInit<Resources::MAX_NUM_SKELETAL_NODES/2>(device))
    return;
  blockSize = Resources::MAX_NUM_SKELETAL_NODES;
  impl.reset(new TImpl<Resources::MAX_NUM_SKELETAL_NODES>());
  }

SkeletalStorage::~SkeletalStorage() {
  }

template<size_t sz>
bool SkeletalStorage::tryInit(Tempest::Device& device) {
  const auto   align  = device.properties().ubo.offsetAlign;
  if(Resources::MAX_NUM_SKELETAL_NODES%sz==0 && (sizeof(Matrix4x4)*sz)%align==0) {
    blockSize = sz;
    impl.reset(new TImpl<sz>());
    return true;
    }
  return false;
  }

size_t SkeletalStorage::alloc(size_t bonesCount) {
  return impl->alloc(bonesCount);
  }

void SkeletalStorage::free(const size_t objId, size_t bonesCount) {
  impl->free(objId,bonesCount);
  }

void SkeletalStorage::bind(Uniforms& ubo, uint8_t bind, uint8_t fId, size_t id, size_t /*boneCnt*/) {
  impl->bind(ubo,bind,fId,id);
  }

bool SkeletalStorage::commitUbo(Tempest::Device& device, uint8_t fId) {
  if(!impl->commitUbo(device,fId))
    return false;
  updatesTotal++;
  return true;
  }

void SkeletalStorage::markAsChanged(size_t elt) {
  impl->markAsChanged(elt);
  }

Matrix4x4& SkeletalStorage::element(size_t i) {
  return *impl->get(i);
  }

void SkeletalStorage::reserve(size_t sz) {
  impl->reserve(sz*Resources::MAX_NUM_SKELETAL_NODES/blockSize);
  }

