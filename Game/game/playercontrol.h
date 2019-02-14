#pragma once

#include <array>

class DialogMenu;
class World;
class Interactive;
class Npc;

class PlayerControl final {
  public:
    PlayerControl(DialogMenu& dlg);

    void changeZoom(int delta);

    bool interact(Interactive& it);
    bool interact(Npc&         other);

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
    DialogMenu&  dlg;

    void         implMove(uint64_t dt);
    void         setPos(std::array<float,3> a, uint64_t dt, float speed);
  };
