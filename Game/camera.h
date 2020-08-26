#pragma once

#include <Tempest/Event>
#include <Tempest/Matrix4x4>
#include <Tempest/Point>
#include <array>
#include <daedalus/DaedalusStdlib.h>

class World;
class Npc;
class Gothic;
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
      Death
      };

    void reset();
    void save(Serialize &s);
    void load(Serialize &s,Npc* pl);

    void changeZoom(int delta);

    void rotateLeft ();
    void rotateRight();

    void moveForward();
    void moveBack();
    void moveLeft();
    void moveRight();

    void setMode(Mode m);
    void follow(const Npc& npc, uint64_t dt, bool inMove, bool includeRot);

    Tempest::PointF spin()     const;
    Tempest::PointF destSpin() const;

    void            setSpin(const Tempest::PointF& p);
    void            setDestSpin(const Tempest::PointF& p);

    void            setPosition(float x,float y,float z);
    void            setDestPosition(float x,float y,float z);

    void            setDialogDistance(float d);

    void            onRotateMouse(const Tempest::PointF& dpos);

    Tempest::Matrix4x4 view() const;
    Tempest::Matrix4x4 viewShadow(const Tempest::Vec3& ldir, int layer) const;

  private:
    struct State {
      Tempest::Vec3 pos={};
      Tempest::Vec3 spin={};
      };

    Gothic&               gothic;
    State                 state, dest;
    float                 zoom=0.3f;
    float                 dlgDist = 0;
    float                 camDistLast = 0.f;

    bool                  isInMove = false;
    bool                  hasPos   = false;
    Mode                  camMod   = Normal;

    void               applyModPosition(Tempest::Vec3& pos);
    void               applyModRotation(Tempest::Vec3& spin);

    void               implReset(const Npc& pl);
    void               implMove(Tempest::KeyEvent::KeyType t);
    Tempest::Matrix4x4 mkView(float dist) const;

    const Daedalus::GEngineClasses::CCamSys& cameraDef() const;
    void clampZoom(float& z);
  };
