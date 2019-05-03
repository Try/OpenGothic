#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/UniformBuffer>
#include <Tempest/Uniforms>
#include <Tempest/Matrix4x4>

#include <vector>
#include <cstdint>

#include "resources.h"

class Pose;

class AbstractObjectsBucket {
  public:
    AbstractObjectsBucket();
    AbstractObjectsBucket(AbstractObjectsBucket&&)=default;
    virtual ~AbstractObjectsBucket()=default;

    class Item final {
      public:
        Item()=default;
        Item(AbstractObjectsBucket& owner,size_t id)
          :owner(&owner),id(id){}
        Item(Item&& obj):owner(obj.owner),id(obj.id){
          obj.owner=nullptr;
          obj.id   =0;
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
        void   setSkeleton (const Skeleton*           sk);
        void   setSkeleton (const Pose&                p);

        const Tempest::Texture2d &texture() const;
        void  draw(Tempest::CommandBuffer &cmd, const Tempest::RenderPipeline &pipeline, uint32_t imgId) const;

      private:
        AbstractObjectsBucket* owner=nullptr;
        size_t                 id=0;
      };

    virtual size_t getNextId()=0;
    virtual void   free(const size_t objId)=0;

    virtual void   setObjMatrix(size_t i,const Tempest::Matrix4x4& m)=0;
    virtual void   setSkeleton(size_t i,const Skeleton* sk)=0;
    virtual void   setSkeleton(size_t i,const Pose& sk)=0;

    virtual const Tempest::Texture2d& texture() const = 0;
    virtual void draw(size_t id,Tempest::CommandBuffer &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId) = 0;

  protected:
    virtual void markAsChanged()=0;
  };
