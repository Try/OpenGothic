#pragma once

#include <Tempest/Event>
#include <Tempest/Matrix4x4>
#include <Tempest/Point>

#include <zenkit/addon/daedalus.hh>

class World;
class Npc;
class DbgPainter;
class Serialize;

class Camera final {
  public:
    Camera();

    constexpr static const float minShadowY = -0.025f;

    enum Mode {
      Dialog,
      Normal,
      Inventory,
      Melee,
      Ranged,
      Magic,
      Mobsi,
      Death,
      Swim,
      Dive,
      Fall,
      Cutscene,
      FirstPerson,
      };

    enum MarvinMode {
      M_Normal,
      M_Freeze,
      M_Free,
      M_Pinned,
      };

    struct ListenerPos {
      Tempest::Vec3 up;
      Tempest::Vec3 front;
      Tempest::Vec3 pos;
      };

    // gothic-diablo: data for hiding geometry around the player in ARPG view
    struct ArpgView {
      bool          enabled    = false;
      Tempest::Vec3 feet       = {};   // player feet, world space (cm)
      Tempest::Vec3 center     = {};   // player body center, world space (cm)
      float         cutHeight  = 0;    // cut everything above feet+cutHeight (0 = off)
      float         cutRadius  = 0;    // horizontal radius of the cut (cm)
      float         fadeRadius = 0;    // see-through circle radius (NDC, vertical units)
      float         fowRadius  = 0;    // fog of war: world fades to black beyond this distance from the hero (cm)
      float         fowFade    = 0;    // fog of war: width of the fade (cm)
      float         seeThrough = 0;    // 0..1: something hides the hero -> see-through circle (animated)
      float         indoor     = 0;    // 0..1: indoors -> lower levels fade to black (animated)
      float         lowerStart = 0;    // lower levels: start of the fade, cm below the feet
      float         lowerEnd   = 0;    // lower levels: fully black, cm below the feet
      };

    void reset();
    void reset(const Npc* pl);

    void save(Serialize &s);
    void load(Serialize &s, Npc* pl);

    void changeZoom(int delta);
    void setViewport(uint32_t w, uint32_t h);

    void rotateLeft(uint64_t dt);
    void rotateRight(uint64_t dt);

    void moveForward(uint64_t dt);
    void moveBack(uint64_t dt);
    void moveLeft(uint64_t dt);
    void moveRight(uint64_t dt);

    void setMode(const Mode m);
    void setMarvinMode(MarvinMode m);
    bool isMarvin() const;
    bool isFree() const;
    bool isInWater() const;
    bool isCutscene() const;

    void setToggleEnable(bool e);
    bool isToggleEnabled() const;

    void setInertiaTargetEnable(bool e);
    bool isInertiaTargetEnabled() const;

    void setFirstPerson(bool fp);
    bool isFirstPerson() const;

    void setLookBack(bool lb);

    void toggleDebug();

    void tick(uint64_t dt);
    void debugDraw(DbgPainter& p);

    Tempest::PointF    spin() const;
    Tempest::Vec3      destTarget() const;
    float              azimuth() const;

    void               setSpin(const Tempest::PointF& p);
    void               setTarget(const Tempest::Vec3& pos);

    void               setAngles(const Tempest::PointF& pos);
    void               setPosition(const Tempest::Vec3& pos);
    void               setDialogDistance(float d);

    void               onRotateMouse(const Tempest::PointF& dpos);

    Tempest::Matrix4x4 projective() const;
    Tempest::Matrix4x4 view() const;
    Tempest::Matrix4x4 viewProj() const;
    Tempest::Matrix4x4 viewShadow(const Tempest::Vec3& ldir, size_t layer) const;

    Tempest::Matrix4x4 viewLwc() const;
    Tempest::Matrix4x4 viewProjLwc() const;
    Tempest::Matrix4x4 viewShadowLwc(const Tempest::Vec3& ldir, size_t layer) const;

    Tempest::Matrix4x4 viewShadowVsm(const Tempest::Vec3& ldir) const;
    Tempest::Matrix4x4 viewShadowVsmLwc(const Tempest::Vec3& ldir) const;

    ListenerPos        listenerPosition() const;
    Tempest::Vec3      originLwc() const { return origin+shake; };

    float              zNear() const;
    float              zFar()  const;

