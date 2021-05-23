#pragma once

#include "physicvbo.h"
#include "physics.h"

class PhysicMeshShape final {
  public:
    PhysicMeshShape(const PhysicMeshShape&)=delete;
    PhysicVbo mesh;
    float     friction = 0;

    static PhysicMeshShape* load(ZenLoad::PackedMesh&& sPacked);

  private:
    PhysicMeshShape(ZenLoad::PackedMesh&& sPacked);
    //mutable btBvhTriangleMeshShape shape;
    //reactphysics3d::CollisionShape* shape = nullptr;

  friend class DynamicWorld;
  };
