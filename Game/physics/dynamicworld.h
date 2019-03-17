#pragma once

#include "physicmesh.h"

#include <zenload/zCMesh.h>
#include <zenload/zTypes.h>
#include <LinearMath/btScalar.h>
#include <memory>
#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>
#include <Tempest/Matrix4x4>

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

class PhysicMeshShape;
class World;

class DynamicWorld final {
  public:
    DynamicWorld(World &world, const ZenLoad::PackedMesh &pkg);
    ~DynamicWorld();

    enum Category {
      C_Landscape,
      C_Ghost
      };

    struct Item {
      public:
        Item()=default;
        Item(DynamicWorld* owner,btCollisionObject* obj):owner(owner),obj(obj){}
        Item(Item&& it):owner(it.owner),obj(it.obj){it.obj=nullptr;}
        ~Item() { if(owner) owner->deleteObj(obj); }

        Item& operator = (Item&& it){
          std::swap(owner,it.owner);
          std::swap(obj,it.obj);
          return *this;
          }

        void setPosition(float x,float y,float z);
        void setObjMatrix(const Tempest::Matrix4x4& m);

        bool tryMove(const std::array<float,3>& pos);
        bool tryMove(const std::array<float,3>& pos, std::array<float,3> &fallback, float speed);

        bool hasCollision() const;

      private:
        DynamicWorld* owner=nullptr;
        btCollisionObject*  obj  =nullptr;
      friend class DynamicWorld;
      };

    float dropRay(float x, float y, float z,bool& hasCol) const;
    float dropRay(float x, float y, float z) const;
    std::array<float,3> landNormal(float x, float y, float z) const;

    std::array<float,3> ray(float x0, float y0, float z0,
                            float x1, float y1,float z1) const;
    std::array<float,3> ray(float x0, float y0, float z0,
                            float x1, float y1,float z1,bool& hasCol) const;

    Item ghostObj (float r, float height);
    Item staticObj(const PhysicMeshShape *src, const Tempest::Matrix4x4& m);

    void tick(uint64_t dt);

  private:
    void deleteObj(btCollisionObject* obj);
    bool hasCollision(const Item &it,std::array<float,3>& normal);

    struct HumShape;

    std::unique_ptr<btCollisionConfiguration>   conf;
    std::unique_ptr<btDispatcher>               dispatcher;
    std::unique_ptr<btBroadphaseInterface>      broadphase;
    std::unique_ptr<btCollisionWorld>           world;

    std::unique_ptr<PhysicMesh>                 landMesh;
    std::unique_ptr<btCollisionShape>           landShape;
    std::unique_ptr<btRigidBody>                landBody;

    static const float                          ghostPadding;
    static const float                          ghostHeight;
    static const float                          worldHeight;
  };
