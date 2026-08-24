#pragma once

#include "physics/physics.h"
#include "physicvbo.h"

class PhysicMeshShape final {
  public:
    PhysicMeshShape(const PhysicMeshShape&)=delete;

    float friction() const;

    static PhysicMeshShape* load(PackedMesh&& packed);

  private:
    PhysicMeshShape(PackedMesh&& packed);

    PhysicVbo                      mesh;
    mutable btBvhTriangleMeshShape shape;
    float                          frict = 0;

  friend class DynamicWorld;
  };
