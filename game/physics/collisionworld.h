#pragma once

#include <Tempest/Vec>
#include <memory>
#include <vector>
#include <unordered_map>

#include "physics/physics.h"

class PhysicVbo;

class CollisionWorld {
  public:
    CollisionWorld();
    ~CollisionWorld();

    class RayCallback;

    reactphysics3d::CollisionShape* createConcaveMeshShape(const PhysicVbo* pvbo);
    reactphysics3d::CollisionShape* createConcaveMeshShape(reactphysics3d::TriangleMesh* mesh);
    reactphysics3d::CollisionShape* createBoxShape        (const Tempest::Vec3& sz);
    void                            deleteShape(reactphysics3d::CollisionShape* shp);

    reactphysics3d::CollisionBody*  createStatic   (reactphysics3d::CollisionShape* shp, const reactphysics3d::Transform& tr, uint16_t group=0xFFFF);
    reactphysics3d::CollisionBody*  createMovable  (reactphysics3d::CollisionShape* shp, const reactphysics3d::Transform& tr, uint16_t group=0xFFFF);
    reactphysics3d::CollisionBody*  createDynamic  (reactphysics3d::CollisionShape* shp, const reactphysics3d::Transform& tr, uint16_t group=0xFFFF);
    void                            deleteObj      (reactphysics3d::CollisionBody*  obj);

    void tick(uint64_t dt);
    void raycast(const Tempest::Vec3& from, const Tempest::Vec3& to, RayCallback& fn);

    bool hasCollision(reactphysics3d::CollisionBody &it, Tempest::Vec3& normal);

    class RayCallback {
      public:
        virtual bool onHit(reactphysics3d::Collider* obj, size_t submeshId, const Tempest::Vec3& pos, const Tempest::Vec3& norm);
      };

  private:
    reactphysics3d::PhysicsCommon common;
    reactphysics3d::PhysicsWorld* world = nullptr;

    std::unordered_map<const PhysicVbo*,reactphysics3d::CollisionShape*> meshes;
  };

