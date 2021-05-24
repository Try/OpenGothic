#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Vec>

#include <zenload/zTypes.h>

#include <memory>
#include <vector>

#include "physics/physics.h"

class btCollisionConfiguration;
class btConstraintSolver;

class CollisionWorld : public btDiscreteDynamicsWorld {
  public:
    CollisionWorld();

    static btVector3           toMeters     (const ZMath::float3& v);
    static btVector3           toMeters     (const Tempest::Vec3& v);
    static const Tempest::Vec3 toCentimeters(const btVector3& v);

    using btDiscreteDynamicsWorld::operator new;
    using btDiscreteDynamicsWorld::operator delete;

    class CollisionBody;
    class DynamicBody;
    class RayCallback;

    void tick(uint64_t dt);
    void setBBox(const btVector3& min, const btVector3& max);

    void updateAabbs() override;
    void touchAabbs();

    bool hasCollision(const btCollisionObject &it, Tempest::Vec3& normal);
    bool hasCollision(btRigidBody& it, Tempest::Vec3& normal);

    std::unique_ptr<CollisionBody> addCollisionBody(btCollisionShape& shape, const Tempest::Matrix4x4& tr, float friction);
    std::unique_ptr<DynamicBody>   addDynamicBody  (btCollisionShape& shape, const Tempest::Matrix4x4& tr, float friction, float mass);

    void rayCast(const Tempest::Vec3& b, const Tempest::Vec3& e, RayResultCallback& cb);

    class CollisionBody : public btRigidBody {
      public:
        ~CollisionBody();
      private:
        CollisionBody(btRigidBody::btRigidBodyConstructionInfo& inf, CollisionWorld* owner);
        CollisionWorld* owner = nullptr;
      friend class CollisionWorld;
      friend class CollisionWorld::DynamicBody;
      };

    class DynamicBody : public CollisionBody {
      DynamicBody(btRigidBody::btRigidBodyConstructionInfo& inf, CollisionWorld* owner)
        :CollisionBody(inf,owner){}
      friend class CollisionWorld;
      };

  private:
    struct Broadphase;
    struct ContructInfo;

    CollisionWorld(std::unique_ptr<btCollisionConfiguration>&& conf);
    CollisionWorld(ContructInfo ci);

    bool tick(float step, btRigidBody& body);

    void saveKinematicState(btScalar timeStep) override;

    std::unique_ptr<btCollisionConfiguration>   conf;
    std::unique_ptr<btCollisionDispatcher>      disp;
    std::unique_ptr<btBroadphaseInterface>      broad;
    std::unique_ptr<btSequentialImpulseConstraintSolver> solver;

    std::vector<btRigidBody*>                   rigid;
    btVector3                                   gravity = {};
    btVector3                                   bbox[2] = {};

    mutable uint32_t aabbChanged = 0;
  };

