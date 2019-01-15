#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/RenderPass>
#include <Tempest/CommandBuffer>
#include <Tempest/Matrix4x4>
#include <Tempest/Widget>
#include <Tempest/Device>
#include <Tempest/UniformBuffer>

class Gothic;

class Renderer final {
  public:
    Renderer(Tempest::Device& device);

    void initSwapchain(uint32_t w,uint32_t h);

    void setDebugView(const Tempest::PointF& spin,const float zoom);
    void draw(Tempest::CommandBuffer& cmd, uint32_t imgId, const Gothic& gothic);

  private:
    void setupShaderConstants(const Tempest::FrameBuffer &window);
    Tempest::RenderPipeline& landPipeline(Tempest::RenderPass& pass, uint32_t w, uint32_t h);

    struct Ubo {
      Tempest::Matrix4x4 mvp;
      };
    Tempest::Device&        device;
    Tempest::Texture2d      zbuffer;

    Tempest::RenderPass     mainPass;
    Tempest::RenderPipeline pLand;
    std::vector<Tempest::FrameBuffer> fbo3d;

    Tempest::Shader         vsLand,fsLand;

    Tempest::UniformsLayout layout;
    Ubo                     uboCpu;
    Tempest::UniformBuffer  uboGpu[3];

    std::vector<Tempest::Uniforms> uboArr;

    Tempest::PointF         spin;
    float                   zoom=1.f;
  };
