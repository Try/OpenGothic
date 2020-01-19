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
    Tempest::RenderPipeline pLand, pLandAlpha, pObject, pAnim, pPfx, pSky;
    Tempest::RenderPipeline pLandSh, pObjectSh, pAnimSh;
    Tempest::RenderPipeline pComposeShadow;

    const Tempest::UniformsLayout& uboObjLayout() const { return layoutObj; }
    const Tempest::UniformsLayout& uboLndLayout() const { return layoutLnd; }
    const Tempest::UniformsLayout& uboPfxLayout() const { return layoutLnd; }
    const Tempest::UniformsLayout& uboSkyLayout() const { return layoutSky; }
    const Tempest::UniformsLayout& uboComposeLayout() const { return layoutComp; }

  private:
    struct ShaderPair {
      const Tempest::Shader* vs = nullptr;
      const Tempest::Shader* fs = nullptr;
      void load(Tempest::Assets &asset, const char* tag, const char* format);
      };

    struct Material {
      ShaderPair main, shadow;
      void load(Tempest::Assets &asset, const char* f);
      };

    Tempest::UniformsLayout layoutLnd, layoutObj, layoutAni, layoutSky, layoutComp;

    Tempest::Assets         shaders;
    Material                land, object, ani, pfx;

    void initPipeline(Gothic& gothic);
    void initShadow();
    template<class Vertex>
    Tempest::RenderPipeline pipeline(Tempest::RenderState& st, const Tempest::UniformsLayout& ulay,const ShaderPair &fs);
  };
