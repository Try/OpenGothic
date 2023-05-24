#pragma once

#include <Tempest/Event>
#include <Tempest/Matrix4x4>
#include <Tempest/Point>
#include <array>

#include <phoenix/ext/daedalus_classes.hh>

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
      Dive
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

    void reset();
    void reset(const Npc* pl);

    void save(Serialize &s);
    void load(Serialize &s,Npc* pl);

    void changeZoom(int delta);
    void setViewport(uint32_t w, uint32_t h);

    void rotateLeft(uint64_t dt);
    void rotateRight(uint64_t dt);

    void moveForward(uint64_t dt);
    void moveBack(uint64_t dt);
    void moveLeft(uint64_t dt);
    void moveRight(uint64_t dt);

    void setMode(Mode m);
    void setMarvinMode(MarvinMode m);
    bool isMarvin() const;
    bool isFree() const;
    bool isInWater() const;

    void setToggleEnable(bool e);
    bool isToggleEnabled() const;

    void setFirstPerson(bool fp);
    bool isFirstPerson() const;

    void setLookBack(bool lb);

    void toggleDebug();

    void tick(uint64_t dt);
    void debugDraw(DbgPainter& p);

    Tempest::PointF    spin()     const;
    Tempest::PointF    destSpin() const;

    void               setSpin(const Tempest::PointF& p);
    void               setDestSpin(const Tempest::PointF& p);

    void               setPosition(const Tempest::Vec3& pos);
    void               setDestPosition(const Tempest::Vec3& pos);

    void               setDialogDistance(float d);

    void               onRotateMouse(const Tempest::PointF& dpos);

    Tempest::Matrix4x4 projective() const;
    Tempest::Matrix4x4 view() const;
    Tempest::Matrix4x4 viewProj() const;
    Tempest::Matrix4x4 viewShadow(const Tempest::Vec3& ldir, size_t layer) const;

    Tempest::Matrix4x4 viewLwc() const;
    Tempest::Matrix4x4 viewProjLwc() const;
    Tempest::Matrix4x4 viewShadowLwc(const Tempest::Vec3& ldir, size_t layer) const;

    ListenerPos        listenerPosition() const;

    float              zNear() const;
    float              zFar()  const;

  private:
    struct State {
      float               range  = 3.f;
      Tempest::Vec3       spin   = {};
      Tempest::Vec3       target = {};
      };

    struct Pin {
      Tempest::Vec3       origin = {};
      Tempest::Vec3       spin   = {};
      };

    Tempest::Vec3         cameraPos       = {};
    Tempest::Vec3         origin          = {};
    Tempest::Vec3         rotOffset       = {};
    Tempest::Vec3         offsetAng       = {};
    State                 src, dst;

    Pin                   pin;

    float                 dlgDist   = 0;
    float                 userRange = 0.13f;

    Tempest::Matrix4x4    proj;
    uint32_t              vpWidth=0;
    uint32_t              vpHeight=0;

    bool                  dbg           = false;
    bool                  tgEnable      = true;
    bool                  fpEnable      = false;
    bool                  lbEnable      = false;
    bool                  inertiaTarget = true;
    Mode                  camMod        = Normal;
    MarvinMode            camMarvinMod  = M_Normal;
    bool                  inWater       = false;

    mutable int           raysCasted = 0;

    static float          maxDist;
    static float          baseSpeeed;
    static float          offsetAngleMul;
    static const float    minLength;

    void                  calcControlPoints(float dtF);

    Tempest::Vec3         calcOffsetAngles(const Tempest::Vec3& srcOrigin, const Tempest::Vec3& target) const;
    Tempest::Vec3         calcOffsetAngles(Tempest::Vec3 srcOrigin, Tempest::Vec3 dstOrigin, Tempest::Vec3 target) const;
    float                 calcCameraColision(const Tempest::Vec3& target, const Tempest::Vec3& origin, const Tempest::Vec3& rotSpin, float dist) const;

    void                  implMove(Tempest::KeyEvent::KeyType t, uint64_t dt);
    Tempest::Matrix4x4    mkView    (const Tempest::Vec3& pos, const Tempest::Vec3& spin) const;
    Tempest::Matrix4x4    mkRotation(const Tempest::Vec3& spin) const;
    Tempest::Matrix4x4    mkViewShadow(const Tempest::Vec3& cameraPos, float rotation,
                                       const Tempest::Matrix4x4& viewProj, const Tempest::Vec3& lightDir, size_t layer) const;
    void                  resetDst();

    void                  clampRotation(Tempest::Vec3& spin);

    void                  followCamera(Tempest::Vec3& pos,  Tempest::Vec3 dest, float dtF);
    void                  followPos   (Tempest::Vec3& pos,  Tempest::Vec3 dest, float dtF);
    Tempest::Vec3         clampPos    (Tempest::Vec3  pos,  Tempest::Vec3 dest);
    void                  followAng   (Tempest::Vec3& spin, Tempest::Vec3 dest, float dtF);
    static void           followAng   (float& ang, float dest, float speed, float dtF);

    const phoenix::c_camera& cameraDef() const;
  };
