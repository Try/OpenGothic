#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/Shader>
#include <Tempest/Device>
#include <list>

#include "graphics/drawcommands.h"
#include "material.h"
#include "game/constants.h"

class Shaders {
  public:
    Shaders();
    ~Shaders();

    enum PipelineType: uint8_t {
      T_Depth,
      T_Shadow,
      T_Vsm,
      T_Main,
      };

    static Shaders& inst();
    static bool isVsmSupported();
    static bool isRtsmSupported();

    Tempest::RenderPipeline  lights, lightsRq, lightsVsm;
    Tempest::RenderPipeline  directLight,  directLightSh, directLightRq;
    Tempest::RenderPipeline  ambientLight, ambientLightSsao;

    Tempest::ComputePipeline copyBuf;
    Tempest::ComputePipeline copyImg;
    Tempest::ComputePipeline patch;
    Tempest::RenderPipeline  copy;
    Tempest::RenderPipeline  stash;

    Tempest::ComputePipeline ssao, ssaoBlur;

    Tempest::ComputePipeline irradiance;

    // Scalable and Production Ready Sky and Atmosphere
    Tempest::RenderPipeline  skyTransmittance, skyMultiScattering;
    Tempest::RenderPipeline  skyViewLut, skyViewCldLut, sky, skySep;
    Tempest::RenderPipeline  fog;
    Tempest::RenderPipeline  fog3dHQ;
    Tempest::RenderPipeline  sun;
    Tempest::ComputePipeline cloudsLut, fogOcclusion;
    Tempest::ComputePipeline fogViewLut3d, fogViewLutSep;
    Tempest::ComputePipeline skyExposure;

    Tempest::RenderPipeline  skyPathTrace;

    Tempest::RenderPipeline  underwaterT, underwaterS;
    Tempest::RenderPipeline  waterReflection, waterReflectionSSR;

    Tempest::RenderPipeline  tonemapping, tonemappingUpscale;

    // AA
    Tempest::ComputePipeline cmaa2EdgeColor2x2Presets[uint32_t(AaPreset::PRESETS_COUNT)];
    Tempest::ComputePipeline cmaa2ProcessCandidates;
    Tempest::RenderPipeline  cmaa2DeferredColorApply2x2;

    // HiZ
    Tempest::ComputePipeline hiZPot, hiZMip;

    // Cluster
    Tempest::ComputePipeline clusterInit, clusterPatch;
    Tempest::ComputePipeline visibilityPassSh, visibilityPassHiZ, visibilityPassHiZCr;

    // GI
    Tempest::RenderPipeline  probeDbg, probeHitDbg;
    Tempest::ComputePipeline probeInit, probeClear, probeClearHash, probeMakeHash;
    Tempest::ComputePipeline probeVote, probePrune, probeAlocation;
    Tempest::ComputePipeline probeTrace, probeLighting;
    Tempest::RenderPipeline  probeAmbient;

    // Epipolar
    Tempest::ComputePipeline fogEpipolarVsm;
    Tempest::ComputePipeline fogEpipolarOcclusion;

    // Virtual shadow
    Tempest::ComputePipeline vsmVisibilityPass;
    Tempest::ComputePipeline vsmClear, vsmClearOmni, vsmCullLights, vsmMarkPages, vsmMarkOmniPages, vsmPostprocessOmni;
    Tempest::ComputePipeline vsmTrimPages, vsmSortPages, vsmListPages, vsmClumpPages, vsmAllocPages, vsmAlloc2Pages, vsmMergePages;
    Tempest::ComputePipeline vsmPackDraw0, vsmPackDraw1;
    Tempest::ComputePipeline vsmFogPages, vsmFogShadow, vsmFogTrace;
    Tempest::RenderPipeline  vsmFog;
    Tempest::RenderPipeline  vsmDirectLight;
    Tempest::RenderPipeline  vsmDbg;

    Tempest::ComputePipeline vsmRendering;

    // RTSM (Experimental)
    Tempest::ComputePipeline rtsmClear, rtsmPages, rtsmFogPages, rtsmHiZ;
    Tempest::ComputePipeline rtsmCulling, rtsmPosition;
    Tempest::ComputePipeline rtsmMeshletCull, rtsmMeshletComplex, rtsmSampleCull, rtsmPrimCull;
    Tempest::ComputePipeline rtsmRaster;

    Tempest::ComputePipeline rtsmClearOmni;
    Tempest::ComputePipeline rtsmCullLights, rtsmCullingOmni;
    Tempest::ComputePipeline rtsmPositionOmni;
    Tempest::ComputePipeline rtsmLightTiles;
    Tempest::ComputePipeline rtsmMeshletOmni, rtsmBackfaceOmni, rtsmCompactOmni, rtsmPrimOmni;
    Tempest::ComputePipeline rtsmRasterOmni;

    Tempest::RenderPipeline  rtsmDirectLight;

    Tempest::ComputePipeline rtsmRendering, rtsmRenderingOmni; //reference
    Tempest::RenderPipeline  rtsmDbg;

    // Software rendering
    Tempest::ComputePipeline swRendering;
    Tempest::RenderPipeline  swRenderingDbg;

    // Inventory
    Tempest::RenderPipeline  inventory;

    const Tempest::RenderPipeline* materialPipeline(const Material& desc, DrawCommands::Type t, PipelineType pt, bool bindless) const;

  private:
    struct Entry {
      Tempest::RenderPipeline pipeline;
      Material::AlphaFunc     alpha        = Material::Solid;
      DrawCommands::Type      type         = DrawCommands::Static;
      PipelineType            pipelineType = PipelineType::T_Main;
      bool                    bindless     = false;
      bool                    trivial      = false;
      };

    Tempest::RenderPipeline  postEffect(std::string_view name);
    Tempest::RenderPipeline  postEffect(std::string_view name, Tempest::RenderState::ZTestMode ztest);
    Tempest::RenderPipeline  postEffect(std::string_view vs, std::string_view fs, Tempest::RenderState::ZTestMode ztest = Tempest::RenderState::ZTestMode::LEqual);
    Tempest::ComputePipeline computeShader(std::string_view name);
    Tempest::RenderPipeline  fogShader (std::string_view name);
    Tempest::RenderPipeline  inWaterShader   (std::string_view name, bool isScattering);
    Tempest::RenderPipeline  reflectionShader(std::string_view name, bool hasMeshlets);
    Tempest::RenderPipeline  ambientLightShader(std::string_view name);

    static Shaders* instance;

    mutable std::list<Entry> materials;
  };
