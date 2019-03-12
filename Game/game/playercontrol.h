#pragma once

#include "inventory.h"

#include <array>

class DialogMenu;
class InventoryMenu;
class World;
class Interactive;
class Npc;
class Item;

class PlayerControl final {
  public:
    PlayerControl(DialogMenu& dlg,InventoryMenu& inv);

    void changeZoom(int delta);
    void setWorld(World* w);

    bool interact(Interactive& it);
    bool interact(Npc&         other);
    bool interact(Item&        item);

    void toogleWalkMode();

    void clearInput();
    void drawWeaponMele();
    void drawWeaponBow();
    void drawWeaponMage(uint8_t s);
    void jump();

    void rotateLeft ();
    void rotateRight();

    void moveForward();
    void moveBack();
    void moveLeft();
    void moveRight();

    void actionFocus(Npc& other);
    void actionForward();
    void actionLeft();
    void actionRight();
    void actionBack();

    void marvinF8();

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

      ActionFocus,
      ActForward,
      ActLeft,
      ActRight,
      ActBack,

      Walk,

      CloseWeapon,
      DrawWeaponMele,
      DrawWeaponBow,

      DrawWeaponMage3,
      DrawWeaponMage4,
      DrawWeaponMage5,
      DrawWeaponMage6,
      DrawWeaponMage7,
      DrawWeaponMage8,
      DrawWeaponMage9,
      DrawWeaponMage10,

      Last
      };

    bool           ctrl[Control::Last]={};
    World*         world=nullptr;
    DialogMenu&    dlg;
    InventoryMenu& inv;

    void           clrDraw();
    auto           weaponState() const -> WeaponState;
    void           implMove(uint64_t dt);
    void           setPos(std::array<float,3> a, uint64_t dt, float speed);
  };
