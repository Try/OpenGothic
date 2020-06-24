#pragma once

#include <Tempest/Device>
#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/CommandBuffer>
#include <Tempest/Matrix4x4>

#include <vector>

#include "resources.h"

class Bounds;

class Painter3d final {
  public:
    using Vertex    = Resources::Vertex;
    using VertexA   = Resources::VertexA;
    using VertexFsq = Resources::VertexFsq;

    Painter3d(Tempest::Encoder<Tempest::CommandBuffer>& enc);
    ~Painter3d();

    void setFrustrum(const Tempest::Matrix4x4& m);

    bool isVisible(const Bounds& b) const;

    void setViewport(int x,int y,int w,int h);

    void setUniforms(const Tempest::RenderPipeline& pipeline, const void* b, size_t sz);
    void draw(const Tempest::RenderPipeline& pipeline,
              const Tempest::Uniforms& ubo,
              const Tempest::VertexBuffer<Vertex>& vbo,
              const Tempest::IndexBuffer<uint32_t>& ibo);
    void draw(const Tempest::RenderPipeline& pipeline,
              const Tempest::Uniforms& ubo,
              const Tempest::VertexBuffer<VertexA>& vbo,
              const Tempest::IndexBuffer<uint32_t>& ibo);

    void draw(const Tempest::RenderPipeline& pipeline,
              const Tempest::Uniforms& ubo,
              const Tempest::VertexBuffer<VertexFsq>& vbo);
    void draw(const Tempest::RenderPipeline& pipeline,
              const Tempest::Uniforms& ubo,
              const Tempest::VertexBuffer<Vertex>& vbo);

  private:
    struct Frustrum {
      void make(const Tempest::Matrix4x4& m);
      void clear();

      bool testPoint(float x, float y, float z) const;
      bool testPoint(float x, float y, float z, float R) const;

      float f[6][4] = {};
      };

    Tempest::Encoder<Tempest::CommandBuffer>& enc;

    Frustrum              frustrum;
  };

