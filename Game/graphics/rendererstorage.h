#pragma once

#include <Tempest/RenderPass>
#include <Tempest/RenderPipeline>
#include <Tempest/Shader>
#include <Tempest/UniformsLayout>

class RendererStorage {
  public:
    RendererStorage(Tempest::Device& device);

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
    struct Material {
      Tempest::Shader vs,fs;
      Tempest::Shader vsShadow,fsShadow;

      void load(Tempest::Device &device,const char* f);
      };

    Tempest::UniformsLayout layoutLnd, layoutObj, layoutAni, layoutSky, layoutComp;

    Tempest::Shader         vsSky,fsSky;
    Tempest::Shader         vsComp,fsComp;
    Material                land, object, ani, pfx;

    void initPipeline();
    void initShadow();
  };
