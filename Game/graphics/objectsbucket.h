#pragma once

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
      for(size_t i=0;i<pfSize;++i) {
        pf[i].ubo      = device.uniforms(layout);
        pf[i].uboSh[0] = device.uniforms(layout);
        pf[i].uboSh[1] = device.uniforms(layout);
        }
      }

    ObjectsBucket(ObjectsBucket&&)=default;

    ~ObjectsBucket(){
      assert(data.size()==freeList.size());
      }

    const Tempest::Texture2d&   texture() const override { return *tex; }
    Tempest::Uniforms&          uboMain  (size_t imgId) { return pf[imgId].ubo;   }
    Tempest::Uniforms&          uboShadow(size_t imgId,int layer) { return pf[imgId].uboSh[layer]; }

    size_t                      alloc(const Tempest::VertexBuffer<Vertex> &vbo, const Tempest::IndexBuffer<uint32_t> &ibo);
    void                        free(size_t i) override;

    void                        draw      (Tempest::Encoder<Tempest::CommandBuffer> &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId);
    void                        drawShadow(Tempest::Encoder<Tempest::CommandBuffer> &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId, int layer);

    void                        draw(size_t id,Tempest::Encoder<Tempest::CommandBuffer> &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId) override;

    bool                        needToUpdateCommands() const;
    void                        setAsUpdated();

  private:
    struct NonUbo final {
      const Tempest::VertexBuffer<Vertex>*  vbo=nullptr;
      const Tempest::IndexBuffer<uint32_t>* ibo=nullptr;
      size_t                                ubo=size_t(-1);
      };

    struct PerFrame final {
      Tempest::Uniforms ubo, uboSh[2];
      };

    const Tempest::Texture2d*   tex=nullptr;
    UboStorage<Ubo>&            uStorage;
    std::unique_ptr<PerFrame[]> pf;

    std::vector<NonUbo>         data;
    std::vector<NonUbo*>        index;
    std::vector<size_t>         freeList;
    bool                        nToUpdate=true; //invalidate cmd buffers

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
  nToUpdate=true;
  const size_t id=getNextId();
  data[id].vbo = &vbo;
  data[id].ibo = &ibo;
  data[id].ubo = uStorage.alloc();
  return id;
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::free(size_t i) {
  nToUpdate=true;
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
void ObjectsBucket<Ubo,Vertex>::draw(Tempest::Encoder<Tempest::CommandBuffer> &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId) {
  auto& frame = pf[imgId];
  index.resize(data.size());
  for(size_t i=0;i<index.size();++i)
    index[i]=&data[i];

  std::sort(index.begin(),index.end(),[](const NonUbo* a,const NonUbo* b){
    return a->ibo<b->ibo;
    });

  for(size_t i=0;i<index.size();++i){
    auto& di = *index[i];
    if(di.vbo==nullptr)
      continue;
    uint32_t offset = uint32_t(di.ubo);

    cmd.setUniforms(pipeline,frame.ubo,1,&offset);
    cmd.draw(*di.vbo,*di.ibo);
    }
  }

template<class Ubo,class Vertex>
void ObjectsBucket<Ubo,Vertex>::drawShadow(Tempest::Encoder<Tempest::CommandBuffer> &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId, int layer) {
  auto& frame = pf[imgId];
  index.resize(data.size());
  for(size_t i=0;i<index.size();++i)
    index[i]=&data[i];

  std::sort(index.begin(),index.end(),[](const NonUbo* a,const NonUbo* b){
    return a->ibo<b->ibo;
    });

  for(size_t i=0;i<index.size();++i){
    auto& di = *index[i];
    if(di.vbo==nullptr)
      continue;
    uint32_t offset = uint32_t(di.ubo);

    cmd.setUniforms(pipeline,frame.uboSh[layer],1,&offset);
    cmd.draw(*di.vbo,*di.ibo);
    }
  }

template<class Ubo,class Vertex>
void ObjectsBucket<Ubo,Vertex>::draw(size_t id, Tempest::Encoder<Tempest::CommandBuffer> &cmd, const Tempest::RenderPipeline &pipeline, uint32_t imgId) {
  auto& frame = pf[imgId];
  auto& di    = data[id];
  if(di.vbo==nullptr)
    return;
  uint32_t offset = uint32_t(di.ubo);

  cmd.setUniforms(pipeline,frame.ubo,1,&offset);
  cmd.draw(*di.vbo,*di.ibo);
  }

template<class Ubo, class Vertex>
bool ObjectsBucket<Ubo,Vertex>::needToUpdateCommands() const {
  return nToUpdate;
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::setAsUpdated() {
  nToUpdate=false;
  }
