#pragma once

#include <Tempest/AlignedArray>
#include <Tempest/UniformBuffer>
#include <Tempest/Uniforms>

#include <cassert>

template<class Ubo>
class UboStorage {
  public:
    UboStorage(Tempest::Device &device);

    void                     commitUbo(Tempest::Device &device, uint32_t imgId);
    void                     updateUbo(uint32_t imgId);
    size_t                   alloc();
    void                     free(const size_t objId);

    size_t                   elementSize() const { return obj.elementSize(); }
    const
    Tempest::UniformBuffer&  operator[](size_t i) const { return pf[i].uboData; }

    void                     markAsChanged();
    Ubo&                     element(size_t i){ return obj[i]; }

    void                     reserve(size_t sz);

  private:
    struct PerFrame final {
      Tempest::UniformBuffer uboData;
      bool                   uboChanged=false; // invalidate ubo array
      };

    std::unique_ptr<PerFrame[]> pf;
    size_t                      pfSize=0;

    Tempest::AlignedArray<Ubo>  obj;
    std::vector<size_t>         freeList;
  };

template<class Ubo>
UboStorage<Ubo>::UboStorage(Tempest::Device &device)
  :pfSize(device.maxFramesInFlight()),obj(device.caps().minUboAligment) {
  pf.reset(new PerFrame[pfSize]);
  }

template<class Ubo>
size_t UboStorage<Ubo>::alloc() {
  markAsChanged();
  if(freeList.size()>0){
    size_t id=freeList.back();
    freeList.pop_back();
    return id;
    }
  obj.resize(obj.size()+1);
  return obj.size()-1;
  }

template<class Ubo>
void UboStorage<Ubo>::free(const size_t objId) {
  freeList.push_back(objId);
  obj[objId]=Ubo();
  }

template<class Ubo>
void UboStorage<Ubo>::markAsChanged() {
  for(uint32_t i=0;i<pfSize;++i)
    pf[i].uboChanged=true;
  }

template<class Ubo>
void UboStorage<Ubo>::commitUbo(Tempest::Device& device,uint32_t imgId) {
  auto& frame=pf[imgId];
  size_t sz = obj.byteSize();
  if(frame.uboData.size()!=sz)
    frame.uboData = device.loadUbo(obj.data(),sz); else
    frame.uboData.update(obj.data(),0,sz);
  }

template<class Ubo>
void UboStorage<Ubo>::updateUbo(uint32_t imgId) {
  auto& frame=pf[imgId];
  size_t sz = obj.byteSize();
  assert(sz==frame.uboData.size());
  if(frame.uboChanged) {
    frame.uboData.update(obj.data(),0,sz);
    frame.uboChanged = false;
    }
  }

template<class Ubo>
void UboStorage<Ubo>::reserve(size_t sz){
  obj.reserve(sz);
  }
