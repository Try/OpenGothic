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
    void draw(Tempest::PrimaryCommandBuffer &cmd, uint32_t imgId, const Gothic& gothic);
    void draw(Tempest::PrimaryCommandBuffer &cmd, uint32_t imgId, InventoryMenu& inv);

    const RendererStorage&            storage() const { return stor; }

  private:
    Tempest::Device&                  device;
    Gothic&                           gothic;
    Tempest::Matrix4x4                view;
    Tempest::Matrix4x4                shadow[2];

    Tempest::Texture2d                zbuffer;
    Tempest::Texture2d                shadowMap[2], shadowZ[2], shadowMapFinal;
    Tempest::TextureFormat            shadowFormat =Tempest::TextureFormat::RGBA8;
    Tempest::TextureFormat            zBufferFormat=Tempest::TextureFormat::Depth16;
    std::vector<Tempest::FrameBuffer> fbo3d;
    Tempest::FrameBuffer              fboShadow[2], fboCompose;

    Tempest::RenderPass               mainPass, shadowPass, composePass;
    Tempest::RenderPass               inventoryPass;

    Tempest::Uniforms                 uboShadowComp;
    RendererStorage                   stor;

    void composeShadow(Tempest::PrimaryCommandBuffer &cmd, Tempest::FrameBuffer &fbo);
  };
