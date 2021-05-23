#pragma once

#include <zenload/zTypes.h>
#include <vector>
#include <list>
#include <cstdint>

#include "graphics/mesh/protomesh.h"
#include "physics/physics.h"

class PhysicVbo {
  public:
    PhysicVbo(ZenLoad::PackedMesh&& sPacked);
    PhysicVbo(const std::vector<Tempest::Vec3>* v);

    PhysicVbo(const PhysicVbo&)=delete;
    PhysicVbo(PhysicVbo&&)=delete;

    void    addIndex(std::vector<uint32_t>&& index, uint8_t material);
    void    addIndex(std::vector<uint32_t>&& index, uint8_t material, const char* sector);

    uint8_t materialId(size_t segment) const;
    auto    sectorName(size_t segment) const -> const char*;

    bool    useQuantization() const;
    bool    isEmpty() const;

    void    adjustMesh();

    reactphysics3d::CollisionShape* mkMesh(reactphysics3d::PhysicsCommon& common) const;

    const char* validateSectorName(const char* name) const;

  private:
    PhysicVbo(const std::vector<ZenLoad::WorldVertex>& v);

    void    addSegment(size_t indexSize,size_t offset,uint8_t material,const char* sector);

    struct Segment {
      size_t      off;
      size_t      size;
      uint8_t     mat;
      const char* sector=nullptr;
      };

    std::vector<Tempest::Vec3>        vStorage;
    const std::vector<Tempest::Vec3>& vert;
    std::vector<uint32_t>             id;
    std::vector<Segment>              segments;
    mutable std::list<reactphysics3d::TriangleVertexArray> vba;
  };