    // gothic-diablo: isometric ARPG camera
    static bool        isArpgEnabled();
    bool               isArpgActive() const;
    float              arpgYaw() const { return arpg.yaw; }
    void               arpgRotate(float deltaDeg);
    const ArpgView&    arpgView() const { return arpgV; }

  private:
    struct Arpg {
      bool  init        = false;
      float yaw         = 0;      // world yaw, same convention as Npc::rotation()
      float yawDest     = 0;
      float elevation   = 55.f;   // degrees above horizon
      float range       = 1100.f; // cm
      float rangeDest   = 1100.f;
      bool  indoor      = false;
      float indoorHold  = 0;      // seconds left before 'indoor' may switch off
      float indoorBlend = 0;      // 0..1, smooth version of 'indoor'
      float seeThrough  = 0;      // 0..1, smooth "hero is hidden" factor
      };
    Arpg                  arpg;
    ArpgView              arpgV;

    void                  tickArpg(float dtF);
    void                  updateArpgView(float dtF);

    struct Pin {
      Tempest::Vec3       origin = {};
      Tempest::Vec3       spin   = {};
      };

    struct State {
      float               range  = 3.f;
      Tempest::Vec3       spin   = {};
      Tempest::Vec3       target = {};
      };

    struct Interpolated {
      Tempest::Vec3       target    = {};
      Tempest::Vec3       rotOffset = {};
      };

    State                 state;
    Interpolated          inter;
    Pin                   pin;

    Tempest::Vec3         origin    = {};
    Tempest::Vec3         angles    = {};
    Tempest::Vec3         veloTrans = {};
    float                 userRange  = 0;
    Tempest::Vec3         shake      = {};

    float                 targetVelo = 0;

    float                 dlgRange   = 0;

    Tempest::Matrix4x4    proj;
    uint32_t              vpWidth=0;
    uint32_t              vpHeight=0;
    float                 depthNear = 0;

    bool                  dbg           = false;
    bool                  tgEnable      = true;
    bool                  fpEnable      = false;
    bool                  lbEnable      = false;
    bool                  inertiaTarget = true;
    Mode                  camMod        = Normal;
    MarvinMode            camMarvinMod  = M_Normal;
    bool                  inWater       = false;

    mutable int           raysCasted = 0;

    static const float    minLength;

    void                  tickFirstPerson(float dtF);
    void                  tickThirdPerson(float dtF);

    Tempest::Vec3         clampRotation(Tempest::Vec3 spin);
    float                 calcCameraColision(const Tempest::Vec3& target, const Tempest::Vec3& dir, const Tempest::Vec3& rotSpin, float dist) const;
    Tempest::Vec3         calcCameraColision(const Tempest::Vec3& from, const Tempest::Vec3& dir) const;

    Tempest::Vec3         calcLookAtAngles(const Tempest::Vec3& origin, const Tempest::Vec3& target,
                                           const Tempest::Vec3& rotOffset, const Tempest::Vec3& defSpin) const;

    void                  implMove(Tempest::KeyEvent::KeyType t, uint64_t dt);

    Tempest::Matrix4x4    mkView    (const Tempest::Vec3& pos, const Tempest::Vec3& spin) const;
    Tempest::Matrix4x4    mkRotation(const Tempest::Vec3& spin) const;
    Tempest::Matrix4x4    mkViewShadow(const Tempest::Vec3& cameraPos, float rotation,
                                       const Tempest::Matrix4x4& viewProj, const Tempest::Vec3& lightDir, size_t layer) const;
    Tempest::Matrix4x4    mkViewShadowVsm(const Tempest::Vec3& cameraPos, const Tempest::Vec3& ldir) const;

    Tempest::Vec3         followTarget(Tempest::Vec3 pos,  Tempest::Vec3 dest, float dtF);
    Tempest::Vec3         followTrans (Tempest::Vec3 pos,  Tempest::Vec3 dest, float dtF, float velo);
    Tempest::Vec3         followRot   (Tempest::Vec3 spin, Tempest::Vec3 dest, float dtF, float velo);

    void                  followAng   (Tempest::Vec3& spin, Tempest::Vec3 dest, float dtF);
    static void           followAng   (float& ang, float dest, float speed, float dtF);

    const zenkit::ICamera& cameraDef() const;
  };
