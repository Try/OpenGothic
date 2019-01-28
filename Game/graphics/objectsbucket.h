#pragma once

#include <Tempest/AlignedArray>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <cassert>

#include "resources.h"
#include "abstractobjectsbucket.h"

template<class Ubo,class Vertex>
class ObjectsBucket : public AbstractObjectsBucket {
  public:
    ObjectsBucket(const Tempest::Texture2d* tex, const Tempest::UniformsLayout &layout, Tempest::Device &device)
      :AbstractObjectsBucket(device,layout),tex(tex),obj(device.caps().minUboAligment) {
      }

    Tempest::AlignedArray<Ubo>  obj;

    const Tempest::Texture2d&   texture() const { return *tex; }

    void                        updateUbo(uint32_t imgId);
    void                        draw     (Tempest::CommandBuffer &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId);
    size_t                      alloc    (const Tempest::VertexBuffer<Vertex> &vbo, const Tempest::IndexBuffer<uint32_t> &ibo);

  private:
    struct NonUbo final {
      const Tempest::VertexBuffer<Vertex>*  vbo=nullptr;
      const Tempest::IndexBuffer<uint32_t>* ibo=nullptr;
      };

    const Tempest::Texture2d*   tex=nullptr;
    std::vector<NonUbo>         data;

    size_t storageSize() const {
      return data.size();
      }

    void onResizeStorage(size_t sz) override {
      obj .resize(sz);
      data.resize(sz);
      }

    void setObjectMatrix(size_t id,const Tempest::Matrix4x4& m) override {
      obj[id].setObj(m);
      }

    void setSkeleton    (size_t id,const Skeleton* s) override {
      obj[id].setSkeleton(s);
      }
  };

template<class Ubo,class Vertex>
size_t ObjectsBucket<Ubo,Vertex>::alloc(const Tempest::VertexBuffer<Vertex>  &vbo,
                                        const Tempest::IndexBuffer<uint32_t> &ibo){
  const size_t id=getNextId();
  markAsChanged();

  data[id].vbo = &vbo;
  data[id].ibo = &ibo;
  return data.size()-1;
  }

template<class Ubo,class Vertex>
void ObjectsBucket<Ubo,Vertex>::updateUbo(uint32_t imgId) {
  auto& frame=pf[imgId];
  size_t sz = obj.byteSize();
  assert(sz==frame.uboData.size());
  frame.uboData.update(obj.data(),0,sz);
  }

template<class Ubo,class Vertex>
void ObjectsBucket<Ubo,Vertex>::draw(Tempest::CommandBuffer &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId) {
  auto& frame = pf[imgId];
  for(size_t i=0;i<data.size();++i){
    auto&    di     = data[i];
    uint32_t offset = i*obj.elementSize();

    cmd.setUniforms(pipeline,frame.ubo,1,&offset);
    cmd.draw(*di.vbo,*di.ibo);
    }
  }
