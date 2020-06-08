#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>
#include <Tempest/Log>

#include <cassert>

#include "abstractobjectsbucket.h"
#include "ubostorage.h"
#include "graphics/dynamic/painter3d.h"
#include "bounds.h"
#include "resources.h"

template<class Ubo,class Vertex>
class ObjectsBucket : public AbstractObjectsBucket {
  public:
    ObjectsBucket(const Tempest::Texture2d* tex, const Tempest::UniformsLayout &layout,
                  Tempest::Device &device, UboStorage<Ubo>& uStorage)
      : tex(tex),uStorage(uStorage) {
      for(auto& i:pf) {
        i.ubo      = device.uniforms(layout);
        i.uboSh[0] = device.uniforms(layout);
        i.uboSh[1] = device.uniforms(layout);
        }
      }

    ObjectsBucket(ObjectsBucket&&)=default;

    ~ObjectsBucket(){
      assert(data.size()==freeList.size());
      }

    const Tempest::Texture2d&   texture() const override { return *tex; }
    Tempest::Uniforms&          uboMain  (size_t imgId) { return pf[imgId].ubo;   }
    Tempest::Uniforms&          uboShadow(size_t imgId,int layer) { return pf[imgId].uboSh[layer]; }

    size_t                      alloc(const Tempest::VertexBuffer<Vertex> &vbo,
                                      const Tempest::IndexBuffer<uint32_t> &ibo,
                                      const Bounds& bounds);
    void                        free(size_t i) override;

    void                        draw      (Painter3d& painter,const Tempest::RenderPipeline &pipeline, uint32_t imgId);
    void                        drawShadow(Painter3d& painter,const Tempest::RenderPipeline &pipeline, uint32_t imgId, int layer);

    void                        draw(size_t id,Tempest::Encoder<Tempest::CommandBuffer> &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId) override;

    void                        invalidateCmd();
    bool                        needToUpdateCommands(uint8_t fId) const;
    void                        setAsUpdated(uint8_t fId);

  private:
    struct NonUbo final {
      const Tempest::VertexBuffer<Vertex>*  vbo=nullptr;
      const Tempest::IndexBuffer<uint32_t>* ibo=nullptr;
      size_t                                ubo=size_t(-1);
      Bounds                                bbox;
      };

    struct PerFrame final {
      Tempest::Uniforms ubo, uboSh[2];
      bool              nToUpdate=true; //invalidate cmd buffers
      };

    const Tempest::Texture2d*   tex=nullptr;
    UboStorage<Ubo>&            uStorage;

    PerFrame                    pf[Resources::MaxFramesInFlight];

    std::vector<NonUbo>         data;
    std::vector<NonUbo*>        index;
    std::vector<size_t>         freeList;
    bool                        indexed=false;

    Ubo&                        element(size_t i);
    void                        markAsChanged() override;
    size_t                      getNextId() override final;
    static bool                 idxCmp(const NonUbo* a,const NonUbo* b);
    void                        mkIndex();

    void                        setObjMatrix(size_t i,const Tempest::Matrix4x4& m) override;
    void                        setSkeleton(size_t i,const Skeleton* sk) override;
    void                        setSkeleton(size_t i,const Pose& sk) override;
    void                        setBounds  (size_t i,const Bounds& b) override;
  };

template<class Ubo,class Vertex>
size_t ObjectsBucket<Ubo,Vertex>::getNextId(){
  if(freeList.size()>0){
    auto id = freeList.back();
    freeList.pop_back();
    return id;
    }
  indexed = false;
  data.emplace_back();
  return data.size()-1;
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::setObjMatrix(size_t i, const Tempest::Matrix4x4 &m) {
  element(i).setObjMatrix(m);
  data[i].bbox.setObjMatrix(m);
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::setSkeleton(size_t i, const Skeleton *sk) {
  element(i).setSkeleton(sk);
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::setSkeleton(size_t i, const Pose& p) {
  element(i).setSkeleton(p);
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::setBounds(size_t i, const Bounds& b) {
  data[i].bbox = b;
  }

template<class Ubo,class Vertex>
size_t ObjectsBucket<Ubo,Vertex>::alloc(const Tempest::VertexBuffer<Vertex>  &vbo,
                                        const Tempest::IndexBuffer<uint32_t> &ibo,
                                        const Bounds& bounds) {
  invalidateCmd();
  const size_t id=getNextId();
  data[id].vbo  = &vbo;
  data[id].ibo  = &ibo;
  data[id].ubo  = uStorage.alloc();
  data[id].bbox = bounds;
  return id;
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::free(size_t i) {
  invalidateCmd();
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
void ObjectsBucket<Ubo,Vertex>::draw(Painter3d& painter,const Tempest::RenderPipeline &pipeline, uint32_t imgId) {
  mkIndex();

  auto& frame = pf[imgId];
  for(size_t i=0;i<index.size();++i){
    auto& di = *index[i];
    if(di.vbo==nullptr)
      continue;
    uint32_t offset = uint32_t(di.ubo);
    painter.draw(pipeline,di.bbox,frame.ubo,offset,*di.vbo,*di.ibo);
    }
  }

template<class Ubo,class Vertex>
void ObjectsBucket<Ubo,Vertex>::drawShadow(Painter3d& painter,const Tempest::RenderPipeline &pipeline, uint32_t imgId, int layer) {
  mkIndex();

  auto& frame = pf[imgId];
  for(size_t i=0;i<index.size();++i){
    auto& di = *index[i];
    if(di.vbo==nullptr)
      continue;
    uint32_t offset = uint32_t(di.ubo);
    painter.draw(pipeline,di.bbox,frame.uboSh[layer],offset,*di.vbo,*di.ibo);
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
bool ObjectsBucket<Ubo,Vertex>::needToUpdateCommands(uint8_t fId) const {
  return pf[fId].nToUpdate;
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::setAsUpdated(uint8_t fId) {
  pf[fId].nToUpdate=false;
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::invalidateCmd() {
  for(auto& i:pf)
    i.nToUpdate=true;
  }

template<class Ubo, class Vertex>
void ObjectsBucket<Ubo,Vertex>::mkIndex() {
  if(indexed)
    return;
  indexed = true;

  index.resize(data.size());
  for(size_t i=0;i<index.size();++i)
    index[i]=&data[i];

  std::sort(index.begin(),index.end(),idxCmp);
  }

template<class Ubo, class Vertex>
bool ObjectsBucket<Ubo,Vertex>::idxCmp(const NonUbo* a,const NonUbo* b) {
  return a->ibo<b->ibo;
  }
