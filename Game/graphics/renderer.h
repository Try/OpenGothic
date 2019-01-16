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
    void updateUbo  (const Tempest::FrameBuffer &fbo, const Gothic &gothic, uint32_t frameId);
    void drawLand   (Tempest::CommandBuffer &cmd, const Gothic &gothic, uint32_t frameId);
    void drawObjects(Tempest::CommandBuffer &cmd, const Gothic &gothic, uint32_t frameId);

    Tempest::RenderPipeline& landPipeline(Tempest::RenderPass& pass, uint32_t w, uint32_t h);

    struct UboLand {
      Tempest::Matrix4x4 mvp;
      uint8_t sz[0x100-sizeof(mvp)];
      };

    Tempest::Device&        device;
    Tempest::Texture2d      zbuffer;
    Tempest::Matrix4x4      view,projective;
    size_t                  isUboReady=0;

    Tempest::RenderPass     mainPass;
    Tempest::RenderPipeline pLand;
    std::vector<Tempest::FrameBuffer> fbo3d;

    Tempest::Shader         vsLand,fsLand;

    Tempest::UniformsLayout        layout;
    UboLand                        uboCpu;
    Tempest::UniformBuffer         uboGpu[3];
    std::vector<Tempest::Uniforms> uboLand;

    std::vector<Tempest::Uniforms> uboDodads;
    std::vector<UboLand>           uboObj;
    Tempest::UniformBuffer         uboObjGpu[3];

    Tempest::PointF         spin;
    float                   zoom=1.f;
  };
