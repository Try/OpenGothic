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
class VideoWidget;

class Renderer final {
  public:
    Renderer(Tempest::Swapchain& swapchain);
    ~Renderer();

    void resetSwapchain();
    void onWorldChanged();

    void draw(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t cmdId, size_t imgId,
              Tempest::VectorImage::Mesh& uiLayer, Tempest::VectorImage::Mesh& numOverlay,
              InventoryMenu &inventory, VideoWidget& video);

    void dbgDraw(Tempest::Painter& painter);

    Tempest::Attachment screenshoot(uint8_t frameId);

  private:
    Tempest::Size internalResolution() const;
    void updateCamera(const Camera &camera);

    void prepareUniforms();
    void prepareRtUniforms();

    void prepareSky       (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view);
    void prepareSSAO      (Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void prepareFog       (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view);
    void prepareIrradiance(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& wview);
    void prepareGi        (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void prepareExposure  (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view);

    void drawHiZ          (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view);
    void buildHiZ         (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void drawVsm          (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view);
    void drawGBuffer      (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view);
    void drawGWater       (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view);
    void drawShadowMap    (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view);
    void drawShadowResolve(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, const WorldView& view);
    void drawLights       (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view);
    void drawSky          (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view);
    void drawAmbient      (Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& view);
    void draw             (Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void drawTonemapping  (Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void drawCMAA2        (Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void drawReflections  (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void drawUnderwater   (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);

    void drawProbesDbg    (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void drawProbesHitDbg (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void stashSceneAux    (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);

    void drawVsmDbg       (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);

    void initGiData();
    void initSettings();
    void toggleGi();

    struct Settings {
      const uint32_t shadowResolution   = 2048;
      bool           vsmEnabled         = true;
      bool           zEnvMappingEnabled = false;
      bool           zCloudShadowScale  = false;
      bool           giEnabled          = false;
      bool           aaEnabled          = false;

      float          zVidBrightness     = 0.5;
      float          zVidContrast       = 0.5;
      float          zVidGamma          = 0.5;

      float          vidResIndex        = 0;
      } settings;

    Frustrum                  frustrum[SceneGlobals::V_Count];
    Tempest::Swapchain&       swapchain;
    Tempest::Matrix4x4        proj, viewProj, viewProjLwc;
    Tempest::Matrix4x4        shadowMatrix[Resources::ShadowLayers];
    Tempest::Vec3             clipInfo;

    Tempest::Attachment       sceneLinear;
    Tempest::ZBuffer          zbuffer, shadowMap[Resources::ShadowLayers];
    Tempest::ZBuffer          zbufferUi;

    Tempest::Attachment       sceneOpaque;
    Tempest::Attachment       sceneDepth;

    Tempest::Attachment       gbufDiffuse;
    Tempest::Attachment       gbufNormal;

    struct Shadow {
      Tempest::RenderPipeline* directLightPso = nullptr;
      Tempest::DescriptorSet   ubo;
      } shadow;

    struct Water {
      Tempest::RenderPipeline* reflectionsPso = nullptr;
      Tempest::DescriptorSet   ubo;
      Tempest::DescriptorSet   underUbo;
      } water;

    struct SSAO {
      Tempest::TextureFormat    aoFormat = Tempest::TextureFormat::R8;
      Tempest::StorageImage     ssaoBuf;

      Tempest::DescriptorSet    uboBlur;
      Tempest::StorageImage     ssaoBlur;

      Tempest::ComputePipeline* ssaoPso = nullptr;
      Tempest::DescriptorSet    uboSsao;

      Tempest::RenderPipeline*  ambientLightPso = nullptr;
      Tempest::DescriptorSet    uboCompose;
      } ssao;

    struct Tonemapping {
      Tempest::RenderPipeline*  pso = nullptr;
      Tempest::DescriptorSet    uboTone;
      } tonemapping;

    struct Cmaa2 {
      Tempest::ComputePipeline* detectEdges2x2 = nullptr;
      Tempest::DescriptorSet    detectEdges2x2Ubo;

      Tempest::ComputePipeline* processCandidates = nullptr;
      Tempest::DescriptorSet    processCandidatesUbo;

      Tempest::RenderPipeline*  defferedColorApply = nullptr;
      Tempest::DescriptorSet    defferedColorApplyUbo;

      Tempest::StorageImage     workingEdges;
      Tempest::StorageBuffer    shapeCandidates;
      Tempest::StorageBuffer    deferredBlendLocationList;
      Tempest::StorageBuffer    deferredBlendItemList;
      Tempest::StorageImage     deferredBlendItemListHeads;
      Tempest::StorageBuffer    controlBuffer;
      Tempest::StorageBuffer    indirectBuffer;
      } cmaa2;

    struct {
      Tempest::StorageImage     hiZ;
      Tempest::StorageImage     counter;
      Tempest::StorageBuffer    counterBuf;

      bool                      atomicImg = false;
      Tempest::DescriptorSet    uboPot;
      Tempest::DescriptorSet    uboMip;

      Tempest::ZBuffer          smProj;
      Tempest::DescriptorSet    uboReproj;

      Tempest::StorageImage     hiZSm1;
      Tempest::DescriptorSet    uboPotSm1;
      Tempest::DescriptorSet    uboMipSm1;
      } hiz;

    struct {
      const uint32_t            atlasDim  = 256; // sqrt(maxProbes)
      const uint32_t            maxProbes = atlasDim*atlasDim; // 65536
      Tempest::DescriptorSet    uboDbg, uboHitDbg;

      Tempest::ComputePipeline* probeInitPso   = nullptr;

      Tempest::ComputePipeline* probeClearPso  = nullptr;
      Tempest::ComputePipeline* probeClearHPso = nullptr;
      Tempest::ComputePipeline* probeMakeHPso  = nullptr;
      Tempest::DescriptorSet    uboClear;

      Tempest::ComputePipeline* probeVotePso   = nullptr;
      Tempest::ComputePipeline* probePrunePso  = nullptr;
      Tempest::ComputePipeline* probeAllocPso  = nullptr;
      Tempest::DescriptorSet    uboProbes;

      Tempest::DescriptorSet    uboPrevIrr, uboZeroIrr;

      Tempest::ComputePipeline* probeTracePso = nullptr;
      Tempest::DescriptorSet    uboTrace;

      Tempest::ComputePipeline* probeLightPso = nullptr;
      Tempest::DescriptorSet    uboLight;

      Tempest::RenderPipeline*  ambientLightPso = nullptr;
      Tempest::DescriptorSet    uboCompose;

      Tempest::StorageBuffer    voteTable, hashTable, freeList;
      Tempest::StorageBuffer    probes;
      Tempest::StorageImage     probesGBuffDiff;
      Tempest::StorageImage     probesGBuffNorm;
      Tempest::StorageImage     probesGBuffRayT;
      Tempest::StorageImage     probesLighting;
      Tempest::StorageImage     probesLightingPrev;
      bool                      fisrtFrame = false;
      } gi;

    struct {
      Tempest::ComputePipeline* pagesClearPso = nullptr;
      Tempest::DescriptorSet    uboClear;

      Tempest::ComputePipeline* pagesMarkPso = nullptr;
      Tempest::DescriptorSet    uboPages;

      Tempest::ComputePipeline* pagesListPso = nullptr;
      Tempest::DescriptorSet    uboList;

      Tempest::ComputePipeline* pagesComposePso = nullptr;
      Tempest::DescriptorSet    uboCompose;

      Tempest::RenderPipeline*  pagesDbgPso = nullptr;
      Tempest::DescriptorSet    uboDbg;

      Tempest::StorageImage     pageTbl;
      Tempest::StorageImage     pageData;
      Tempest::StorageBuffer    pageList;
      Tempest::StorageImage     shadowMask;
      } vsm;

    Tempest::TextureFormat    shadowFormat  = Tempest::TextureFormat::Depth16;
    Tempest::TextureFormat    zBufferFormat = Tempest::TextureFormat::Depth16;

    Tempest::DescriptorSet    uboStash;

    Shaders                   stor;
  };
