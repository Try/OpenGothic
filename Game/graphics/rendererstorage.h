#pragma once

#include <Tempest/RenderPass>
#include <Tempest/RenderPipeline>
#include <Tempest/UniformsLayout>
#include <Tempest/Shader>
#include <Tempest/Device>
#include <Tempest/Assets>

#include "graphics/objectsbucket.h"

class Gothic;
class Material;

class RendererStorage {
  public:
    RendererStorage(Tempest::Device& device, Gothic& gothic);

    Tempest::Device&        device;

    Tempest::RenderPipeline pSky;
    Tempest::RenderPipeline pFog;
    Tempest::RenderPipeline pLights;
    Tempest::RenderPipeline pComposeShadow;
    Tempest::RenderPipeline pCopy;

    enum PipelineType: uint8_t {
      T_Forward,
      T_Deffered,
      T_Shadow,
      };

    const Tempest::RenderPipeline* materialPipeline(const Material& desc, ObjectsBucket::Type t, PipelineType pt) const;

  private:
    struct ShaderPair {
      Tempest::Shader vs;
      Tempest::Shader fs;
      void load(Tempest::Device &device, const char* tag, const char* format);
      void load(Tempest::Device &device, const char* tag);
      };

    struct MaterialTemplate {
      ShaderPair obj, ani, mph;
      void load(Tempest::Device& device, const char* tag);
      };

    struct Entry {
      Tempest::RenderPipeline pipeline;
      Material::AlphaFunc     alpha        = Material::Solid;
      ObjectsBucket::Type     type         = ObjectsBucket::Static;
      PipelineType            pipelineType = PipelineType::T_Forward;
      };

    template<class Vertex>
    Tempest::RenderPipeline pipeline(Tempest::RenderState& st, const ShaderPair &fs) const;

    MaterialTemplate solid,  atest, solidF, atestF, water, ghost, emmision;
    MaterialTemplate shadow, shadowAt;
    mutable std::list<Entry> materials;
  };
