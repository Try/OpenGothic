#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/RenderPass>
#include <Tempest/CommandBuffer>
#include <Tempest/Matrix4x4>
#include <Tempest/Widget>
#include <Tempest/Device>
#include <Tempest/UniformBuffer>

#include "worldview.h"
#include "rendererstorage.h"

class Gothic;

class Renderer final {
  public:
    Renderer(Tempest::Device& device, Gothic &gothic);

    void initSwapchain(uint32_t w,uint32_t h);
    void onWorldChanged();

    void setDebugView(const std::array<float,3>& cam,const Tempest::PointF& spin,const float zoom);
    void draw(Tempest::CommandBuffer& cmd, uint32_t imgId, const Gothic& gothic);

    const RendererStorage&            storage() const { return stor; }

  private:
    Tempest::Device&                  device;
    Gothic&                           gothic;
    Tempest::Matrix4x4                view;

    Tempest::Texture2d                zbuffer;
    std::vector<Tempest::FrameBuffer> fbo3d;

    Tempest::RenderPass               mainPass;
    RendererStorage                   stor;

    Tempest::PointF                   spin;
    std::array<float,3>               cam;
    float                             zoom=1.f;
  };
