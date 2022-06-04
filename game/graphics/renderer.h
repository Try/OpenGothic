#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/CommandBuffer>
#include <Tempest/Matrix4x4>
#include <Tempest/Widget>
#include <Tempest/Device>
#include <Tempest/UniformBuffer>
#include <Tempest/VectorImage>

#include "worldview.h"
#include "shaders.h"

class Camera;
class InventoryMenu;

class Renderer final {
  public:
    Renderer(Tempest::Swapchain& swapchain);
    ~Renderer();

    void resetSwapchain();
    void onWorldChanged();

    void setCameraView(const Camera &camera);

    void draw(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t cmdId, size_t imgId,
              Tempest::VectorImage::Mesh& uiLayer, Tempest::VectorImage::Mesh& numOverlay,
              InventoryMenu &inventory);

    Tempest::Attachment       screenshoot(uint8_t frameId);

  private:
    void prepareUniforms();
    void drawHiZ (Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview, uint8_t cmdId);
    void drawDSM (Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& view);
    void drawSSAO(Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& view);
    void draw    (Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t cmdId);
    void initSettings();

    struct Settings {
      const uint32_t shadowResolution   = 2048;
      bool           zEnvMappingEnabled = false;
      bool           zCloudShadowScale  = false;
      } settings;

    Tempest::Swapchain&       swapchain;
    Tempest::Matrix4x4        view, proj, viewProj;
    Tempest::Matrix4x4        shadow[Resources::ShadowLayers];
    float                     zNear = 0;
    float                     zFar  = 0;
    Tempest::Vec3             clipInfo;

    Tempest::Attachment       shadowMap[Resources::ShadowLayers];
    Tempest::ZBuffer          zbuffer, zbufferItem, shadowZ[Resources::ShadowLayers];

    Tempest::Attachment       lightingBuf;
    Tempest::Attachment       gbufDiffuse;
    Tempest::Attachment       gbufNormal;
    Tempest::Attachment       gbufDepth;

    struct SSAO {
      Tempest::TextureFormat   aoFormat = Tempest::TextureFormat::R8;
      Tempest::Attachment      ssaoBuf;
      Tempest::Attachment      blurBuf;
      Tempest::RenderPipeline* ssaoPso = nullptr;
      Tempest::RenderPipeline* ssaoComposePso = nullptr;
      Tempest::DescriptorSet   uboSsao, uboBlur[2], uboCompose;
    } ssao;

    Tempest::TextureFormat    shadowFormat  = Tempest::TextureFormat::RGBA8;
    Tempest::TextureFormat    zBufferFormat = Tempest::TextureFormat::Depth16;

    Tempest::Attachment       hiZBase;
    Tempest::StorageImage     hiZPot;
    Tempest::StorageImage     hiZ;

    Tempest::DescriptorSet    uboHiZPot, uboHiZ;
    std::vector<Tempest::DescriptorSet> uboZMip;
    std::vector<Tempest::DescriptorSet> uboZGather;

    Tempest::DescriptorSet    uboCopy;
    Shaders                   stor;
  };
