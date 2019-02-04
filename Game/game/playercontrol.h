#pragma once

#include <array>

class World;

class PlayerControl final {
  public:
    PlayerControl();

    void changeZoom(int delta);

    void rotateLeft ();
    void rotateRight();

    void moveForward();
    void moveBack();
    void moveLeft();
    void moveRight();

    void setWorld(const World* w);
    bool tickMove(uint64_t dt);

    float speed() const { return pSpeed; }

  private:
    enum Control : uint8_t {
      Idle,
      Forward,
      Back,
      Left,
      Right,
      RotateL,
      RotateR,

      Last
      };

    void         implMove(uint64_t dt);

    bool         ctrl[Control::Last]={};
    const World* world=nullptr;
    float        pSpeed=0;
    float        mulSpeed=1.f;
  };
