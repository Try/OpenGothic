#pragma once

#include <Tempest/AlignedArray>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>
#include <Tempest/Log>

#include <cassert>

#include "abstractobjectsbucket.h"
#include "ubostorage.h"
#include "resources.h"

template<class Ubo,class Vertex>
class ObjectsBucket : public AbstractObjectsBucket {
  public:
    ObjectsBucket(const Tempest::Texture2d* tex, const Tempest::UniformsLayout &layout,
                  Tempest::Device &device, UboStorage<Ubo>& uStorage)
      : tex(tex),uStorage(uStorage) {
      size_t pfSize=device.maxFramesInFlight();
      pf.reset(new PerFrame[pfSize]);
      for(size_t i=0;i<pfSize;++i)
        pf[i].ubo = device.uniforms(layout);
      }

    const Tempest::Texture2d&   texture() const { return *tex; }
    Tempest::Uniforms&          getUbo(size_t imgId) { return pf[imgId].ubo; }

    size_t                      alloc(const Tempest::VertexBuffer<Vertex> &vbo, const Tempest::IndexBuffer<uint32_t> &ibo);
    void                        free(size_t i) override;

    void                        draw(Tempest::CommandBuffer &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId);

  private:
    struct NonUbo final {
      const Tempest::VertexBuffer<Vertex>*  vbo=nullptr;
      const Tempest::IndexBuffer<uint32_t>* ibo=nullptr;
      size_t                                ubo=size_t(-1);
      };

    struct PerFrame final {
      Tempest::Uniforms        ubo;
      };

    const Tempest::Texture2d*   tex=nullptr;
    UboStorage<Ubo>&            uStorage;
    std::unique_ptr<PerFrame[]> pf;

    std::vector<NonUbo>         data;
    std::vector<NonUbo*>        index;
    std::vector<size_t>         freeList;

    Ubo&                        element(size_t i);
    void                        markAsChanged() override;
    size_t                      getNextId() override;

    void                        setObjMatrix(size_t i,const Tempest::Matrix4x4& m) override;
    void                        setSkeleton(size_t i,const Skeleton* sk) override;
    void                        setSkeleton(size_t i,const Pose& sk) override;
  };

template<class Ubo,class Vertex>
size_t ObjectsBucket<Ubo,Vertex>::getNextId(){
  if(freeList.size()>0){
    auto id = freeList.back();
    freeList.pop_back();
    return id;
    }
  data.emplace_back();
  return data.size()-1;
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::setObjMatrix(size_t i, const Tempest::Matrix4x4 &m) {
  element(i).setObjMatrix(m);
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::setSkeleton(size_t i, const Skeleton *sk) {
  element(i).setSkeleton(sk);
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::setSkeleton(size_t i, const Pose& p) {
  element(i).setSkeleton(p);
  }

template<class Ubo,class Vertex>
size_t ObjectsBucket<Ubo,Vertex>::alloc(const Tempest::VertexBuffer<Vertex>  &vbo,
                                        const Tempest::IndexBuffer<uint32_t> &ibo) {
  const size_t id=getNextId();
  data[id].vbo = &vbo;
  data[id].ibo = &ibo;
  data[id].ubo = uStorage.alloc();
  if(data[id].ubo==167)
    Tempest::Log::i("");
  return data.size()-1;
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::free(size_t i) {
  auto id = data[i].ubo;
  if(id==size_t(-1))
    assert(0 && "double free!");
  data[i] = NonUbo();
  uStorage.free(id);
  freeList.push_back(i);
  }

template<class Ubo, class Vertex>
Ubo &ObjectsBucket<Ubo,Vertex>::element(size_t i) {
  return uStorage.element(data[i].ubo);
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::markAsChanged() {
  uStorage.markAsChanged();
  }

template<class Ubo,class Vertex>
void ObjectsBucket<Ubo,Vertex>::draw(Tempest::CommandBuffer &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId) {
  auto& frame = pf[imgId];
  index.resize(data.size());
  for(size_t i=0;i<index.size();++i)
    index[i]=&data[i];

  std::sort(index.begin(),index.end(),[](const NonUbo* a,const NonUbo* b){
    return a->ibo<b->ibo;
    });

  for(size_t i=0;i<index.size();++i){
    auto&    di     = *index[i];
    uint32_t offset = di.ubo*uStorage.elementSize();

    cmd.setUniforms(pipeline,frame.ubo,1,&offset);
    cmd.draw(*di.vbo,*di.ibo);
    }
  }
