#pragma once

#include <Tempest/AlignedArray>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <cassert>

#include "resources.h"
#include "abstractobjectsbucket.h"

template<class Ubo>
class ObjectsBucket : public AbstractObjectsBucket {
  public:
    ObjectsBucket(const Tempest::Texture2d* tex, const Tempest::UniformsLayout &layout, Tempest::Device &device)
      :AbstractObjectsBucket(device,layout),tex(tex),obj(device.caps().minUboAligment) {
      }

    Tempest::AlignedArray<Ubo>  obj;

    const Tempest::Texture2d&   texture() const { return *tex; }

    void                        set      (size_t i,const Ubo& u);
    void                        updateUbo(uint32_t imgId);
    void                        draw     (Tempest::CommandBuffer &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId);

  private:
    const Tempest::Texture2d*   tex=nullptr;

    void onResizeStorage(size_t sz) override {
      obj .resize(sz);
      data.resize(sz);
      }

    void setObjectMatrix(size_t sz,const Tempest::Matrix4x4& m) override {
      obj[sz].obj=m;
      }
  };

template<class Ubo>
void ObjectsBucket<Ubo>::set(size_t i,const Ubo& u){
  this->obj[i] = u;
  markAsChanged();
  }

template<class Ubo>
void ObjectsBucket<Ubo>::updateUbo(uint32_t imgId) {
  auto& frame=pf[imgId];
  size_t sz = obj.byteSize();
  assert(sz==frame.uboData.size());
  frame.uboData.update(obj.data(),0,sz);
  }

template<class Ubo>
void ObjectsBucket<Ubo>::draw(Tempest::CommandBuffer &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId) {
  auto& frame = pf[imgId];
  for(size_t i=0;i<data.size();++i){
    auto&    di     = data[i];
    uint32_t offset = i*obj.elementSize();

    cmd.setUniforms(pipeline,frame.ubo,1,&offset);
    cmd.draw(*di.vbo,*di.ibo);
    }
  }
