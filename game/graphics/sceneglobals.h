#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Vec>
#include <list>

#include "graphics/dynamic/frustrum.h"
#include "lightsource.h"
#include "lightgroup.h"
#include "bindless.h"

class Sky;

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
    void setSunlight(const LightSource& light, const Tempest::Vec3& ambient, float GSunIntensity);
    void setExposure(float expInv);
    void setSky(const Sky& s);
    void setUnderWater(bool w);

    void setTime(uint64_t time);
    void commitUbo(uint8_t fId);

    void setResolution(uint32_t w, uint32_t h);
    void setHiZ(const Tempest::Texture2d& hiZ);
    void setShadowMap(const Tempest::Texture2d* tex[]);

    const Tempest::Matrix4x4& viewProject() const;
    const Tempest::Matrix4x4& viewProjectInv() const;
    const Tempest::Matrix4x4& viewShadow(uint8_t view) const;
    const Tempest::Vec3       clipInfo() const;

    uint64_t                          tickCount = 0;
    const Tempest::Texture2d*         shadowMap[2] = {};

    Tempest::Matrix4x4                view, proj;

    const Tempest::Texture2d*         sceneColor   = &Resources::fallbackBlack();
    const Tempest::Texture2d*         sceneDepth   = &Resources::fallbackBlack();
    const Tempest::Texture2d*         zbuffer      = &Resources::fallbackBlack();

    const Tempest::Texture2d*         gbufDiffuse  = &Resources::fallbackBlack();
    const Tempest::Texture2d*         gbufNormals  = &Resources::fallbackBlack();

    const Tempest::Texture2d*         hiZ          = &Resources::fallbackTexture();
    const Tempest::Texture2d*         skyLut       = &Resources::fallbackTexture();

    const Tempest::AccelerationStructure* tlas = nullptr;

    struct UboGlobal final {
      Tempest::Vec3                   sunDir     = {0,0,1};
      float                           waveAnim   = 0;
      Tempest::Matrix4x4              viewProject;
      Tempest::Matrix4x4              viewProjectInv;
      Tempest::Matrix4x4              viewShadow[Resources::ShadowLayers];
      Tempest::Vec3                   lightAmb      = {0,0,0};
      float                           exposureInv   = 1;
      Tempest::Vec3                   lightCl       = {1,1,1};
      float                           GSunIntensity = 0;
      Tempest::Vec4                   frustrum[6];
      Tempest::Vec3                   clipInfo;
      uint32_t                        tickCount32 = 0;
      Tempest::Vec3                   camPos;
      float                           isNight = 0;
      Tempest::Vec2                   screenResInv;
      Tempest::Vec2                   closeupShadowSlice;

      Tempest::Vec3                   pfxLeft  = {};
      uint32_t                        underWater = 0;
      Tempest::Vec3                   pfxTop   = {};
      float                           padd2 = 0;
      Tempest::Vec3                   pfxDepth = {};
      float                           padd3 = 0;
      Tempest::Point                  hiZTileSize = {};
      Tempest::Point                  screenRes = {};
      Tempest::Vec2                   cloudsDir[2] = {};
      };

    Tempest::UniformBuffer<UboGlobal> uboGlobalPf[Resources::MaxFramesInFlight][V_Count];

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

