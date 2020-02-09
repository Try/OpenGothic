#pragma once

#include <zenload/zCMesh.h>
#include <zenload/zTypes.h>
#include <zenload/zCMaterial.h>
#include <Tempest/Matrix4x4>
#include <memory>
#include <limits>

class btConstraintSolver;
class btCollisionConfiguration;
class btBroadphaseInterface;
class btDispatcher;
class btDynamicsWorld;
class btTriangleIndexVertexArray;
class btCollisionShape;
class btRigidBody;
class btGhostObject;
class btCollisionObject;
class btTriangleMesh;
class btCollisionWorld;
class btVector3;

class PhysicMeshShape;
class PhysicMesh;
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

  public:
    static constexpr float gravity     = 100*9.8f;
    static constexpr float bulletSpeed = 3000; //per sec
    static constexpr float spellSpeed  = 1000; //per sec

    DynamicWorld(World &world, const PackedMesh &pkg);
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

        bool  testMove(const std::array<float,3>& pos);
        bool  testMove(const std::array<float,3>& pos, std::array<float,3> &fallback, float speed);
        bool  tryMoveN(const std::array<float,3>& pos, std::array<float,3> &norm);
        bool  tryMove (const std::array<float,3>& pos, std::array<float,3> &fallback, float speed);

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

    struct RayResult final {
      std::array<float,3> v={};
      std::array<float,3> n={};
      uint8_t             mat    = 0;
      Category            colCat = C_Null;
      bool                hasCol = 0;

      float               x() const { return v[0]; }
      float               y() const { return v[1]; }
      float               z() const { return v[2]; }
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
        std::array<float,3> position()  const { return pos; }
        std::array<float,3> direction() const { return dir; }
        Tempest::Matrix4x4  matrix()    const;
        bool                isSpell()   const { return spl!=std::numeric_limits<int>::max(); }
        int                 spellId()   const { return spl; }

      private:
        DynamicWorld*       owner = nullptr;
        BulletCallback*     cb    = nullptr;

        std::array<float,3> pos={};
        std::array<float,3> lastPos={};

        std::array<float,3> dir={};
        float               dirL=0.f;
        float               totalL=0.f;
        int                 spl=std::numeric_limits<int>::max();

      friend class DynamicWorld;
      };

    RayResult   dropRay (float x, float y, float z) const;
    RayResult   waterRay(float x, float y, float z) const;

    RayResult   ray          (float x0, float y0, float z0, float x1, float y1, float z1) const;
    float       soundOclusion(float x0, float y0, float z0, float x1, float y1, float z1) const;

    std::array<float,3> landNormal(float x, float y, float z) const;

    Item        ghostObj (const ZMath::float3& min,const ZMath::float3& max);
    StaticItem  staticObj(const PhysicMeshShape *src, const Tempest::Matrix4x4& m);
    BulletBody* bulletObj(BulletCallback* cb);

    void        tick(uint64_t dt);

    void        deleteObj(BulletBody* obj);

  private:
    void        deleteObj(NpcBody*    obj);
    void        deleteObj(btCollisionObject* obj);


    void       moveBullet(BulletBody& b, float dx, float dy, float dz, uint64_t dt);
    RayResult  implWaterRay (float x0, float y0, float z0, float x1, float y1, float z1) const;
    bool       hasCollision(const Item &it,std::array<float,3>& normal);

    template<class RayResultCallback>
    void       rayTest(const btVector3& rayFromWorld, const btVector3& rayToWorld, RayResultCallback& resultCallback) const;

    std::unique_ptr<btRigidBody> landObj();
    std::unique_ptr<btRigidBody> waterObj();

    void updateSingleAabb(btCollisionObject* obj);

    std::unique_ptr<btCollisionConfiguration>   conf;
    std::unique_ptr<btDispatcher>               dispatcher;
    std::unique_ptr<btBroadphaseInterface>      broadphase;
    std::unique_ptr<btCollisionWorld>           world;

    std::unique_ptr<PhysicMesh>                 landMesh;
    std::unique_ptr<btCollisionShape>           landShape;
    std::unique_ptr<btRigidBody>                landBody;

    std::unique_ptr<btCollisionShape>           waterShape;
    std::unique_ptr<btRigidBody>                waterBody;
    std::unique_ptr<PhysicMesh>                 waterMesh;

    std::unique_ptr<NpcBodyList>                npcList;
    std::unique_ptr<BulletsList>                bulletList;

    static const float                          ghostPadding;
    static const float                          ghostHeight;
    static const float                          worldHeight;
  };
