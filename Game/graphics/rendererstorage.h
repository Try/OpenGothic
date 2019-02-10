#pragma once

#include <Tempest/RenderPass>
#include <Tempest/RenderPipeline>
#include <Tempest/Shader>
#include <Tempest/UniformsLayout>

class RendererStorage {
  public:
    RendererStorage(Tempest::Device& device);

    void initPipeline(Tempest::RenderPass &pass, uint32_t w, uint32_t h);

    Tempest::Device&        device;
    Tempest::RenderPipeline pLand, pLandAlpha, pObject, pAnim, pSky;

    const Tempest::RenderPass&     pass()         const { return *renderPass; }
    const Tempest::UniformsLayout& uboObjLayout() const { return layoutObj; }
    const Tempest::UniformsLayout& uboLndLayout() const { return layoutLnd; }
    const Tempest::UniformsLayout& uboSkyLayout() const { return layoutSky; }

  private:
    Tempest::RenderPass* renderPass=nullptr;

    Tempest::UniformsLayout layoutLnd, layoutObj, layoutAni, layoutSky;
    Tempest::Shader         vsLand,fsLand,vsObject,fsObject,vsAni,fsAni,vsSky,fsSky;
  };
