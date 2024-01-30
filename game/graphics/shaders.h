#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/Shader>
#include <Tempest/Device>
#include <list>

#include "graphics/drawstorage.h"

class Material;

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

    Tempest::RenderPipeline  tonemapping;

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

    const Tempest::RenderPipeline* materialPipeline(const Material& desc, DrawStorage::Type t, PipelineType pt) const;

  private:
    struct Entry {
      Tempest::RenderPipeline pipeline;
      Material::AlphaFunc     alpha        = Material::Solid;
      DrawStorage::Type       type         = DrawStorage::Static;
      PipelineType            pipelineType = PipelineType::T_Main;
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
