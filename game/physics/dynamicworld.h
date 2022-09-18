#pragma once

#include <zenload/zCMesh.h>
#include <zenload/zTypes.h>
#include <zenload/zCMaterial.h>
#include <Tempest/Matrix4x4>
#include <memory>
#include <limits>

class btTriangleIndexVertexArray;
class btCollisionShape;
class btCollisionObject;
class btRigidBody;
class btVector3;

class PhysicMeshShape;
class PhysicVbo;
class PackedMesh;
class Bounds;

class World;
class Bullet;
class Npc;
class Item;
class Interactive;

class CollisionWorld;

class DynamicWorld final {
  private:
    struct HumShape;
    struct NpcBody;
    struct NpcBodyList;
    struct BulletsList;
    struct BBoxList;

  public:
    static constexpr float gravityMS   = 9.8f; // meters per second^2
    static constexpr float gravity     = gravityMS*100.f/(1000.f*1000.f); // centimeters per milliseconds^2
    static constexpr float bulletSpeed = 3; // centimeters per milliseconds
    static constexpr float spellSpeed  = 1; // centimeters per milliseconds
    static const     float ghostPadding;

    DynamicWorld(World &world, const ZenLoad::zCMesh& mesh);
    DynamicWorld(const DynamicWorld&)=delete;
    ~DynamicWorld();

    enum Category {
      C_Null      = 1,
      C_Landscape = 2,
      C_Water     = 3,
      C_Object    = 4,
      C_Ghost     = 5,
      C_Item      = 6,
      };

    enum MoveCode : uint8_t {
      MC_Fail,
      MC_OK,
      MC_Skip,
      MC_Partial,
      };

    struct CollisionTest {
      Tempest::Vec3 partial = {};
      Tempest::Vec3 normal  = {};
      bool          npcCol  = false;
      bool          preFall = false;
      Interactive*  vob     = nullptr;
      };

    struct NpcItem {
      public:
        NpcItem()=default;
        NpcItem(DynamicWorld* owner,NpcBody* obj,float r):owner(owner),obj(obj),r(r){}
        NpcItem(NpcItem&& it):owner(it.owner),obj(it.obj),r(it.r){it.obj=nullptr;}
        ~NpcItem();

        NpcItem& operator = (NpcItem&& it){
          std::swap(owner,it.owner);
          std::swap(obj,it.obj);
          std::swap(r,it.r);
          return *this;
          }

        void  setPosition(const Tempest::Vec3& pos);
        const Tempest::Vec3& position() const;

        void  setEnable(bool e);
        void  setUserPointer(void* p);

        float centerY() const;

        bool  testMove(const Tempest::Vec3& to, CollisionTest& out);
        bool  testMove(const Tempest::Vec3& to, const Tempest::Vec3& from, CollisionTest& out);
        auto  tryMove (const Tempest::Vec3& to, CollisionTest& out) -> DynamicWorld::MoveCode;

        bool  hasCollision() const;
        float radius() const { return r; }

      private:
        DynamicWorld*       owner  = nullptr;
        NpcBody*            obj    = nullptr;
        float               r      = 0.f;
        auto  implTryMove    (const Tempest::Vec3& dp, const Tempest::Vec3& pos0, CollisionTest& out) -> DynamicWorld::MoveCode;
        void  implSetPosition(const Tempest::Vec3& pos);

      friend class DynamicWorld;
      };

    struct Item {
      public:
        Item()=default;
        Item(DynamicWorld* owner, btCollisionObject* obj, btCollisionShape* shp):owner(owner),obj(obj),shp(shp){}
        Item(Item&& it):owner(it.owner),obj(it.obj),shp(it.shp){ it.obj=nullptr; it.shp=nullptr; }
        ~Item();

        Item& operator = (Item&& it){
          std::swap(owner,it.owner);
          std::swap(obj,  it.obj);
          std::swap(shp,  it.shp);
          return *this;
          }

        void setObjMatrix(const Tempest::Matrix4x4& m);
        void setItem(::Item* it);
        void setInteractive(Interactive* it);
        bool isEmpty() const { return obj==nullptr; }

      private:
        DynamicWorld*       owner  = nullptr;
        btCollisionObject*  obj    = nullptr;
        btCollisionShape*   shp    = nullptr;
      };

    struct RayLandResult {
      Tempest::Vec3       v={};
      Tempest::Vec3       n={};
      uint8_t             mat    = 0;
      bool                hasCol = false;
      float               hitFraction = 0;

      const char*         sector = nullptr;
      };

    struct RayWaterResult {
      float               wdepth  = 0.f;
      bool                hasCol = false;
      };

    struct RayQueryResult : RayLandResult{
      Npc* npcHit = nullptr;
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
        void  setTargetRange(float tgRange);

