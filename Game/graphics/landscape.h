#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

class Gothic;

class Landscape final {
  public:
    Landscape(Tempest::Device &device);

    const Tempest::UniformsLayout& uboLayout(){ return layout; }

    void commitUbo(const Tempest::Matrix4x4& mat,uint32_t imgId);
    void draw(Tempest::CommandBuffer &cmd, const Tempest::RenderPipeline &pLand, uint32_t imgId, const Gothic &gothic);

  private:
    Tempest::Device &device;

    struct UboLand {
      Tempest::Matrix4x4 mvp;
      };

    struct PerFrame {
      std::vector<Tempest::Uniforms> uboLand;
      Tempest::UniformBuffer         uboGpu;
      };

    Tempest::UniformsLayout        layout;
    UboLand                        uboCpu;
    std::unique_ptr<PerFrame[]>    landPF;
  };
