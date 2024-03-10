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
    Tempest::RenderPipeline  fog3dLQ, fog3dHQ;
    Tempest::RenderPipeline  sun;
    Tempest::ComputePipeline cloudsLut, fogOcclusion;
    Tempest::ComputePipeline fogViewLut3dLQ, fogViewLut3dHQ, shadowDownsample;
    Tempest::ComputePipeline skyExposure;

    Tempest::RenderPipeline  underwaterT, underwaterS;
    Tempest::RenderPipeline  waterReflection, waterReflectionSSR;

    // VSM
    Tempest::ComputePipeline shadowPagesClr;
    Tempest::ComputePipeline shadowPages, shadowPages2;
    Tempest::ComputePipeline shadowWrite;

    Tempest::RenderPipeline  tonemapping;

    // AA
    Tempest::RenderPipeline  fxaaPresets[uint32_t(FxaaPreset::PRESETS_COUNT)];

    // HiZ
    Tempest::ComputePipeline hiZPot, hiZMip;
    Tempest::RenderPipeline  hiZReproj;

    // Cluster
    Tempest::ComputePipeline clusterInit, clusterPath;
    Tempest::ComputePipeline clusterTask, clusterTaskHiZ, clusterTaskHiZCr;

    // GI
    Tempest::RenderPipeline  probeDbg;
    Tempest::ComputePipeline probeInit, probeClear, probeClearHash, probeMakeHash;
    Tempest::ComputePipeline probeVote, probePrune, probeAlocation;
    Tempest::ComputePipeline probeTrace, probeLighting;
    Tempest::RenderPipeline  probeAmbient;

    Tempest::RenderPipeline  inventory;

    enum Flag : uint8_t {
      F_None     = 0x0,
      F_Bindless = 0x1,
      F_FTS      = 0x2
      };
    const Tempest::RenderPipeline* materialPipeline(const Material& desc, DrawCommands::Type t, PipelineType pt, Flag flags) const;

  private:
    struct Entry {
      Tempest::RenderPipeline pipeline;
      Material::AlphaFunc     alpha        = Material::Solid;
      DrawCommands::Type      type         = DrawCommands::Static;
      PipelineType            pipelineType = PipelineType::T_Main;
      Flag                    flags        = F_None;
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
