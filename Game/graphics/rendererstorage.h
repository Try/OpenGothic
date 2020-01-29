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
    Tempest::RenderPipeline pLand, pLandAt, pLandAlpha, pObject, pAnim, pPfx, pSky;
    Tempest::RenderPipeline pLandSh, pLandAtSh, pObjectSh, pAnimSh;
    Tempest::RenderPipeline pComposeShadow;

    const Tempest::UniformsLayout& uboObjLayout() const { return layoutObj; }
    const Tempest::UniformsLayout& uboLndLayout() const { return layoutLnd; }
    const Tempest::UniformsLayout& uboPfxLayout() const { return layoutLnd; }
    const Tempest::UniformsLayout& uboSkyLayout() const { return layoutSky; }
    const Tempest::UniformsLayout& uboComposeLayout() const { return layoutComp; }

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

    Tempest::UniformsLayout layoutLnd, layoutObj, layoutAni, layoutSky, layoutComp;
    Material                land, landAt, object, ani, pfx;

    void initPipeline(Gothic& gothic);
    void initShadow();
    template<class Vertex>
    Tempest::RenderPipeline pipeline(Tempest::RenderState& st, const Tempest::UniformsLayout& ulay,const ShaderPair &fs);
  };
