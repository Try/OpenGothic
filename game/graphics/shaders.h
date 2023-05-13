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

    Tempest::RenderPipeline  copy;
    Tempest::RenderPipeline  stash;

    Tempest::RenderPipeline  ssaoCompose;
    Tempest::ComputePipeline ssao, ssaoRq;

    Tempest::ComputePipeline irradiance;

    // Scalable and Production Ready Sky and Atmosphere
    Tempest::RenderPipeline  skyTransmittance, skyMultiScattering;
    Tempest::RenderPipeline  skyViewLut, sky;
    Tempest::RenderPipeline  fog;
    Tempest::RenderPipeline  fogViewLut, fog3dLQ, fog3dHQ;
    Tempest::RenderPipeline  sun;
    Tempest::ComputePipeline cloudsLut, fogOcclusion;
    Tempest::ComputePipeline fogViewLut3dLQ, fogViewLut3dHQ, shadowDownsample;

    Tempest::RenderPipeline  underwaterT, underwaterS;
    Tempest::RenderPipeline  waterReflection, waterReflectionSSR;

    Tempest::RenderPipeline  tonemapping;

    // Compute
    Tempest::ComputePipeline hiZRaw, hiZPot, hiZMip;

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

    static Shaders* instance;

    MaterialTemplate solid,  atest, solidF, atestF, water, ghost, emmision, multiply;
    MaterialTemplate shadow, shadowAt;
    mutable std::list<Entry> materials;
  };
