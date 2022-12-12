#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Device>

#include "graphics/material.h"
#include "graphics/bounds.h"

#include "resources.h"

class PackedMesh;

class StaticMesh {
  public:
    using Vertex=Resources::Vertex;

    StaticMesh(const PackedMesh& data);
    StaticMesh(const Material& mat, std::vector<Resources::Vertex> vbo, std::vector<uint32_t> ibo);
    StaticMesh(StaticMesh&&)=default;
    StaticMesh& operator=(StaticMesh&&)=default;

    struct SubMesh {
      Material                       material;
      size_t                         iboOffset = 0;
      size_t                         iboLength = 0;
      std::string                    texName;
      Tempest::AccelerationStructure blas;
      };

    struct Morph {
      std::string            name;
      size_t                 numFrames       = 0;
      size_t                 samplesPerFrame = 0;
      int32_t                layer           = 0;
      uint64_t               tickPerFrame    = 50;
      uint64_t               duration        = 0;

      size_t                 index           = 0;
      };

    struct MorphAnim {
      const std::vector<Morph>*     anim    = nullptr;
      const Tempest::StorageBuffer* index   = nullptr;
      const Tempest::StorageBuffer* samples = nullptr;
      };

    Tempest::VertexBuffer<Vertex>   vbo;
    Tempest::IndexBuffer<uint32_t>  ibo;
    Tempest::StorageBuffer          ibo8;
    MorphAnim                       morph;

    std::vector<SubMesh>            sub;
    Bounds                          bbox;
  };
