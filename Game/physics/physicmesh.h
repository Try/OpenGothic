#pragma once

#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>
#include <zenload/zTypes.h>
#include <vector>
#include <cstdint>

#include "graphics/protomesh.h"

class PhysicMesh:public btTriangleIndexVertexArray {
  public:
    PhysicMesh(ZenLoad::PackedMesh&& sPacked);
    PhysicMesh(const std::vector<btVector3>* v);

    PhysicMesh(const PhysicMesh&)=delete;
    PhysicMesh(PhysicMesh&&)=delete;

    void    addIndex(std::vector<uint32_t>&& index, uint8_t material);
    uint8_t getMaterialId(size_t segment) const;
    bool    useQuantization() const;

    void    adjustMesh();

  private:
    PhysicMesh(const std::vector<ZenLoad::WorldVertex>& v);

    void addSegment(size_t indexSize,size_t offset,uint8_t material);

    struct Segment {
      size_t  off;
      int     size;
      uint8_t mat;
      };

    std::vector<btVector3>        vStorage;
    const std::vector<btVector3>& vert;
    std::vector<uint32_t>         id;
    std::vector<Segment>          segments;
  };
