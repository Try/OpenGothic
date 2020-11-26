#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Vec>
#include <list>

#include "lightsource.h"
#include "lightgroup.h"

class RendererStorage;

class SceneGlobals final {
  public:
    SceneGlobals(const RendererStorage& storage);
    ~SceneGlobals();

    void setModelView(const Tempest::Matrix4x4& m, const Tempest::Matrix4x4 *sh, size_t shCount);
    void setTime(uint64_t time);
    void commitUbo(uint8_t fId);

    void setShadowMap(const Tempest::Texture2d& tex);

    const Tempest::Matrix4x4& viewProject() const;

    const RendererStorage&            storage;
    uint64_t                          tickCount = 0;
    const Tempest::Texture2d*         shadowMap = &Resources::fallbackBlack();

    const Tempest::Texture2d*         lightingBuf = &Resources::fallbackBlack();
    const Tempest::Texture2d*         gbufDiffuse = &Resources::fallbackBlack();
    const Tempest::Texture2d*         gbufNormals = &Resources::fallbackBlack();
    const Tempest::Texture2d*         gbufDepth   = &Resources::fallbackBlack();

    struct UboGlobal final {
      Tempest::Vec3                   lightDir={0,0,1};
      float                           shadowSize=2048;
      Tempest::Matrix4x4              viewProject;
      Tempest::Matrix4x4              viewProjectInv;
      Tempest::Matrix4x4              shadowView;
      Tempest::Vec4                   lightAmb={0,0,0,0};
      Tempest::Vec4                   lightCl ={1,1,1,0};
      float                           secondFrac;
      };

    Tempest::UniformBuffer<UboGlobal> uboGlobalPf[Resources::MaxFramesInFlight][Resources::ShadowLayers];

    LightSource                             sun;
    Tempest::Vec3                     ambient;
    LightGroup                        lights;

  private:
    UboGlobal                         uboGlobal;
    Tempest::Matrix4x4                shadowView1;
  };

