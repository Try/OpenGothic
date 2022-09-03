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

    Tempest::RenderPipeline lights, lightsRq;
    Tempest::RenderPipeline copy;
    Tempest::RenderPipeline bilateralBlur;
    Tempest::RenderPipeline ssao, ssaoCompose;
    Tempest::RenderPipeline ssaoRq, ssaoComposeRq;

    // Scalable and Production Ready Sky and Atmosphere
    Tempest::RenderPipeline  skyTransmittance, skyMultiScattering, skyViewLut, sky;
    Tempest::RenderPipeline  fogViewLut, fog, fog3d;
    Tempest::ComputePipeline fogViewLut3D;

    // Compute
    Tempest::ComputePipeline hiZPot, hiZMip;

    enum PipelineType: uint8_t {
      T_Forward,
      T_Deffered,
      T_Shadow,
      };

    const Tempest::RenderPipeline* materialPipeline(const Material& desc, ObjectsBucket::Type t, PipelineType pt) const;
    Tempest::RenderPipeline lndPrePass;

  private:
    struct ShaderSet {
      Tempest::Shader vs;
      Tempest::Shader fs;
      Tempest::Shader tc, te;
      Tempest::Shader me;
      void load(Tempest::Device &device, const char* tag, const char* format, bool hasTesselation, bool hasMeshlets);
      void load(Tempest::Device &device, const char* tag, bool hasTesselation, bool hasMeshlets);
      };

    struct MaterialTemplate {
      ShaderSet lnd, obj, ani, mph, pfx;
      void load(Tempest::Device& device, const char* tag, bool hasTesselation=false, bool hasMeshlets=false);
      };

    struct Entry {
      Tempest::RenderPipeline pipeline;
      Material::AlphaFunc     alpha        = Material::Solid;
      ObjectsBucket::Type     type         = ObjectsBucket::Static;
      PipelineType            pipelineType = PipelineType::T_Forward;
      };

    Tempest::RenderPipeline  pipeline(Tempest::RenderState& st, const ShaderSet &fs) const;
    Tempest::RenderPipeline  postEffect(std::string_view name);
    Tempest::ComputePipeline computeShader(std::string_view name);
    Tempest::RenderPipeline  postEffect(std::string_view vs, std::string_view fs);
    Tempest::RenderPipeline  fogShader (std::string_view name);

    static Shaders* instance;

    MaterialTemplate solid,  atest, solidF, atestF, water, ghost, emmision;
    MaterialTemplate shadow, shadowAt;
    mutable std::list<Entry> materials;
  };
