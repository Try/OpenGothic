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

    void reset();
    void reset(const Npc* pl);

    void save(Serialize &s);
    void load(Serialize &s,Npc* pl);

    void changeZoom(int delta);
    void setViewport(uint32_t w, uint32_t h);

    void rotateLeft ();
    void rotateRight();

    void moveForward();
    void moveBack();
    void moveLeft();
    void moveRight();

    void setMode(Mode m);
    void setToogleEnable(bool e);
    bool isToogleEnabled() const;

    void setFirstPerson(bool fp);
    bool isFirstPerson() const;

    void setLookBack(bool lb);

    void toogleDebug();

    void tick(uint64_t dt);
    void debugDraw(DbgPainter& p);

    Tempest::PointF spin()     const;
    Tempest::PointF destSpin() const;

    void            setSpin(const Tempest::PointF& p);
    void            setDestSpin(const Tempest::PointF& p);

    void            setPosition(const Tempest::Vec3& pos);
    void            setDestPosition(const Tempest::Vec3& pos);

    void            setDialogDistance(float d);

    void            onRotateMouse(const Tempest::PointF& dpos);

    Tempest::Matrix4x4 projective() const;
    Tempest::Matrix4x4 view() const;
    Tempest::Matrix4x4 viewProj() const;
    Tempest::Matrix4x4 viewShadow(const Tempest::Vec3& ldir, size_t layer) const;

    float              zNear() const;
    float              zFar()  const;

  private:
    struct State {
      float               range  = 3.f;
      Tempest::Vec3       spin   = {};
      Tempest::Vec3       target = {};
      };

    Tempest::Vec3         cameraPos = {};
    Tempest::Vec3         origin    = {};
    Tempest::Vec3         rotOffset = {};
    Tempest::Vec3         offsetAng = {};
    State                 src, dst;

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

    mutable int           raysCasted = 0;

    static float          maxDist;
    static float          baseSpeeed;
    static float          offsetAngleMul;

    void                  calcControlPoints(float dtF);

    Tempest::Vec3         calcOffsetAngles(const Tempest::Vec3& srcOrigin, const Tempest::Vec3& target) const;
    Tempest::Vec3         calcOffsetAngles(Tempest::Vec3 srcOrigin, Tempest::Vec3 dstOrigin, Tempest::Vec3 target) const;
    float                 calcCameraColision(const Tempest::Vec3& target, const Tempest::Vec3& origin, const Tempest::Vec3& rotSpin, float dist) const;

    void                  implMove(Tempest::KeyEvent::KeyType t);
    Tempest::Matrix4x4    mkView    (const Tempest::Vec3& pos, const Tempest::Vec3& spin) const;
    Tempest::Matrix4x4    mkRotation(const Tempest::Vec3& spin) const;
    void                  resetDst();

    void                  clampRotation(Tempest::Vec3& spin);

    void                  followCamera(Tempest::Vec3& pos,  Tempest::Vec3 dest, float dtF);
    void                  followPos   (Tempest::Vec3& pos,  Tempest::Vec3 dest, float dtF);
    void                  followAng   (Tempest::Vec3& spin, Tempest::Vec3 dest, float dtF);
    static void           followAng   (float& ang, float dest, float speed, float dtF);

    const phoenix::c_camera& cameraDef() const;
  };
