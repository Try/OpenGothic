#pragma once

#include "world/focus.h"
#include "inventory.h"

#include <array>

class DialogMenu;
class InventoryMenu;
class World;
class Interactive;
class Npc;
class Item;
class Camera;
class Gothic;

class PlayerControl final {
  public:
    PlayerControl(Gothic& gothic,DialogMenu& dlg,InventoryMenu& inv);

    void changeZoom(int delta);

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
    void rotateMouse(int dAngle);

    void moveForward();
    void moveBack();
    void moveLeft();
    void moveRight();

    void setTarget(Npc* other);
    void actionFocus(Npc& other);
    void emptyFocus();
    void actionForward();
    void actionLeft();
    void actionRight();
    void actionBack();

    void marvinF8();

    bool tickMove(uint64_t dt);

    Focus findFocus(Focus *prev, const Camera &camera, int w, int h);
    auto  weaponState() const -> WeaponState;

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

      EmptyFocus,
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
    bool           cacheFocus=false;
    uint64_t       rotMouse=0;
    bool           rotMouseDir=false;
    int32_t        mobsiState=-1;

    Gothic&        gothic;
    DialogMenu&    dlg;
    InventoryMenu& inv;

    World*         world() const;
    void           clrDraw();
    void           implMove(uint64_t dt);
    void           setPos(std::array<float,3> a, uint64_t dt, float speed);
    void           invokeMobsiState();
  };
