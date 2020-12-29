#pragma once

#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>
#include <Tempest/Vec>
#include <memory>

class btCollisionConfiguration;
class btConstraintSolver;

class CollisionWorld : public btCollisionWorld {
  public:
    CollisionWorld();

    void tick(uint64_t dt);

    void updateAabbs() override;
    void touchAabbs();

    bool hasCollision(const btCollisionObject &it, Tempest::Vec3& normal);

    void addRigidBody   (btRigidBody* body);
    void removeRigidBody(btRigidBody* body);

  private:
    struct Broadphase;
    struct ContructInfo;

    CollisionWorld(std::unique_ptr<btCollisionConfiguration>&& conf);
    CollisionWorld(ContructInfo ci);

    bool tick(float step, btRigidBody& body);
    bool hasCollision(btRigidBody& it, Tempest::Vec3& normal);

    std::unique_ptr<btCollisionConfiguration>   conf;
    std::unique_ptr<btCollisionDispatcher>      disp;
    std::unique_ptr<btBroadphaseInterface>      broad;

    std::vector<btRigidBody*>                   rigid;
    btVector3                                   gravity = {};

    mutable uint32_t aabbChanged = 0;
  };

