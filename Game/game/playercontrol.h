#pragma once

#include <array>

class World;
class Interactive;

class PlayerControl final {
  public:
    PlayerControl();

    void changeZoom(int delta);

    bool interact(Interactive &it);
    void drawFist();
    void drawWeapon1H();
    void drawWeapon2H();
    void drawWeaponBow();
    void drawWeaponCBow();
    void drawWeaponMage();
    void action();
    void jump();

    void rotateLeft ();
    void rotateRight();

    void moveForward();
    void moveBack();
    void moveLeft();
    void moveRight();

    void marvinF8();

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
      Jump,

      DrawFist,
      DrawWeapon1h,
      DrawWeapon2h,
      DrawWeaponBow,
      DrawWeaponCBow,
      DrawWeaponMage,

      Action,

      Last
      };

    bool         ctrl[Control::Last]={};
    const World* world=nullptr;
    float        pSpeed=0;
    float        fallSpeed=0;
    float        mulSpeed=1.f;

    void         implMove(uint64_t dt);
    void         setPos(std::array<float,3> a, uint64_t dt, float speed);
  };
