#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/RenderPass>
#include <Tempest/CommandBuffer>
#include <Tempest/Matrix4x4>
#include <Tempest/Widget>
#include <Tempest/Device>
#include <Tempest/UniformBuffer>
#include <Tempest/VectorImage>
#include <Tempest/Pixmap>

#include "worldview.h"
#include "shaders.h"

class Camera;
class InventoryMenu;

class Renderer final {
  public:
    Renderer(Tempest::Swapchain& swapchain);

    void resetSwapchain();
    void onWorldChanged();

    void setCameraView(const Camera &camera);

    void draw(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t cmdId, size_t imgId,
              Tempest::VectorImage::Mesh& uiLayer, Tempest::VectorImage::Mesh& numOverlay,
              InventoryMenu &inventory);

    Tempest::Attachment               screenshoot(uint8_t frameId);

  private:
    Tempest::Swapchain&               swapchain;
    Tempest::Matrix4x4                view, proj, viewProj;
    Tempest::Matrix4x4                shadow[Resources::ShadowLayers];

    Tempest::Attachment               shadowMap[Resources::ShadowLayers];
    Tempest::ZBuffer                  zbuffer, zbufferItem, shadowZ[Resources::ShadowLayers];

    Tempest::Attachment               lightingBuf;
    Tempest::Attachment               gbufDiffuse;
    Tempest::Attachment               gbufNormal;
    Tempest::Attachment               gbufDepth;

    Tempest::Attachment               hiZ;
    Tempest::FrameBuffer              fboHiZ;
    Tempest::StorageBuffer            bufHiZ[Resources::MaxFramesInFlight];

    Tempest::TextureFormat            shadowFormat =Tempest::TextureFormat::RGBA8;
    Tempest::TextureFormat            zBufferFormat=Tempest::TextureFormat::Depth16;

    std::vector<Tempest::FrameBuffer> fbo3d, fboCpy, fboUi, fboItem;
    Tempest::FrameBuffer              fboShadow[2], fboGBuf;

    Tempest::RenderPass               mainPass, mainPassNoGbuf, gbufPass, gbufPass2, shadowPass, copyPass;
    Tempest::RenderPass               inventoryPass;
    Tempest::RenderPass               uiPass;

    Tempest::DescriptorSet            uboCopy, uboHiZ;
    Shaders                           stor;

    Tempest::Pixmap                   hiZCpu;

    void draw(Tempest::Encoder<Tempest::CommandBuffer> &cmd, Tempest::FrameBuffer& fbo, Tempest::FrameBuffer& fboCpy, Tempest::FrameBuffer& fboHiZ, uint8_t cmdId);
    void draw(Tempest::Encoder<Tempest::CommandBuffer> &cmd, Tempest::FrameBuffer& fbo, InventoryMenu& inv, uint8_t cmdId);
    void draw(Tempest::Encoder<Tempest::CommandBuffer> &cmd, Tempest::FrameBuffer& fbo, Tempest::VectorImage::Mesh& surface);
  };
