#pragma once

#include <daedalus/DaedalusStdlib.h>
#include <Tempest/RenderState>

#include <Tempest/Texture2d>

class ParticleFx final {
  public:
    ParticleFx(const Daedalus::GEngineClasses::C_ParticleFX & src);

    enum class EmitterType:uint8_t {
      Point,
      Line,
      Box,
      Circle,
      Sphere,
      Mesh
      };

    enum class Dir:uint8_t {
      None,
      Dir,
      Target,
      Mesh
      };

    enum class AlphaFunc:uint8_t {
      None,
      Blend,
      Add,
      Mul
      };

    float         ppsValue=0;
    std::string   ppsScaleKeys_S;
    bool          ppsIsLooping=0;
    bool          ppsIsSmooth=0;
    float         ppsFPS=0;
    std::string   ppsCreateEm_S;
    float         ppsCreateEmDelay=0;

    EmitterType   shpType_S=EmitterType::Point;
    std::string   shpFOR_S;
    std::string   shpOffsetVec_S;
    std::string   shpDistribType_S;
    float         shpDistribWalkSpeed=0.f;
    bool          shpIsVolume=false;
    Tempest::Vec3 shpDim_S;
    std::string   shpMesh_S;
    bool          shpMeshRender_B=false;
    std::string   shpScaleKeys_S;
    bool          shpScaleIsLooping=false;
    bool          shpScaleIsSmooth=false;
    float         shpScaleFPS=0.f;

    Dir           dirMode_S=Dir::None;
    std::string   dirFOR_S;
    std::string   dirModeTargetFOR_S;
    std::string   dirModeTargetPos_S;
    float         dirAngleHead=0.f;
    float         dirAngleHeadVar=0.f;
    float         dirAngleElev=0.f;
    float         dirAngleElevVar=0.f;

    float         velAvg=0.f;
    float         velVar=0.f;
    float         lspPartAvg=0.f;
    float         lspPartVar=0.f;

    std::string   flyGravity_S;
    bool          flyCollDet_B=false;

    const Tempest::Texture2d* visName_S=nullptr;
    std::string   visOrientation_S;
    bool          visTexIsQuadPoly=true;
    float         visTexAniFPS=0.f;
    bool          visTexAniIsLooping=false;
    Tempest::Vec3 visTexColorStart_S;
    Tempest::Vec3 visTexColorEnd_S;
    Tempest::Vec2 visSizeStart_S;
    float         visSizeEndScale=0.f;
    AlphaFunc     visAlphaFunc_S=AlphaFunc::None;
    float         visAlphaStart=0.f;
    float         visAlphaEnd=0.f;

    float         trlFadeSpeed=0.f;
    std::string   trlTexture_S;
    float         trlWidth=0.f;

    float         mrkFadeSpeed=0.f;
    std::string   mrkTexture_S;
    float         mrkSize=0.f;

    std::string   flockMode;
    float         flockStrength;

    bool          useEmittersFOR=true;

    std::string   timeStartEnd_S;
    bool          m_bIsAmbientPFX=false;

    uint64_t      maxLifetime() const;

  private:
    const Daedalus::GEngineClasses::C_ParticleFX* src=nullptr;

    static auto          loadTexture(const std::string& src) -> const Tempest::Texture2d*;
    static Tempest::Vec2 loadVec2(const std::string& src);
    static Tempest::Vec3 loadVec3(const std::string& src);
    static EmitterType   loadEmitType(const std::string& src);
    static Dir           loadDirType(const std::string& src);
    static AlphaFunc     loadAlphaFn(const std::string& src);
  };

