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

    Tempest::RenderPipeline pAnim,   pAnimG,   pAnimAt,   pAnimAtG,   pAnimLt,   pAnimAtLt;
    Tempest::RenderPipeline pObject, pObjectG, pObjectAt, pObjectAtG, pObjectLt, pObjectAtLt;

    Tempest::RenderPipeline pObjectAlpha, pAnimAlpha;
    Tempest::RenderPipeline pObjectMAdd,  pAnimMAdd;

    Tempest::RenderPipeline pObjectSh, pObjectAtSh;
    Tempest::RenderPipeline pAnimSh,   pAnimAtSh;

    Tempest::RenderPipeline pSky;
    Tempest::RenderPipeline pLights;
    Tempest::RenderPipeline pComposeShadow;

  private:
    struct ShaderPair {
      Tempest::Shader vs;
      Tempest::Shader fs;
      void load(Tempest::Device &device, const char* tag, const char* format);
      void load(Tempest::Device &device, const char* tag);
      };

    struct Material {
      ShaderPair obj, ani;
      void load(Tempest::Device& device, const char* tag);
      };
    // ShaderPair obj, objAt, objG, objAtG, objEmi;
    // ShaderPair ani, aniAt, aniG, aniAtG, aniEmi;

    Material obj, objAt, objG, objAtG, objEmi, objShadow, objShadowAt;

    void initPipeline(Gothic& gothic);
    void initShadow();
    template<class Vertex>
    Tempest::RenderPipeline pipeline(Tempest::RenderState& st, const ShaderPair &fs);
  };
