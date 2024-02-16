#pragma once

#include <vector>
#include <cstdint>

#include <zenkit/Material.hh>

#include "physics/physics.h"
#include "resources.h"

class PackedMesh;

class PhysicVbo : public btTriangleIndexVertexArray {
  public:
    using Vertex = Resources::Vertex;

    PhysicVbo(PackedMesh&& packed);
    PhysicVbo(const std::vector<btVector3>* v);

    PhysicVbo(const PhysicVbo&)=delete;
    PhysicVbo(PhysicVbo&&)=delete;

    void                    addIndex(const std::vector<uint32_t>& index, size_t iboOff, size_t iboLen, zenkit::MaterialGroup material);
    void                    addIndex(const std::vector<uint32_t>& index, size_t iboOff, size_t iboLen, zenkit::MaterialGroup material, const char* sector);
    zenkit::MaterialGroup   materialId(size_t segment) const;
    auto                    sectorName(size_t segment) const -> const char*;
    bool                    useQuantization() const;
    bool                    isEmpty() const;

    void                    adjustMesh();

    std::string_view        validateSectorName(std::string_view name) const;

  private:
    PhysicVbo(const std::vector<Vertex>& v);

    void addSegment(size_t indexSize,size_t offset, zenkit::MaterialGroup material, const char* sector);

    struct Segment {
      size_t                off;
      int                   size;
      zenkit::MaterialGroup mat;
      const char*           sector=nullptr;
      };

    std::vector<btVector3>        vStorage;
    const std::vector<btVector3>& vert;
    std::vector<uint32_t>         id;
    std::vector<Segment>          segments;
  };
