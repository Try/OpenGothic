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
    using Vertex  = Resources::Vertex;
    using VertexA = Resources::VertexA;

    Painter3d(Tempest::Device& device);
    ~Painter3d();

    void reset();
    void setPass(const Tempest::FrameBuffer& fbo, uint8_t frameId);
    void commit(Tempest::Encoder<Tempest::PrimaryCommandBuffer>& cmd);

    void setFrustrum(const Tempest::Matrix4x4& m);

    void draw(const Tempest::RenderPipeline& pipeline, const Bounds& bbox,
              const Tempest::Uniforms& ubo, uint32_t offset,
              const Tempest::VertexBuffer<Vertex>& vbo,
              const Tempest::IndexBuffer<uint32_t>& ibo);
    void draw(const Tempest::RenderPipeline& pipeline, const Bounds& bbox,
              const Tempest::Uniforms& ubo, uint32_t offset,
              const Tempest::VertexBuffer<VertexA>& vbo,
              const Tempest::IndexBuffer<uint32_t>& ibo);

  private:
    struct Frustrum {
      void make(const Tempest::Matrix4x4& m);
      void clear();

      bool testPoint(float x, float y, float z) const;
      bool testPoint(float x, float y, float z, float R) const;

      float f[6][4] = {};
      };

    struct PerFrame {
      PerFrame();
      std::vector<Tempest::CommandBuffer> cmd;
      };

    bool isVisible(const Bounds& b) const;

    using Recorder = Tempest::Encoder<Tempest::CommandBuffer>;

    Tempest::Device&      device;
    char                  encBuf[sizeof(Recorder)];
    Recorder*             enc = nullptr;

    PerFrame              pf[Resources::MaxFramesInFlight];
    PerFrame*             current = nullptr;

    Frustrum              frustrum;
    size_t                passId = 0;
  };

