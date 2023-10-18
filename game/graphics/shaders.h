#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/Shader>
#include <Tempest/Device>

#include "graphics/objectsbucket.h"

class Material;

class Shaders {
  public:
    Shaders();
    ~Shaders();

    static Shaders& inst();

    Tempest::RenderPipeline  lights, lightsRq;
    Tempest::RenderPipeline  shadowResolve, shadowResolveSh, shadowResolveRq;

    Tempest::ComputePipeline copyBuf;
    Tempest::ComputePipeline copyImg;
    Tempest::ComputePipeline path;
    Tempest::RenderPipeline  copy;
    Tempest::RenderPipeline  stash;

    Tempest::RenderPipeline  ambientCompose, ambientComposeSsao;
    Tempest::ComputePipeline ssao;

    Tempest::ComputePipeline irradiance;

    // Scalable and Production Ready Sky and Atmosphere
    Tempest::RenderPipeline  skyTransmittance, skyMultiScattering;
    Tempest::RenderPipeline  skyViewLut, skyViewCldLut, sky;
    Tempest::RenderPipeline  fog;
    Tempest::RenderPipeline  fog3dLQ, fog3dHQ;
    Tempest::RenderPipeline  sun;
    Tempest::ComputePipeline cloudsLut, fogOcclusion;
    Tempest::ComputePipeline fogViewLut3dLQ, fogViewLut3dHQ, shadowDownsample;

    Tempest::RenderPipeline  underwaterT, underwaterS;
    Tempest::RenderPipeline  waterReflection, waterReflectionSSR;

    Tempest::RenderPipeline  tonemapping;

    // HiZ
    Tempest::ComputePipeline hiZPot, hiZMip;
    Tempest::RenderPipeline  hiZReproj;

    // GI
    Tempest::RenderPipeline  probeDbg;
    Tempest::ComputePipeline probeInit, probeClear, probeClearHash, probeMakeHash;
    Tempest::ComputePipeline probeVote, probePrune, probeAlocation;
    Tempest::ComputePipeline probeTrace, probeLighting;
    Tempest::RenderPipeline  probeDraw;

    enum PipelineType: uint8_t {
      T_Forward,
      T_Deffered,
      T_Shadow,
      };

    const Tempest::RenderPipeline* materialPipeline(const Material& desc, ObjectsBucket::Type t, PipelineType pt) const;
    Tempest::RenderPipeline lndPrePass;
    Tempest::RenderPipeline inventory;

  private:
    struct ShaderSet {
      Tempest::Shader vs;
      Tempest::Shader fs;
      Tempest::Shader tc, te;
      Tempest::Shader me, ts;
      void load(Tempest::Device &device, std::string_view tag, bool hasTesselation, bool hasMeshlets);
      };

    struct MaterialTemplate {
      ShaderSet lnd, obj, ani, mph, pfx;
      void load(Tempest::Device& device, std::string_view tag, bool hasTesselation=false, bool hasMeshlets=false);
      };

    struct Entry {
      Tempest::RenderPipeline pipeline;
      Material::AlphaFunc     alpha        = Material::Solid;
      ObjectsBucket::Type     type         = ObjectsBucket::Static;
      PipelineType            pipelineType = PipelineType::T_Forward;
      };

    Tempest::RenderPipeline  pipeline(Tempest::RenderState& st, const ShaderSet &fs) const;
    Tempest::RenderPipeline  postEffect(std::string_view name);
    Tempest::RenderPipeline  postEffect(std::string_view vs, std::string_view fs, Tempest::RenderState::ZTestMode ztest = Tempest::RenderState::ZTestMode::LEqual);
    Tempest::ComputePipeline computeShader(std::string_view name);
    Tempest::RenderPipeline  fogShader (std::string_view name);
    Tempest::RenderPipeline  inWaterShader   (std::string_view name, bool isScattering);
    Tempest::RenderPipeline  reflectionShader(std::string_view name, bool hasMeshlets);
    Tempest::RenderPipeline  ambientLightShader(std::string_view name);

    static Shaders* instance;

    MaterialTemplate solid,  atest, solidF, atestF, water, waterTess, ghost, emmision, multiply;
    MaterialTemplate shadow, shadowAt;
    mutable std::list<Entry> materials;
  };
