#pragma once

#include <Tempest/UniformBuffer>
#include <Tempest/Uniforms>
#include <Tempest/Device>

#include <cassert>

#include "resources.h"

template<class Ubo>
class UboStorage {
  public:
    UboStorage();

    bool                     commitUbo(Tempest::Device &device, uint8_t fId);
    size_t                   alloc();
    void                     free(const size_t objId);
    const
    Tempest::UniformBuffer<Ubo>& operator[](size_t i) const { return pf[i].uboData; }

    void                     markAsChanged(size_t elt);
    Ubo&                     element(size_t i){ return obj[i]; }

    void                     reserve(size_t sz);

  private:
    struct PerFrame final {
      Tempest::UniformBuffer<Ubo>  uboData;
      std::atomic_bool             uboChanged{false};  // invalidate ubo array
      };

    PerFrame                    pf[Resources::MaxFramesInFlight];
    std::vector<Ubo>            obj;
    std::vector<size_t>         freeList;
    size_t                      updatesTotal=0; // perf statistic
  };

template<class Ubo>
UboStorage<Ubo>::UboStorage() {
  }

template<class Ubo>
size_t UboStorage<Ubo>::alloc() {
  if(freeList.size()>0){
    size_t id=freeList.back();
    freeList.pop_back();
    return id;
    }
  obj.resize(obj.size()+1);
  markAsChanged(obj.size()-1);
  return obj.size()-1;
  }

template<class Ubo>
void UboStorage<Ubo>::free(const size_t objId) {
  freeList.push_back(objId);
  obj[objId]=Ubo();
  }

template<class Ubo>
void UboStorage<Ubo>::markAsChanged(size_t /*elt*/) {
  for(auto& i:pf)
    i.uboChanged=true;
  }

template<class Ubo>
bool UboStorage<Ubo>::commitUbo(Tempest::Device& device,uint8_t fId) {
  auto&        frame   = pf[fId];
  const bool   realloc = frame.uboData.size()!=obj.size();
  if(!frame.uboChanged)
    return false;
  updatesTotal++;
  if(realloc)
    frame.uboData = device.ubo<Ubo>(obj.data(),obj.size()); else
    frame.uboData.update(obj.data(),0,obj.size());
  frame.uboChanged = false;
  return realloc;
  }

template<class Ubo>
void UboStorage<Ubo>::reserve(size_t sz){
  obj.reserve(sz);
  }
