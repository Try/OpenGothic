#pragma once

#include <Tempest/Device>
#include <Tempest/Uniforms>
#include <Tempest/UniformBuffer>

template<class T>
class UboChain final {
  public:
    UboChain(Tempest::Device& device) {
      const uint32_t maxFrames=device.maxFramesInFlight();
      pf.reset(new PerFrame[maxFrames]);

      for(size_t i=0;i<maxFrames;++i) {
        pf[i].uboData = device.loadUbo(nullptr,sizeof(T));
        }
      }

    void update(const T& val,uint32_t frameId) {
      pf[frameId].uboData.update(&val,0,sizeof(T));
      }

    const Tempest::UniformBuffer& operator[](size_t i) const {
      return pf[i].uboData;
      }

  private:
    struct PerFrame final {
      Tempest::UniformBuffer uboData;
      bool                   uboChanged=false; // invalidate ubo array
      };
    std::unique_ptr<PerFrame[]> pf;
  };
