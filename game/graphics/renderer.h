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
    enum Quality : uint8_t {
      None,
      VolumetricLQ,
      VolumetricHQ,
      Epipolar,
      PathTrace,
      };
    Tempest::Size internalResolution() const;
    float         internalResolutionScale() const;
    void updateCamera(const Camera &camera);

    bool requiresTlas() const;

    Tempest::StorageImage&  usesImage2d(Tempest::StorageImage& ret, Tempest::TextureFormat frm, uint32_t w, uint32_t h, bool mips = false);
    Tempest::StorageImage&  usesImage2d(Tempest::StorageImage& ret, Tempest::TextureFormat frm, Tempest::Size sz, bool mips = false);
    Tempest::StorageImage&  usesImage3d(Tempest::StorageImage& ret, Tempest::TextureFormat frm, uint32_t w, uint32_t h, uint32_t d, bool mips = false);
    Tempest::ZBuffer&       usesZBuffer(Tempest::ZBuffer&      ret, Tempest::TextureFormat frm, uint32_t w, uint32_t h);
    Tempest::StorageBuffer& usesSsbo(Tempest::StorageBuffer& ret, size_t size);
    Tempest::StorageBuffer& usesScratch(Tempest::StorageBuffer& ret, size_t size);

    void prepareUniforms();
    void resetShadowmap();
    void resetSkyFog();

    void prepareSky       (Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview);
    void prepareSSAO      (Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview);
    void prepareFog       (Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview);
    void prepareIrradiance(Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview);
    void prepareGi        (Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview);
    void prepareExposure  (Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview);
    void prepareEpipolar  (Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview);

    void drawHiZ          (Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& view);
    void buildHiZ         (Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void drawVsm          (Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& view);
    void drawRtsm         (Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& view);
    void drawRtsmOmni     (Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& view);
    void drawSwr          (Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& view);
    void drawGBuffer      (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view);
    void drawGWater       (Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& view);
    void drawShadowMap    (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view);
    void drawShadowResolve(Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& view);
    void drawLights       (Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& view);
    void drawSky          (Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& view);
    void drawAmbient      (Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& view);
    void draw             (Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void drawTonemapping  (Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview);
    void drawCMAA2        (Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview);
    void drawReflections  (Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview);
    void drawUnderwater   (Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview);
    void drawFog          (Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview);
    void drawSunMoon      (Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview);
    void drawSunMoon      (Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview, bool isSun);

    void drawSwRT         (Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview);

    void stashSceneAux    (Tempest::Encoder<Tempest::CommandBuffer>& cmd);

    void drawProbesDbg    (Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview);
    void drawProbesHitDbg (Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void drawVsmDbg       (Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview);
    void drawSwrDbg       (Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview);
    void drawRtsmDbg      (Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& wview);

    void setupSettings();
    void toggleGi();
    void toggleVsm();
    void toggleRtsm();

    struct Settings {
      const uint32_t shadowResolution   = 2048;
      bool           vsmEnabled         = false;
      bool           rtsmEnabled        = false;
      bool           swrEnabled         = false;
      bool           swrtEnabled        = false;

      bool           zEnvMappingEnabled = false;
      bool           zCloudShadowScale  = false;
      bool           zFogRadial         = false;

      bool           giEnabled          = false;
      bool           aaEnabled          = false;

      float          zVidBrightness     = 0.5;
      float          zVidContrast       = 0.5;
      float          zVidGamma          = 0.5;

      float          sunSize            = 0;
      float          moonSize           = 0;

      float          vidResIndex        = 0;

      float          vsmMipBias         = 0.25; //TODO: set to lower, eventually
      } settings;

    Frustrum                  frustrum[SceneGlobals::V_Count];
    Tempest::Swapchain&       swapchain;
    Tempest::Matrix4x4        proj, viewProj, viewProjLwc;
    Tempest::Matrix4x4        shadowMatrix[Resources::ShadowLayers];
    Tempest::Matrix4x4        shadowMatrixVsm;
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
      } shadow;

    struct Lights {
      Tempest::RenderPipeline* directLightPso = nullptr;
      } lights;

    struct Sky {
      Quality                quality       = Quality::None;

      Tempest::TextureFormat lutRGBFormat  = Tempest::TextureFormat::R11G11B10UF;
      Tempest::TextureFormat lutRGBAFormat = Tempest::TextureFormat::RGBA16F;

      bool                   lutIsInitialized = false;
      Tempest::Attachment    transLut, multiScatLut, viewLut, viewCldLut;
      Tempest::StorageImage  cloudsLut, fogLut3D, fogLut3DMs;
      Tempest::StorageImage  occlusionLut, irradianceLut;
      } sky;

    struct SSAO {
      Tempest::TextureFormat    aoFormat = Tempest::TextureFormat::R8;
      Tempest::StorageImage     ssaoBuf;
      Tempest::StorageImage     ssaoBlur;
      } ssao;

    struct Cmaa2 {
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
      } hiz;

    struct {
      const uint32_t            atlasDim  = 256; // sqrt(maxProbes)
      const uint32_t            maxProbes = atlasDim*atlasDim; // 65536

      Tempest::StorageBuffer    voteTable, hashTable, freeList;
      Tempest::StorageBuffer    probes;
      Tempest::StorageImage     probesGBuffDiff;
      Tempest::StorageImage     probesGBuffNorm;
      Tempest::StorageImage     probesGBuffRayT;
      Tempest::StorageImage     probesLighting;
      Tempest::StorageImage     probesLightingPrev;
      } gi;

    struct {
      Tempest::StorageBuffer    epipoles;
      Tempest::StorageImage     epTrace;
      } epipolar;

    const int32_t VSM_PAGE_SIZE = 128;
    struct {
      Tempest::StorageImage     pageTbl;
      Tempest::StorageImage     pageHiZ;
      Tempest::ZBuffer          pageData;
      Tempest::StorageBuffer    pageList;
      Tempest::StorageBuffer    pageListTmp;
      Tempest::StorageBuffer    pageTblOmni;
      Tempest::StorageBuffer    visibleLights;

      Tempest::StorageImage     fogDbg;
      Tempest::StorageImage     vsmDbg;
      } vsm;

    struct {
      Tempest::StorageImage     outputImage;
      Tempest::StorageImage     outputImageClr;

      Tempest::StorageImage     pages;
      Tempest::StorageBuffer    visList;
      Tempest::StorageBuffer    posList;

      Tempest::StorageImage     meshTiles;
      Tempest::StorageImage     primTiles;

      Tempest::StorageBuffer    visibleLights;
      Tempest::StorageBuffer    drawTasks;
      Tempest::StorageImage     lightTiles;
      Tempest::StorageImage     lightBins, primTilesOmni;

      Tempest::StorageImage     dbg64, dbg32, dbg16, dbg8;
      } rtsm;

    struct {
      Tempest::StorageImage     outputImage;
      } swr;

    struct {
      Tempest::StorageImage     outputImage;
      } swrt;

    Tempest::TextureFormat    shadowFormat  = Tempest::TextureFormat::Depth16;
    Tempest::TextureFormat    zBufferFormat = Tempest::TextureFormat::Depth16;

    Shaders                   shaders;
  };
