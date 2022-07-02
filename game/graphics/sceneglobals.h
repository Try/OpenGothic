#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Vec>
#include <list>

#include "lightsource.h"
#include "lightgroup.h"
#include "bindless.h"

class SceneGlobals final {
  public:
    SceneGlobals();
    ~SceneGlobals();

    enum VisCamera : uint8_t {
      V_Shadow0    = 0,
      V_Shadow1    = 1,
      V_ShadowLast = 1,
      V_Main       = 2,
      V_Count
      };

    void setViewProject(const Tempest::Matrix4x4& view, const Tempest::Matrix4x4& proj,
                        float zNear, float zFar,
                        const Tempest::Matrix4x4 *sh);

    void setTime(uint64_t time);
    void commitUbo(uint8_t fId);

    void setShadowMap(const Tempest::Texture2d* tex[]);

    const Tempest::Matrix4x4& viewProject() const;
    const Tempest::Matrix4x4& viewProjectInv() const;
    const Tempest::Matrix4x4& shadowView(uint8_t view) const;

    uint64_t                          tickCount = 0;
    const Tempest::Texture2d*         shadowMap[2] = {};

    Tempest::Matrix4x4                view, proj;

    const Tempest::Texture2d*         lightingBuf = &Resources::fallbackBlack();
    const Tempest::Texture2d*         gbufDiffuse = &Resources::fallbackBlack();
    const Tempest::Texture2d*         gbufNormals = &Resources::fallbackBlack();
    const Tempest::Texture2d*         gbufDepth   = &Resources::fallbackBlack();
    const Tempest::Texture2d*         hiZ         = &Resources::fallbackTexture();

    const Tempest::AccelerationStructure* tlas = nullptr;

    struct UboGlobal final {
      Tempest::Vec3                   lightDir   = {0,0,1};
      float                           shadowSize = 2048;
      Tempest::Matrix4x4              viewProject;
      Tempest::Matrix4x4              viewProjectInv;
      Tempest::Matrix4x4              shadowView[Resources::ShadowLayers];
      Tempest::Vec4                   lightAmb   = {0,0,0,0};
      Tempest::Vec4                   lightCl    = {1,1,1,0};
      Tempest::Vec4                   frustrum[6];
      Tempest::Vec3                   clipInfo;
      float                           padd0 = 0;
      Tempest::Vec3                   camPos;
      };

    Tempest::UniformBuffer<UboGlobal> uboGlobalPf[Resources::MaxFramesInFlight][V_Count];

    LightSource                       sun;
    Tempest::Vec3                     ambient;
    LightGroup                        lights;
    Frustrum                          frustrum[V_Count];

    bool                              zWindEnabled = false;
    Tempest::Vec2                     windDir = {0,1};
    uint64_t                          windPeriod = 6000;

    bool                              tlasEnabled = true;
    Bindless                          bindless;

  private:
    void                              initSettings();

    UboGlobal                         uboGlobal;
  };

