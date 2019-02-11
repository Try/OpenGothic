#pragma once

#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include "physicmesh.h"

class PhysicMeshShape final {
  public:
    PhysicMeshShape(const PhysicMeshShape&)=delete;

    static PhysicMeshShape* load(const ZenLoad::PackedMesh& sPacked);

  private:
    PhysicMeshShape(const ZenLoad::PackedMesh& sPacked);
    PhysicMesh             mesh;
    mutable btBvhTriangleMeshShape shape;

  friend class DynamicWorld;
  };
