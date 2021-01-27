#pragma once

#include <Tempest/Event>
#include <Tempest/Matrix4x4>
#include <Tempest/Point>
#include <array>
#include <daedalus/DaedalusStdlib.h>

class World;
class Npc;
class Gothic;
class DbgPainter;
class Serialize;

class Camera final {
  public:
    Camera(Gothic& gothic);

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

    void reset(World& world);
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

    void toogleDebug();

    void tick(const Npc& npc, uint64_t dt, bool inMove, bool includeRot);
    void debugDraw(DbgPainter& p);

    Tempest::PointF spin()     const;
    Tempest::PointF destSpin() const;

    void            setSpin(const Tempest::PointF& p);
    void            setDestSpin(const Tempest::PointF& p);

    void            setPosition(float x,float y,float z);
    void            setDestPosition(float x,float y,float z);

    void            setDialogDistance(float d);

    void            onRotateMouse(const Tempest::PointF& dpos);

    Tempest::Matrix4x4 projective() const;
    Tempest::Matrix4x4 view() const;
    Tempest::Matrix4x4 viewProj() const;
    Tempest::Matrix4x4 viewShadow(const Tempest::Vec3& ldir, int layer) const;

  private:
    struct State {
      Tempest::Vec3 pos   = {};
      Tempest::Vec3 spin  = {};
      Tempest::Vec3 spin2 = {};
      float         range = 0.3f;
      };

    Gothic&               gothic;
    State                 state, dest;
    float                 dlgDist = 0;

    Tempest::Matrix4x4    proj;
    uint32_t              vpWidth=0;
    uint32_t              vpHeight=0;

    bool                  isInMove = false;
    bool                  hasPos   = false;
    bool                  dbg      = false;
    bool                  tgEnable = true;
    Mode                  camMod   = Normal;

    mutable int           raysCasted = 0;

    Tempest::Vec3         applyModPosition(const Tempest::Vec3& pos);
    Tempest::Vec3         applyModRotation(const Tempest::Vec3& spin);

    Tempest::Vec3         calcTranslation(float dist) const;

    void                  implReset(const Npc& pl);
    void                  implMove(Tempest::KeyEvent::KeyType t);
    Tempest::Matrix4x4    mkView    (const Tempest::Vec3& pos, float dist) const;
    Tempest::Matrix4x4    mkRotation(const Tempest::Vec3& spin) const;
    void                  clampRange(float& z);

    void                  followPos(Tempest::Vec3& pos, Tempest::Vec3 dest, bool inMove, float dtF);
    void                  followAng(Tempest::Vec3& spin, Tempest::Vec3 dest, float dtF);
    static void           followAng(float& ang,float dest,float speed);

    float                 calcCameraColision(const Tempest::Matrix4x4& view, const float dist) const;

    const Daedalus::GEngineClasses::CCamSys& cameraDef() const;
  };
