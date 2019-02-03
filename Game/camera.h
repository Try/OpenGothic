#pragma once

#include <Tempest/Event>
#include <Tempest/Matrix4x4>
#include <Tempest/Point>
#include <array>

class World;

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

    void setSpin(const Tempest::PointF& p);

    Tempest::Matrix4x4 view() const;

  private:
    std::array<float,3>   camPos={};
    Tempest::PointF       spin;
    float                 zoom=1.f;

    const World*          world=nullptr;

    void implMove(Tempest::KeyEvent::KeyType t);
    void implMovePl(Tempest::KeyEvent::KeyType t);
  };
