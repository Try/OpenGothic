#pragma once

#include <Tempest/Event>
#include <Tempest/Matrix4x4>
#include <Tempest/Point>

#include <array>

#include <zenkit/addon/daedalus.hh>

#include "arpgdialogcam.h"

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
      float         nearCut    = 0;    // camera inside the rock (behind a ravine wall): the inner side of the world mesh
                                       // closer than this to the camera is not drawn (cm, 0 = off)
      // line of sight around the hero (indoors): how far the hero sees in each direction (cm),
      // direction i = angle 2*pi*i/LosRays in the xz plane, same convention as atan2(z,x)
      static constexpr size_t LosRays = 128;
      std::array<float,LosRays> los = {};
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
    // gothic-diablo: equipment / character window: the camera goes round to the front of the hero, close to him
    void               setArpgPortrait(bool on);
    const ArpgView&    arpgView() const { return arpgV; }
    // gothic-diablo: creatures that glow a little (enemy under the cursor, current target): xyz = center, w = radius
    void               arpgSetHighlight(size_t i, const Tempest::Vec4& v) { if(i<arpgHi.size()) arpgHi[i] = v; }
    const std::array<Tempest::Vec4,2>& arpgHighlight() const { return arpgHi; }
    // gothic-diablo: trade / chest / looting in the ARPG book: the camera shows the other one (or the thing) in the
    // middle between the two pages, the hero stays at the side behind a page (every frame, before tick)
    struct ArpgBookShot {
      bool          on       = false;
      Tempest::Vec3 subject  = {};    // the point in the middle of the picture
      bool          standing = true;  // a trader (from the front, low) or a chest / body (from above)
      };
    void               setArpgBook(const ArpgBookShot& b) { arpgBook.in = b; }
    // gothic-diablo: cinematic dialogs: the dialog menu tells the camera who talks (every frame, before tick)
    void               setArpgDialog(const ArpgDialogCam::Input& in) { arpgDlg.in = in; arpgDlg.on = true; }
    bool               isArpgDialogShot() const { return camMod==Dialog && arpgDlg.cutDone; }
    // depth of field of the dialog shots: strength 0..1, distance in focus (cm)
    struct ArpgDof { float strength = 0; float focus = 0; };
    ArpgDof            arpgDof() const { return ArpgDof{arpgDlg.dof, arpgDlg.focus}; }
    // a soft light for the faces in dialog shots (false: none)
    bool               arpgDialogLight(Tempest::Vec3& pos, float& range) const;

  private:
    struct Arpg {
      bool  init        = false;
      float yaw         = 0;      // world yaw, same convention as Npc::rotation()
      float yawDest     = 0;
      float elevation   = 55.f;   // degrees above horizon
      static constexpr float DefaultRange = 1100.f; // cm; the wheel: +-10%
      float range       = DefaultRange;
      float rangeDest   = DefaultRange;
      bool  indoor      = false;
      float indoorHold  = 0;      // seconds left before 'indoor' may switch off
      float indoorBlend = 0;      // 0..1, smooth version of 'indoor'
      float seeThrough  = 0;      // 0..1, smooth "hero is hidden" factor
      std::array<float,ArpgView::LosRays> los    = {}; // smoothed
      std::array<float,ArpgView::LosRays> losDst = {}; // last measured
      uint32_t losPhase = 0;
      bool     losInit  = false;
      float    insideHold = 0;   // s left: the camera was inside the rock
      float    nearCut    = 0;
      bool     portraitOn = false;
      float    portrait   = 0;   // 0..1 blend to the portrait view
      float    yawSaved   = 0;   // yaw before the portrait view
      };
    Arpg                  arpg;
    ArpgView              arpgV;
    Tempest::Vec3         arpgLookNow = {}; // where the ARPG / dialog camera looks (blends to and from the book shot)

    struct ArpgBook {
      ArpgBookShot  in;
      float         k      = 0;   // 0..1 blend to the book shot
      float         side   = 0;   // +1 / -1: picked when the book opens
      Tempest::Vec3 origin = {};
      Tempest::Vec3 look   = {};
      float         focus  = 0;
      };
    ArpgBook              arpgBook;
    void                  tickArpgBook(float dtF);
    std::array<Tempest::Vec4,2> arpgHi = {};

    void                  tickArpg(float dtF);
    void                  updateArpgView(float dtF);

    // gothic-diablo: cinematic dialogs: a short way down from the ARPG view, then the cut to the director's shots
    struct ArpgDlg {
      bool                 on       = false; // input given for this frame
      ArpgDialogCam::Input in;
      float                t        = 0;     // seconds in the talk
      bool                 cutDone  = false;
      bool                 keepView = false; // the ARPG view (cut-away, fog of war) stays: on the way down, workbench menus
      Tempest::Vec3        viewFrom = {};
      Tempest::Vec3        look     = {};    // where the shot looks
      float                focus    = 0;     // cm from the camera to the face in focus
      float                dof      = 0;     // 0..1, depth of field fades in and out
      bool                 exitOn   = false; // after the talk: the camera goes back up to the ARPG view
      float                exitT    = 0;
      Tempest::Vec3        exitFrom = {};
      Tempest::Vec3        exitLook = {};
      };
    ArpgDlg               arpgDlg;
    ArpgDialogCam         arpgDir;
    float                 fovNow = 0;
    void                  tickArpgDialog(float dtF);
    void                  arpgPlaceHero (const ArpgDialogCam::Input& in);

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
