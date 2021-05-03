#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wargument-outside-range"
#endif
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "physicvbo.h"

class PhysicMeshShape final {
  public:
    PhysicMeshShape(const PhysicMeshShape&)=delete;

    float friction() const;

    static PhysicMeshShape* load(ZenLoad::PackedMesh&& sPacked);

  private:
    PhysicMeshShape(ZenLoad::PackedMesh&& sPacked);
    PhysicVbo                      mesh;
    mutable btBvhTriangleMeshShape shape;
    float                          frict = 0;

  friend class DynamicWorld;
  };
