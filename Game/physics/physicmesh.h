#pragma once

#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>
#include <zenload/zTypes.h>
#include <vector>
#include <cstdint>

class PhysicMesh:public btTriangleIndexVertexArray {
  public:
    PhysicMesh(const ZenLoad::PackedMesh& sPacked);
    PhysicMesh(const std::vector<ZMath::float3>& v,const std::vector<uint32_t>& index);
    PhysicMesh(const std::vector<ZMath::float3>& v);

    PhysicMesh(const std::vector<ZenLoad::WorldVertex>& v);

    PhysicMesh(const PhysicMesh&)=delete;
    PhysicMesh(PhysicMesh&&)=delete;

    void addIndex(const std::vector<uint32_t>& index);

  private:
    struct Segment {
      size_t off;
      int    size;
      };

    std::vector<btVector3> vert;
    std::vector<uint32_t>  id;
    std::vector<Segment>   segments;

    void adjustMesh();
  };
