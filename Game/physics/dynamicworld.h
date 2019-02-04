#pragma once

#include <zenload/zCMesh.h>
#include <zenload/zTypes.h>
#include <LinearMath/btScalar.h>
#include <memory>

class btConstraintSolver;
class btCollisionConfiguration;
class btBroadphaseInterface;
class btDispatcher;
class btDynamicsWorld;
class btTriangleIndexVertexArray;
class btCollisionShape;

class World;
class btRigidBody;

class DynamicWorld final {
  public:
    DynamicWorld(World &world, const ZenLoad::zCMesh &mesh);
    ~DynamicWorld();

    float dropRay(float x, float y, float z,bool& hasCol) const;
    float dropRay(float x, float y, float z) const;

  private:
    std::vector<int>      landIndex;
    std::vector<btScalar> landVert;

    std::unique_ptr<btCollisionConfiguration>   conf;
    std::unique_ptr<btDispatcher>               dispatcher;
    std::unique_ptr<btBroadphaseInterface>      broadphase;
    std::unique_ptr<btConstraintSolver>         solver;
    std::unique_ptr<btDynamicsWorld>            world;

    std::unique_ptr<btTriangleIndexVertexArray> landMesh;
    std::unique_ptr<btCollisionShape>           landShape;
    std::unique_ptr<btRigidBody>                landBody;
  };
