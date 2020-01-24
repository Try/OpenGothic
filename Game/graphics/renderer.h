#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/RenderPass>
#include <Tempest/CommandBuffer>
#include <Tempest/Matrix4x4>
#include <Tempest/Widget>
#include <Tempest/Device>
#include <Tempest/UniformBuffer>
#include <Tempest/VectorImage>

#include "worldview.h"
#include "rendererstorage.h"

class Gothic;
class Camera;
class InventoryMenu;

class Renderer final {
  public:
    Renderer(Tempest::Device& device, Tempest::Swapchain& swapchain, Gothic &gothic);

    void resetSwapchain();
    void onWorldChanged();

    void setCameraView(const Camera &camera);
    bool needToUpdateCmd();

    void draw(Tempest::Encoder<Tempest::PrimaryCommandBuffer> &&cmd, uint32_t frameId, uint32_t imgId,
              Tempest::VectorImage& img, InventoryMenu &inventory, const Gothic& gothic);

    Tempest::Attachment               screenshoot(uint8_t frameId);
    const RendererStorage&            storage() const { return stor; }

  private:
    Tempest::Device&                  device;
    Tempest::Swapchain&               swapchain;
    Gothic&                           gothic;
    Tempest::Matrix4x4                view;
    Tempest::Matrix4x4                shadow[2];

    Tempest::Attachment               zbuffer, zbufferItem;
    Tempest::Attachment               shadowMap[2], shadowZ[2], shadowMapFinal;
    Tempest::TextureFormat            shadowFormat =Tempest::TextureFormat::RGBA8;
    Tempest::TextureFormat            zBufferFormat=Tempest::TextureFormat::Depth16;
    std::vector<Tempest::FrameBuffer> fbo3d, fboUi, fboItem;
    Tempest::FrameBuffer              fboShadow[2], fboCompose;

    Tempest::RenderPass               mainPass, shadowPass, composePass;
    Tempest::RenderPass               inventoryPass;
    Tempest::RenderPass               uiPass;

    Tempest::Uniforms                 uboShadowComp;
    RendererStorage                   stor;

    void draw(Tempest::Encoder<Tempest::PrimaryCommandBuffer> &cmd, Tempest::FrameBuffer& fbo, const Gothic& gothic, uint32_t frameId);
    void draw(Tempest::Encoder<Tempest::PrimaryCommandBuffer> &cmd, Tempest::FrameBuffer& fbo, InventoryMenu& inv);
    void draw(Tempest::Encoder<Tempest::PrimaryCommandBuffer> &cmd, Tempest::FrameBuffer& fbo, Tempest::VectorImage& surface);

    void composeShadow(Tempest::Encoder<Tempest::PrimaryCommandBuffer> &cmd, Tempest::FrameBuffer &fbo);
  };
