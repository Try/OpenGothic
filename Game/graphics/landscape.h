#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

class World;
class RendererStorage;

class Landscape final {
  public:
    Landscape(const RendererStorage& storage);

    void setMatrix(uint32_t imgId,const Tempest::Matrix4x4& mat);
    void commitUbo(uint32_t imgId);
    void draw(Tempest::CommandBuffer &cmd, uint32_t imgId, const World &world);

  private:
    struct UboLand {
      Tempest::Matrix4x4 mvp;
      };

    struct PerFrame {
      std::vector<Tempest::Uniforms> uboLand;
      Tempest::UniformBuffer         uboGpu;
      };

    const RendererStorage&         storage;
    UboLand                        uboCpu;
    std::unique_ptr<PerFrame[]>    landPF;
  };
