#pragma once

#include <zenload/zCMesh.h>
#include <zenload/zTypes.h>
#include <zenload/zCMaterial.h>
#include <Tempest/Matrix4x4>
#include <memory>
#include <limits>

#include "graphics/mesh/protomesh.h"

class btConstraintSolver;
class btCollisionConfiguration;
class btBroadphaseInterface;
class btDispatcher;
class btDynamicsWorld;
class btTriangleIndexVertexArray;
class btCollisionShape;
class btConcaveShape;
class btRigidBody;
class btGhostObject;
class btCollisionObject;
class btTriangleMesh;
class btCollisionWorld;
class btVector3;

class PhysicMeshShape;
class PhysicVbo;
class PackedMesh;
class World;
class Bullet;
class Npc;

class DynamicWorld final {
  private:
    struct HumShape;
    struct NpcBody;
    struct NpcBodyList;
    struct BulletsList;
    struct BBoxList;
    struct Broadphase;

  public:
    static constexpr float gravity     = 100*9.8f;
    static constexpr float bulletSpeed = 3000; //per sec
    static constexpr float spellSpeed  = 1000; //per sec

    DynamicWorld(World &world, const ZenLoad::zCMesh& mesh);
    DynamicWorld(const DynamicWorld&)=delete;
    ~DynamicWorld();

    enum Category {
      C_Null      = 1,
      C_Landscape = 2,
      C_Water     = 3,
      C_Object    = 4,
      C_Ghost     = 5
      };

    struct Item {
      public:
        Item()=default;
        Item(DynamicWorld* owner,NpcBody* obj,float h,float r):owner(owner),obj(obj),height(h),r(r){}
        Item(Item&& it):owner(it.owner),obj(it.obj),height(it.height),r(it.r){it.obj=nullptr;}
        ~Item() { if(owner) owner->deleteObj(obj); }

        Item& operator = (Item&& it){
          std::swap(owner,it.owner);
          std::swap(obj,it.obj);
          std::swap(height,it.height);
          std::swap(r,it.r);
          return *this;
          }

        void  setPosition(float x,float y,float z);
        void  setEnable(bool e);
        void  setUserPointer(void* p);

        float centerY() const;

        bool  testMove(const Tempest::Vec3& pos);
        bool  testMove(const Tempest::Vec3& pos, Tempest::Vec3& fallback, float speed);
        bool  tryMoveN(const Tempest::Vec3& pos, Tempest::Vec3& norm);

        bool  hasCollision() const;
        float radius() const { return r; }

      private:
        DynamicWorld*       owner  = nullptr;
        NpcBody*            obj    = nullptr;
        float               height = 0.f;
        float               r      = 0.f;
        void implSetPosition(float x,float y,float z);

      friend class DynamicWorld;
      };

    struct StaticItem {
      public:
        StaticItem()=default;
        StaticItem(DynamicWorld* owner,btCollisionObject* obj):owner(owner),obj(obj){}
        StaticItem(StaticItem&& it):owner(it.owner),obj(it.obj){it.obj=nullptr;}
        ~StaticItem() { if(owner) owner->deleteObj(obj); }

        StaticItem& operator = (StaticItem&& it){
          std::swap(owner,it.owner);
          std::swap(obj,it.obj);
          return *this;
          }

        void setObjMatrix(const Tempest::Matrix4x4& m);

      private:
        DynamicWorld*       owner  = nullptr;
        btCollisionObject*  obj    = nullptr;
      };

    struct RayLandResult {
      Tempest::Vec3       v={};
      Tempest::Vec3       n={};
      uint8_t             mat     = 0;
      bool                hasCol  = 0;

      const char*         sector  = nullptr;
      };

    struct RayWaterResult {
      float               wdepth  = 0.f;
      bool                hasCol = false;
      };

    struct BulletCallback {
      virtual ~BulletCallback()=default;
      virtual void onStop(){}
      virtual void onMove(){}
      virtual void onCollide(uint8_t matId){(void)matId;}
      virtual void onCollide(Npc& other){(void)other;}
      };

    struct BulletBody final {
      public:
        BulletBody(DynamicWorld* wrld,BulletCallback* cb);
        BulletBody(BulletBody&&);

        void  setSpellId(int spl);

