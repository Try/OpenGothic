#pragma once

#include <Tempest/Event>
#include <Tempest/Matrix4x4>
#include <Tempest/Point>
#include <array>

class World;
class Npc;

class Camera final {
  public:
    Camera();

    void setWorld(const World* w);

    void changeZoom(int delta);
    Tempest::PointF getSpin() const { return spin; }

    void rotateLeft ();
    void rotateRight();

    void moveForward();
    void moveBack();
    void moveLeft();
    void moveRight();

    void follow(const Npc& npc);

    void setSpin(const Tempest::PointF& p);

    Tempest::Matrix4x4 view() const;

  private:
    std::array<float,3>   camPos={};
    Tempest::PointF       spin;
    float                 zoom=1.f;

    const World*          world=nullptr;
    bool                  hasPos=false;

    void implMove(Tempest::KeyEvent::KeyType t);
    Tempest::Matrix4x4 mkView(float dist) const;
  };
