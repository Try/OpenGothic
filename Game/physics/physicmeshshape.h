#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

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
