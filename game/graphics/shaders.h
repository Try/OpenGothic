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
      T_Main,
      };

    static Shaders& inst();

    Tempest::RenderPipeline  lights, lightsRq;
    Tempest::RenderPipeline  directLight,  directLightSh, directLightRq;
    Tempest::RenderPipeline  ambientLight, ambientLightSsao;

    Tempest::ComputePipeline copyBuf;
    Tempest::ComputePipeline copyImg;
    Tempest::ComputePipeline path;
    Tempest::RenderPipeline  copy;
    Tempest::RenderPipeline  stash;

    Tempest::ComputePipeline ssao, ssaoBlur;

    Tempest::ComputePipeline irradiance;

    // Scalable and Production Ready Sky and Atmosphere
    Tempest::RenderPipeline  skyTransmittance, skyMultiScattering;
    Tempest::RenderPipeline  skyViewLut, skyViewCldLut, sky;
    Tempest::RenderPipeline  fog;
    Tempest::RenderPipeline  fog3dHQ;
    Tempest::RenderPipeline  sun;
    Tempest::ComputePipeline cloudsLut, fogOcclusion;
    Tempest::ComputePipeline fogViewLut3dLQ, fogViewLut3dHQ, shadowDownsample;
    Tempest::ComputePipeline skyExposure;

    Tempest::RenderPipeline  skyPathTrace;

    Tempest::RenderPipeline  underwaterT, underwaterS;
    Tempest::RenderPipeline  waterReflection, waterReflectionSSR;

    Tempest::RenderPipeline  tonemapping, tonemappingUpscale;
    Tempest::ComputePipeline tonemappingCompute;

    // AA
    Tempest::ComputePipeline cmaa2EdgeColor2x2Presets[uint32_t(Cmaa2Preset::PRESETS_COUNT)];
    Tempest::ComputePipeline cmaa2ComputeDispatchArgs, cmaa2ProcessCandidates, cmaa2DeferredColorApply2x2;

    // HiZ
    Tempest::ComputePipeline hiZPot, hiZMip;
    Tempest::RenderPipeline  hiZReproj;

    // Cluster
    Tempest::ComputePipeline clusterInit, clusterPath;
    Tempest::ComputePipeline clusterTask, clusterTaskHiZ, clusterTaskHiZCr;

    // GI
    Tempest::RenderPipeline  probeDbg, probeHitDbg;
    Tempest::ComputePipeline probeInit, probeClear, probeClearHash, probeMakeHash;
    Tempest::ComputePipeline probeVote, probePrune, probeAlocation;
    Tempest::ComputePipeline probeTrace, probeLighting;
    Tempest::RenderPipeline  probeAmbient;

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
    Tempest::RenderPipeline  postEffect(std::string_view vs, std::string_view fs, Tempest::RenderState::ZTestMode ztest = Tempest::RenderState::ZTestMode::LEqual);
    Tempest::ComputePipeline computeShader(std::string_view name);
    Tempest::RenderPipeline  fogShader (std::string_view name);
    Tempest::RenderPipeline  inWaterShader   (std::string_view name, bool isScattering);
    Tempest::RenderPipeline  reflectionShader(std::string_view name, bool hasMeshlets);
    Tempest::RenderPipeline  ambientLightShader(std::string_view name);

    static Shaders* instance;

    mutable std::list<Entry> materials;
  };
