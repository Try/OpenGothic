#pragma once

#include <Tempest/RenderState>

#include <phoenix/ext/daedalus_classes.hh>

#include <Tempest/Texture2d>

#include "graphics/visualfx.h"
#include "graphics/material.h"

class PfxEmitterMesh;

class ParticleFx final {
  public:
    ParticleFx(const Material& mat, const phoenix::vob& vob);
    ParticleFx(const phoenix::c_particle_fx& src, std::string_view name);
    ParticleFx(const ParticleFx& proto, const VisualFx::Key& key);

    enum class EmitterType:uint8_t {
      Point,
      Line,
      Box,
      Circle,
      Sphere,
      Mesh
      };

    enum class Dir:uint8_t {
      Rand,
      Dir,
      Target
      };

    enum class Frame:uint8_t {
      World,
      Object,
      Node,
      };

    enum class Distribution:uint8_t {
      Rand,
      Dir,
      Uniform,
      Walk
      };

    enum class Orientation:uint8_t {
      None,
      Velocity,
      Velocity3d,
      };

    using KeyList = std::vector<float>;

    std::string   dbgName;

    float         ppsValue            = 0;
    KeyList       ppsScaleKeys;
    bool          ppsIsLooping        = false;
    bool          ppsIsSmooth         = false;
    float         ppsFPS              = 0;
    const ParticleFx* ppsCreateEm     = nullptr;
    uint64_t      ppsCreateEmDelay    = 0;

    EmitterType   shpType             = EmitterType::Point;
    Frame         shpFOR              = Frame::Object;
    Tempest::Vec3 shpOffsetVec;
    Distribution  shpDistribType      = Distribution::Rand;
    float         shpDistribWalkSpeed = 0.f;
    bool          shpIsVolume         = false;
    Tempest::Vec3 shpDim;
    const PfxEmitterMesh* shpMesh     = nullptr;
    std::string   shpMesh_S;
    bool          shpMeshRender       = false;
    KeyList       shpScaleKeys;
    bool          shpScaleIsLooping   = false;
    bool          shpScaleIsSmooth    = false;
    float         shpScaleFPS         = 0.f;

    Dir           dirMode             = Dir::Rand;
    Frame         dirFOR              = Frame::Object;
    Frame         dirModeTargetFOR    = Frame::Object;
    Tempest::Vec3 dirModeTargetPos;
    float         dirAngleHead        = 0.f;
    float         dirAngleHeadVar     = 0.f;
    float         dirAngleElev        = 0.f;
    float         dirAngleElevVar     = 0.f;

    float         velAvg              = 0.f;
    float         velVar              = 0.f;
    float         lspPartAvg          = 0.f;
    float         lspPartVar          = 0.f;

    Tempest::Vec3 flyGravity;
    bool          flyCollDet = false;

    Material      visMaterial;
    Orientation   visOrientation        = Orientation::None;
    bool          visTexIsQuadPoly      = true;
    float         visTexAniFPS          = 0.f;
    bool          visTexAniIsLooping    = false;
    Tempest::Vec3 visTexColorStart;
    Tempest::Vec3 visTexColorEnd;
    Tempest::Vec2 visSizeStart;
    float         visSizeEndScale      = 0.f;
    float         visAlphaStart        = 0.f;
    float         visAlphaEnd          = 0.f;
    bool          visYawAlign          = false;
    bool          visZBias             = false;

    const Tempest::Texture2d* trlTexture = nullptr;
    float         trlFadeSpeed         = 0.f;
    float         trlWidth             = 0.f;

    const Tempest::Texture2d* mrkTexture = nullptr;
    float         mrkFadeSpeed         = 0.f;
    float         mrkSize              = 0.f;

    std::string   flockMode;
    float         flockStrength = 0;

    bool          useEmittersFOR = true;

    std::string   timeStartEnd_S;
    bool          m_bIsAmbientPFX=false;

    uint64_t      prefferedTime = 0;

    bool          isDecal() const;
    uint64_t      maxLifetime() const;
    uint64_t      effectPrefferedTime() const;
    float         maxPps() const;
    float         shpScale(uint64_t time) const;
    float         ppsScale(uint64_t time) const;

  private:
    uint64_t             calcPrefferedTimeSingle() const;
    static uint64_t      calcPrefferedTimeSingle(const KeyList& k, float fps);

    static auto          loadTexture(std::string_view src) -> const Tempest::Texture2d*;
    static KeyList       loadArr(std::string_view src);
    static EmitterType   loadEmitType(std::string_view src);
    static Frame         loadFrameType(std::string_view src);
    static Distribution  loadDistribType(std::string_view src);
    static Dir           loadDirType(std::string_view src);
    static Orientation   loadOrientation(std::string_view src);

    float                fetchScaleKey(uint64_t time, const KeyList& k, float fps, bool smooth, bool loop) const;
  };

