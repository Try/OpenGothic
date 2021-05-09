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

    enum VisCamera : uint8_t {
      V_Shadow0    = 0,
      V_Shadow1    = 1,
      V_ShadowLast = 1,
      V_Main       = 2,
      V_Count
      };

    void setViewProject(const Tempest::Matrix4x4& view, const Tempest::Matrix4x4& proj);
    void setModelView  (const Tempest::Matrix4x4& m, const Tempest::Matrix4x4 *sh, size_t shCount);

    void setTime(uint64_t time);
    void commitUbo(uint8_t fId);

    void setShadowMap(const Tempest::Texture2d* tex[]);

    const Tempest::Matrix4x4& viewProject() const;
    const Tempest::Matrix4x4& viewProjectInv() const;

    const RendererStorage&            storage;
    uint64_t                          tickCount = 0;
    const Tempest::Texture2d*         shadowMap[2] = {};

    Tempest::Matrix4x4                view, proj;

    const Tempest::Texture2d*         lightingBuf = &Resources::fallbackBlack();
    const Tempest::Texture2d*         gbufDiffuse = &Resources::fallbackBlack();
    const Tempest::Texture2d*         gbufNormals = &Resources::fallbackBlack();
    const Tempest::Texture2d*         gbufDepth   = &Resources::fallbackBlack();

    struct UboGlobal final {
      Tempest::Vec3                   lightDir   = {0,0,1};
      float                           shadowSize = 2048;
      Tempest::Matrix4x4              viewProject;
      Tempest::Matrix4x4              viewProjectInv;
      Tempest::Matrix4x4              shadowView[2];
      Tempest::Vec4                   lightAmb   = {0,0,0,0};
      Tempest::Vec4                   lightCl    = {1,1,1,0};
      float                           secondFrac;
      };

    Tempest::UniformBuffer<UboGlobal> uboGlobalPf[Resources::MaxFramesInFlight][V_Count];

    LightSource                       sun;
    Tempest::Vec3                     ambient;
    LightGroup                        lights;

  private:
    UboGlobal                         uboGlobal;
  };

