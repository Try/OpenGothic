#pragma once

#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>
#include <zenload/zTypes.h>
#include <vector>
#include <cstdint>

#include "graphics/protomesh.h"

class PhysicMesh:public btTriangleIndexVertexArray {
  public:
    PhysicMesh(ZenLoad::PackedMesh&& sPacked);
    PhysicMesh(const std::vector<ZenLoad::WorldVertex>& v);
    PhysicMesh(const std::vector<btVector3>* v);

    PhysicMesh(const PhysicMesh&)=delete;
    PhysicMesh(PhysicMesh&&)=delete;

    void    addIndex(std::vector<uint32_t> index, uint8_t material);
    uint8_t getMaterialId(size_t segment) const;
    bool    useQuantization() const;

    ProtoMesh decalMesh(const std::string& tex, float x, float y, float z, float sX, float sY, float sZ) const;

  private:
    struct Segment {
      size_t  off;
      int     size;
      uint8_t mat;
      };

    std::vector<btVector3>        vStorage;
    const std::vector<btVector3>& vert;
    std::vector<uint32_t>         id;
    std::vector<Segment>          segments;

    void adjustMesh();

    static bool intersects(btVector3 a, btVector3 b, btVector3 c,
                           float x, float y, float z, float sX, float sY, float sZ);
  };