        void  move(float x,float y,float z);
        void  setPosition  (float x,float y,float z);
        void  setDirection (float x,float y,float z);
        float pathLength() const;
        void  addPathLen(float v);

        float               speed()     const { return dirL; }
        Tempest::Vec3       position()  const { return pos; }
        Tempest::Vec3       direction() const { return dir; }
        Tempest::Matrix4x4  matrix()    const;
        bool                isSpell()   const { return spl!=std::numeric_limits<int>::max(); }
        int                 spellId()   const { return spl; }

      private:
        DynamicWorld*       owner = nullptr;
        BulletCallback*     cb    = nullptr;

        Tempest::Vec3       pos={};
        Tempest::Vec3       lastPos={};

        Tempest::Vec3       dir={};
        float               dirL=0.f;
        float               totalL=0.f;
        int                 spl=std::numeric_limits<int>::max();

      friend class DynamicWorld;
      };

    struct BBoxCallback {
      virtual ~BBoxCallback()=default;
      virtual void onCollide(BulletBody& other){(void)other;}
      };

    struct BBoxBody final {
      public:
        BBoxBody(DynamicWorld* wrld, BBoxCallback* cb, const ZMath::float3* bbox);
        BBoxBody(const BBoxBody& other) = delete;
        ~BBoxBody();

      private:
        DynamicWorld*       owner = nullptr;
        BBoxCallback*       cb    = nullptr;

        btCollisionShape*   shape = nullptr;
        btRigidBody*        obj   = nullptr;

      friend class DynamicWorld;
      };

    RayLandResult  landRay    (float x, float y, float z, float maxDy=0) const;
    RayWaterResult waterRay   (float x, float y, float z) const;

    RayLandResult  ray        (float x0, float y0, float z0, float x1, float y1, float z1) const;
    float          soundOclusion(float x0, float y0, float z0, float x1, float y1, float z1) const;

    Item        ghostObj (const ZMath::float3& min,const ZMath::float3& max);
    StaticItem  staticObj(const PhysicMeshShape *src, const Tempest::Matrix4x4& m);
    BulletBody* bulletObj(BulletCallback* cb);
    BBoxBody*   bboxObj(BBoxCallback* cb, const ZMath::float3* bbox);

    void        tick(uint64_t dt);

    void        deleteObj(BulletBody* obj);
    void        deleteObj(BBoxBody*   obj);

    const char* validateSectorName(const char* name) const;

  private:
    void        deleteObj(NpcBody*    obj);
    void        deleteObj(btCollisionObject* obj);


    void       moveBullet(BulletBody& b, float dx, float dy, float dz, uint64_t dt);
    RayWaterResult implWaterRay (float x0, float y0, float z0, float x1, float y1, float z1) const;
    bool       hasCollision(const Item &it, Tempest::Vec3& normal);

    template<class RayResultCallback>
    void       rayTest(const btVector3& rayFromWorld, const btVector3& rayToWorld, RayResultCallback& resultCallback) const;

    std::unique_ptr<btRigidBody> landObj();
    std::unique_ptr<btRigidBody> waterObj();

    void       updateSingleAabb(btCollisionObject* obj);
    void       updateAabbs() const;

    std::unique_ptr<btCollisionConfiguration>   conf;
    std::unique_ptr<btDispatcher>               dispatcher;
    std::unique_ptr<btBroadphaseInterface>      broadphase;
    std::unique_ptr<btCollisionWorld>           world;

    std::vector<std::string>                    sectors;

    std::vector<btVector3>                      landVbo;
    std::unique_ptr<PhysicVbo>                  landMesh;
    std::unique_ptr<btConcaveShape>             landShape;
    std::unique_ptr<btRigidBody>                landBody;

    std::unique_ptr<btCollisionShape>           waterShape;
    std::unique_ptr<btRigidBody>                waterBody;
    std::unique_ptr<PhysicVbo>                  waterMesh;

    std::unique_ptr<NpcBodyList>                npcList;
    std::unique_ptr<BulletsList>                bulletList;
    std::unique_ptr<BBoxList>                   bboxList;

    mutable uint32_t                            aabbChanged = 0;

    static const float                          ghostPadding;
    static const float                          ghostHeight;
    static const float                          worldHeight;
  };
