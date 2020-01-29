#pragma once

#include "world/focus.h"
#include "inventory.h"
#include "utils/keycodec.h"

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

    void onKeyPressed (KeyCodec::Action a);
    void onKeyReleased(KeyCodec::Action a);
    void onRotateMouse(int dAngle);

    void changeZoom(int delta);
    void tickFocus(Focus& focus);

    bool interact(Interactive& it);
    bool interact(Npc&         other);
    bool interact(Item&        item);

    void clearInput();
    bool isInMove();

    void setTarget(Npc* other);
    void actionFocus(Npc& other);
    void emptyFocus();

    bool tickMove(uint64_t dt);
    auto  weaponState() const -> WeaponState;

  private:
    enum WeponAction : uint8_t {
      WeaponClose,
      WeaponMele,
      WeaponBow,
      Weapon3,
      Weapon4,
      Weapon5,
      Weapon6,
      Weapon7,
      Weapon8,
      Weapon9,
      Weapon10,

      Last,
      };

    enum FocusAction : uint8_t {
      ActForward=0,
      ActBack   =1,
      ActLeft   =2,
      ActRight  =3,
      ActGeneric=4,
      };

    using Action=KeyCodec::Action;

    bool           ctrl[Action::Last]={};
    bool           wctrl[WeponAction::Last]={};
    bool           actrl[5]={};

    bool           cacheFocus=false;
    float          rotMouse=0;
    int32_t        mobsiState=-1;

    Gothic&        gothic;
    DialogMenu&    dlg;
    InventoryMenu& inv;

    void           marvinF8();
    void           toogleWalkMode();
    Focus          findFocus(Focus *prev);

    World*         world() const;
    void           clrDraw();
    void           implMove(uint64_t dt);
    void           setPos(std::array<float,3> a, uint64_t dt, float speed);
  };
