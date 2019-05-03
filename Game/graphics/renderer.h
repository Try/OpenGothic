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
class Camera;
class InventoryMenu;

class Renderer final {
  public:
    Renderer(Tempest::Device& device, Gothic &gothic);

    void initSwapchain(uint32_t w,uint32_t h);
    void onWorldChanged();

    void setCameraView(const Camera &camera);
    bool needToUpdateCmd();
    void updateCmd();
    void draw(Tempest::CommandBuffer& cmd, uint32_t imgId, const Gothic& gothic);
    void draw(Tempest::CommandBuffer& cmd, uint32_t imgId, InventoryMenu& inv);

    const RendererStorage&            storage() const { return stor; }

  private:
    Tempest::Device&                  device;
    Gothic&                           gothic;
    Tempest::Matrix4x4                view, shadow;

    Tempest::Texture2d                zbuffer;
    Tempest::Texture2d                shadowMap, shadowZ;
    std::vector<Tempest::FrameBuffer> fbo3d;
    Tempest::FrameBuffer              fboShadow;

    Tempest::RenderPass               mainPass, shadowPass;
    RendererStorage                   stor;
  };
