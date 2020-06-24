#pragma once

#include <Tempest/RenderPass>
#include <Tempest/RenderPipeline>
#include <Tempest/Shader>
#include <Tempest/UniformsLayout>
#include <Tempest/Device>
#include <Tempest/Assets>

class Gothic;

class RendererStorage {
  public:
    RendererStorage(Tempest::Device& device, Gothic& gothic);

    Tempest::Device&        device;

    Tempest::RenderPipeline pAnim,    pAnimAt,   pAnimLt,   pAnimAtLt;
    Tempest::RenderPipeline pObject,  pObjectAt, pObjectLt, pObjectAtLt;
    Tempest::RenderPipeline pObjectAlpha, pAnimAlpha;

    Tempest::RenderPipeline pObjectSh, pObjectAtSh;
    Tempest::RenderPipeline pAnimSh,   pAnimAtSh;

    Tempest::RenderPipeline pPfx, pSky;
    Tempest::RenderPipeline pComposeShadow;

  private:
    struct ShaderPair {
      Tempest::Shader vs;
      Tempest::Shader fs;
      void load(Tempest::Device &device, const char* tag, const char* format);
      };

    struct Material {
      ShaderPair main, shadow, light;
      void load(Tempest::Device& device, const char* f);
      };

    Material obj, objAt;
    Material ani, aniAt;
    Material pfx;

    void initPipeline(Gothic& gothic);
    void initShadow();
    template<class Vertex>
    Tempest::RenderPipeline pipeline(Tempest::RenderState& st, const ShaderPair &fs);
  };
