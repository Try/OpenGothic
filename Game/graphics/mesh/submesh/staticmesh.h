#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Device>

#include "graphics/material.h"
#include "graphics/bounds.h"

#include "resources.h"

class StaticMesh {
  public:
    using Vertex=Resources::Vertex;
    StaticMesh(const ZenLoad::PackedMesh& data);
    StaticMesh(const ZenLoad::PackedSkeletalMesh& data);
    StaticMesh(const Material& mat, std::vector<Resources::Vertex> vbo, std::vector<uint32_t> ibo);
    StaticMesh(StaticMesh&&)=default;
    StaticMesh& operator=(StaticMesh&&)=default;

    struct SubMesh {
      Material                       material;
      size_t                         iboOffset = 0;
      size_t                         iboSize   = 0;
      std::string                    texName;
      };

    Tempest::VertexBuffer<Vertex>  vbo;
    Tempest::IndexBuffer<uint32_t> ibo;

    std::vector<SubMesh>           sub;
    Bounds                         bbox;
  };
