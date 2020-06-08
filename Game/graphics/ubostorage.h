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

    bool                     commitUbo(Tempest::Device &device, uint8_t imgId);
    size_t                   alloc();
    void                     free(const size_t objId);
    bool                     needToRealloc(uint32_t imgId) const;
    const
    Tempest::UniformBuffer<Ubo>& operator[](size_t i) const { return pf[i].uboData; }

    void                     markAsChanged();
    Ubo&                     element(size_t i){ return obj[i]; }

    void                     reserve(size_t sz);

  private:
    struct PerFrame final {
      Tempest::UniformBuffer<Ubo> uboData;
      std::atomic_bool            uboChanged{false};  // invalidate ubo array
      };

    PerFrame                    pf[Resources::MaxFramesInFlight];
    std::vector<Ubo>            obj;
    std::vector<size_t>         freeList;
  };

template<class Ubo>
UboStorage<Ubo>::UboStorage() {
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
bool UboStorage<Ubo>::needToRealloc(uint32_t imgId) const {
  auto& frame=pf[imgId];
  return obj.size()!=frame.uboData.size();
  }

template<class Ubo>
void UboStorage<Ubo>::markAsChanged() {
  for(auto& i:pf)
    i.uboChanged=true;
  }

template<class Ubo>
bool UboStorage<Ubo>::commitUbo(Tempest::Device& device,uint8_t imgId) {
  auto&        frame   = pf[imgId];
  const bool   realloc = frame.uboData.size()!=obj.size();
  if(!frame.uboChanged)
    return false;
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
