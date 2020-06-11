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
    Tempest::RenderPipeline pLand, pLandAt, pLandAlpha, pObject, pObjectDecal, pAnim, pPfx, pSky;
    Tempest::RenderPipeline pLandSh, pLandAtSh, pObjectSh, pAnimSh;
    Tempest::RenderPipeline pComposeShadow;

    const Tempest::UniformsLayout& uboObjLayout() const { return pObject.layout(); }
    const Tempest::UniformsLayout& uboAniLayout() const { return pAnim.layout(); }
    const Tempest::UniformsLayout& uboLndLayout() const { return pLand.layout(); }
    const Tempest::UniformsLayout& uboPfxLayout() const { return pPfx.layout(); }
    const Tempest::UniformsLayout& uboSkyLayout() const { return pSky.layout(); }
    const Tempest::UniformsLayout& uboComposeLayout() const { return pComposeShadow.layout(); }

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

    Material                land, landAt, object, ani, pfx;

    void initPipeline(Gothic& gothic);
    void initShadow();
    template<class Vertex>
    Tempest::RenderPipeline pipeline(Tempest::RenderState& st, const ShaderPair &fs);
  };
