#pragma once

#include <unordered_set>
#include <string>

#include "physics/physics.h"
#include "physicvbo.h"

class PhysicMeshShape final {
  public:
    PhysicMeshShape(const PhysicMeshShape&)=delete;

    float friction() const;
    bool  hasMaterial(const std::unordered_set<std::string>& names) const;

    static PhysicMeshShape* load(PackedMesh&& packed);
    static PhysicMeshShape* load(const PhysicMeshShape& src, const std::unordered_set<std::string>& noColl);

  private:
    PhysicMeshShape(PackedMesh&& packed);
    PhysicMeshShape(const PhysicMeshShape& src, const std::vector<bool>& keep);

    PhysicVbo                      mesh;
    mutable btBvhTriangleMeshShape shape;
    float                          frict = 0;
    std::vector<std::string>       material;

  friend class DynamicWorld;
  };
