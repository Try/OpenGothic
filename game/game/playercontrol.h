#pragma once

#include "world/focus.h"
#include "utils/keycodec.h"
#include "constants.h"

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
    PlayerControl(DialogMenu& dlg, InventoryMenu& inv);

    void onKeyPressed (KeyCodec::Action a, Tempest::Event::KeyType key);
    void onKeyReleased(KeyCodec::Action a);
    bool isPressed(KeyCodec::Action a) const;
    void onRotateMouse(float dAngle);
    void onRotateMouseDy(float dAngle);

    void changeZoom(int delta);
    void tickFocus();
    void clearFocus();

    bool interact(Interactive& it);
    bool interact(Npc&         other);
    bool interact(Item&        item);

    void clearInput();
    bool isInMove();

    void setTarget(Npc* other);
    void actionFocus(Npc& other);
    void emptyFocus();

    Focus focus() const;
    bool  hasActionFocus() const;

    bool  tickMove(uint64_t dt);
    auto  weaponState() const -> WeaponState;

  private:
    enum WeaponAction : uint8_t {
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
      ActMove   =5,
      };

    using Action=KeyCodec::Action;

    bool           ctrl[Action::Last]={};
    bool           wctrl[WeaponAction::Last]={};
    bool           actrl[6]={};

    WeaponAction   wctrlLast = WeaponAction::WeaponMele; //!< Reminder for weapon toggle.

    bool           cacheFocus=false;
    Focus          currentFocus;
    float          rotMouse=0;
    float          rotMouseY=0;
    bool           casting = false;

    float          runAngleDest   = 0.f;
    uint64_t       runAngleSmooth = 0;
    uint64_t       turnAniSmooth  = 0;
    int            rotationAni    = 0;

    DialogMenu&    dlg;
    InventoryMenu& inv;

    bool           canInteract() const;
    void           marvinF8(uint64_t dt);
    void           marvinK(uint64_t dt);
    void           toogleWalkMode();
    void           toggleSneakMode();
    Focus          findFocus(Focus *prev);

    void           clrDraw();
    void           implMove(uint64_t dt);
    void           implMoveMobsi(Npc& pl, uint64_t dt);
    void           setPos(std::array<float,3> a, uint64_t dt, float speed);
    void           assignRunAngle(Npc& pl, float rotation, uint64_t dt);
    void           setAnimRotate (Npc& pl, float rotation, int anim, bool force, uint64_t dt);
    void           processAutoRotate(Npc& pl, float& rot, uint64_t dt);
  };
