#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/UniformBuffer>
#include <Tempest/Uniforms>
#include <Tempest/Matrix4x4>

#include <vector>
#include <cstdint>

#include "resources.h"

class AbstractObjectsBucket {
  public:
    AbstractObjectsBucket(Tempest::Device &device,const Tempest::UniformsLayout &layout);
    AbstractObjectsBucket(AbstractObjectsBucket&&)=default;
    virtual ~AbstractObjectsBucket()=default;

    class Item final {
      public:
        Item()=default;
        Item(AbstractObjectsBucket& owner,size_t id)
          :owner(&owner),id(id){}
        Item(Item&& obj):owner(obj.owner),id(obj.id){
          obj.owner=nullptr;
          }
        Item& operator=(Item&& obj) {
          std::swap(obj.owner,owner);
          std::swap(obj.id,   id);
          return *this;
          }
        ~Item() {
          if(owner)
            owner->free(this->id);
          }

        void   setObjMatrix(const Tempest::Matrix4x4& mt);

      private:
        AbstractObjectsBucket*                owner=nullptr;
        size_t                                id=0;
      };

    struct PerFrame final {
      Tempest::UniformBuffer uboData;
      Tempest::Uniforms      ubo;
      bool                   uboChanged=false; // invalidate ubo array
      };

    std::unique_ptr<PerFrame[]> pf;

    size_t                      alloc(const Tempest::VertexBuffer<Resources::Vertex>& vbo,const Tempest::IndexBuffer<uint32_t>& ibo);
    void                        free(const size_t objId);

  protected:
    struct NonUbo final {
      const Tempest::VertexBuffer<Resources::Vertex>* vbo=nullptr;
      const Tempest::IndexBuffer<uint32_t>*           ibo=nullptr;
      };

    std::vector<NonUbo>         data;

    size_t                      getNextId();
    void                        markAsChanged();

    virtual void                onResizeStorage(size_t sz)=0;
    virtual void                setObjectMatrix(size_t id,const Tempest::Matrix4x4& m)=0;

  private:
    std::vector<size_t>         freeList;
    uint32_t                    pfSize=0;
  };
