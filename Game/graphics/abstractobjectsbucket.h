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
class Bounds;
class Painter3d;

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

        bool   isEmpty() const { return owner==nullptr; }

        void   setObjMatrix(const Tempest::Matrix4x4& mt);
        void   setSkeleton (const Skeleton*           sk);
        void   setPose     (const Pose&                p);
        void   setBounds   (const Bounds&           bbox);

        void   draw(Painter3d& p, uint8_t fId) const;

      private:
        AbstractObjectsBucket* owner=nullptr;
        size_t                 id=0;
      };

    virtual void   free(const size_t objId)=0;

    virtual void   setObjMatrix(size_t i,const Tempest::Matrix4x4& m)=0;
    virtual void   setSkeleton (size_t i,const Skeleton* sk)=0;
    virtual void   setSkeleton (size_t i,const Pose& sk)=0;
    virtual void   setBounds   (size_t i,const Bounds& b)=0;

    virtual void   draw(size_t id, Painter3d& p, uint8_t fId) = 0;
  };