        void  move(const Tempest::Vec3& to);
        void  setPosition  (const Tempest::Vec3& pos);
        void  setDirection (const Tempest::Vec3& dir);
        float pathLength() const;
        void  addPathLen(float v);

        float               speed()       const { return dirL; }
        float               targetRange() const { return tgRange; }
        Tempest::Vec3       position()    const { return pos; }
        Tempest::Vec3       direction()   const { return dir; }
        Tempest::Matrix4x4  matrix()      const;
        bool                isSpell()     const { return spl!=std::numeric_limits<int>::max(); }
        int                 spellId()     const { return spl; }

        void                addHit() { hitCnt++; }
        int                 hitCount()    const { return hitCnt; }

      private:
        DynamicWorld*       owner = nullptr;
        BulletCallback*     cb    = nullptr;

        Tempest::Vec3       pos={};
        Tempest::Vec3       lastPos={};

        Tempest::Vec3       dir={};
        float               dirL=0.f;
        float               totalL=0.f;
        int                 spl=std::numeric_limits<int>::max();
        int                 hitCnt  = 0;
        float               tgRange = 0;

      friend class DynamicWorld;
      };

    struct BBoxCallback {
      BBoxCallback() = default;
      virtual ~BBoxCallback()=default;
      virtual void onCollide(BulletBody& other){(void)other;}
      };

    struct BBoxBody final {
      public:
        BBoxBody() = default;
        BBoxBody(DynamicWorld* wrld, BBoxCallback* cb, const ZMath::float3* bbox);
        BBoxBody(DynamicWorld* wrld, BBoxCallback* cb, const Tempest::Vec3& pos, float R);
        BBoxBody(BBoxBody&& other);
        ~BBoxBody();

        BBoxBody& operator=(BBoxBody&& other);

      private:
        DynamicWorld*       owner = nullptr;
        BBoxCallback*       cb    = nullptr;

        btCollisionShape*   shape = nullptr;
        btRigidBody*        obj   = nullptr;

      friend class DynamicWorld;
      };

    RayLandResult  landRay      (const Tempest::Vec3& from, float maxDy=0) const;
    RayWaterResult waterRay     (const Tempest::Vec3& from) const;

    RayLandResult  ray          (const Tempest::Vec3& from, const Tempest::Vec3& to) const;
    RayQueryResult rayNpc       (const Tempest::Vec3& from, const Tempest::Vec3& to) const;
    float          soundOclusion(const Tempest::Vec3& from, const Tempest::Vec3& to) const;

    NpcItem        ghostObj  (std::string_view visual);
    Item           staticObj (const PhysicMeshShape *src, const Tempest::Matrix4x4& m);
    Item           movableObj(const PhysicMeshShape *src, const Tempest::Matrix4x4& m);
    Item           dynamicObj(const Tempest::Matrix4x4& pos, const Bounds& bbox, ZenLoad::MaterialGroup mat);

    BulletBody*    bulletObj(BulletCallback* cb);
    BBoxBody       bboxObj(BBoxCallback* cb, const ZMath::float3* bbox);
    BBoxBody       bboxObj(BBoxCallback* cb, const Tempest::Vec3& pos, float R);

    void           tick(uint64_t dt);

    void           deleteObj(BulletBody* obj);

    static float   materialFriction(ZenLoad::MaterialGroup mat);
    static float   materialDensity (ZenLoad::MaterialGroup mat);

    std::string_view validateSectorName(std::string_view name) const;

  private:
    enum ItemType : uint8_t {
      IT_Static,
      IT_Movable,
      IT_Dynamic,
      };
    Item           createObj(btCollisionShape* shape, bool ownShape, const Tempest::Matrix4x4& m,
                             float mass, float friction, ItemType type);


    void           moveBullet(BulletBody& b, const Tempest::Vec3& dir, uint64_t dt);
    RayWaterResult implWaterRay(const Tempest::Vec3& from, const Tempest::Vec3& to) const;
    bool           hasCollision(const NpcItem &it, CollisionTest& out);

    std::unique_ptr<CollisionWorld>    world;

    std::vector<std::string>           sectors;

    std::vector<btVector3>             landVbo;
    std::unique_ptr<PhysicVbo>         landMesh;
    std::unique_ptr<btCollisionShape>  landShape;
    std::unique_ptr<btRigidBody>       landBody;

    std::unique_ptr<btCollisionShape>  waterShape;
    std::unique_ptr<btRigidBody>       waterBody;
    std::unique_ptr<PhysicVbo>         waterMesh;

    std::unique_ptr<NpcBodyList>       npcList;
    std::unique_ptr<BulletsList>       bulletList;
    std::unique_ptr<BBoxList>          bboxList;

    static const float                 ghostHeight;
    static const float                 worldHeight;
  };
