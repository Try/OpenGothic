#pragma once

#include <Tempest/Device>
#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/CommandBuffer>
#include <Tempest/Matrix4x4>

#include <vector>

#include "frustrum.h"
#include "resources.h"

class Bounds;
class LightSource;

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

  private:
    Tempest::Encoder<Tempest::CommandBuffer>& enc;

    Frustrum                                  frustrum;
    std::vector<Resources::Vertex>            vboCpu;
    Tempest::VertexBuffer<Resources::Vertex>  vbo[Resources::MaxFramesInFlight];
  };

