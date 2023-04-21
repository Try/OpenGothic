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

    void dbgDraw(Tempest::Painter& painter);

    Tempest::Attachment       screenshoot(uint8_t frameId);

  private:
    void prepareUniforms();
    void setupTlas(const Tempest::AccelerationStructure* tlas);

    void drawHiZ          (Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview, uint8_t fId);
    void drawGBuffer      (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view);
    void drawShadowMap    (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view);
    void drawShadowResolve(Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, const WorldView& view);
    void drawSSAO         (Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& view);
    void draw             (Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void drawTonemapping  (Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void stashDepthAux    (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void initSettings();

    struct Settings {
      const uint32_t shadowResolution   = 2048;
      bool           zEnvMappingEnabled = false;
      bool           zCloudShadowScale  = false;
      } settings;

    Frustrum                  frustrum[SceneGlobals::V_Count];
    Tempest::Swapchain&       swapchain;
    Tempest::Matrix4x4        view, proj, viewProj;
    Tempest::Matrix4x4        shadowMatrix[Resources::ShadowLayers];
    float                     zNear = 0;
    float                     zFar  = 0;
    Tempest::Vec3             clipInfo;

    Tempest::Attachment       sceneOpaque;
    Tempest::Attachment       sceneLinear;
    Tempest::Attachment       sceneDepth;
    Tempest::ZBuffer          zbuffer, zbufferItem, shadowMap[Resources::ShadowLayers];

    Tempest::Attachment       gbufDiffuse;
    Tempest::Attachment       gbufNormal;

    struct Shadow {
      Tempest::RenderPipeline* composePso = nullptr;
      Tempest::DescriptorSet   ubo[Resources::MaxFramesInFlight];
    } shadow;

    struct SSAO {
      Tempest::TextureFormat    aoFormat = Tempest::TextureFormat::R8;
      Tempest::StorageImage     ssaoBuf;

      Tempest::ComputePipeline* ssaoPso = nullptr;
      Tempest::DescriptorSet    uboSsao;

      Tempest::RenderPipeline*  ssaoComposePso = nullptr;
      Tempest::DescriptorSet    uboCompose;
    } ssao;

    struct Tonemapping {
      Tempest::RenderPipeline* pso = nullptr;
      Tempest::DescriptorSet   uboTone;
    } tonemapping;

    Tempest::TextureFormat    shadowFormat  = Tempest::TextureFormat::Depth16;
    Tempest::TextureFormat    zBufferFormat = Tempest::TextureFormat::Depth16;

    Tempest::StorageImage     hiZ;
    Tempest::StorageBuffer    hiZRaw;

    Tempest::DescriptorSet    uboHiZRaw, uboHiZPot;
    std::vector<Tempest::DescriptorSet> uboZMip;

    Tempest::DescriptorSet    uboCopy, uboCopyDepth;
    Shaders                   stor;
  };
