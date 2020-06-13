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
    Tempest::RenderPipeline pAnimSh,   pAnim;
    Tempest::RenderPipeline pObjectSh, pObjectAtSh, pObject, pObjectAt, pObjectAlpha;

    Tempest::RenderPipeline pPfx, pSky;
    Tempest::RenderPipeline pComposeShadow;

    const Tempest::UniformsLayout& uboPfxLayout() const { return pPfx.layout(); }

  private:
    struct ShaderPair {
      Tempest::Shader vs;
      Tempest::Shader fs;
      void load(Tempest::Device &device, const char* tag, const char* format);
      };

    struct Material {
      ShaderPair main, shadow;
      void load(Tempest::Device& device, const char* f);
      };

    Material                object, objectAt, ani, pfx;

    void initPipeline(Gothic& gothic);
    void initShadow();
    template<class Vertex>
    Tempest::RenderPipeline pipeline(Tempest::RenderState& st, const ShaderPair &fs);
  };
