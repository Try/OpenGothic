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
    ~PlayerControl();

    void onKeyPressed (KeyCodec::Action a, Tempest::Event::KeyType key, KeyCodec::Mapping mapping = KeyCodec::Mapping::Primary);
    void onKeyReleased(KeyCodec::Action a, KeyCodec::Mapping mapping = KeyCodec::Mapping::Primary);
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
      ActKill   =6,
      };

    using Action=KeyCodec::Action;

    struct AxisStatus { 
        /// Main direction (e.g. W or Up arrow)
        std::array<bool, KeyCodec::NumMappings> main;
        
        /// Reverse direction (e.g. S or Down arrow)
        std::array<bool, KeyCodec::NumMappings> reverse;

        /// Current axis value (scale from -1 to 1)
        auto value() const -> float {
          return
              (this->anyMain() ? 1.f : 0.f)
            + (this->anyReverse() ? -1.f : 0.f);
          }

        /// True if only one of directions is active
        /// (e.g. false if none or both directions are active).
        auto any() const -> bool {
          return this->value() != 0;
          }

        void reset() {
          this->main.fill(false);
          this->reverse.fill(false);
          }

      private:
        /// Is any key pressed that activates the main direction
        /// (e.g. W or Up Arrow in Forward-Backward axis)
        auto anyMain() const -> bool {
          for(auto elem : main) {
            if(elem) return true;
            }
          return false;
          }
        
        /// Is any key pressed that activates the reverse direction
        /// (e.g. S or Down arrow in Forward-Backward axis)
        auto anyReverse() const -> bool {
          for(auto elem : reverse) {
            if(elem) return true;
            }
          return false;
          }
      };

    struct MovementStatus {
      AxisStatus forwardBackward;  
      AxisStatus strafeRightLeft;
      AxisStatus turnRightLeft;

      /// Resets all axes to their default state.
      void reset() {
        this->forwardBackward.reset();
        this->strafeRightLeft.reset();
        this->turnRightLeft.reset();
        }
      } movement;
    
    bool           ctrl[Action::Last]={};
    bool           wctrl[WeaponAction::Last]={};
    bool           actrl[7]={};

    WeaponAction   wctrlLast = WeaponAction::WeaponMele; //!< Reminder for weapon toggle.

    bool           cacheFocus=false;
    Focus          currentFocus;
    float          rotMouse=0;
    float          rotMouseY=0;
    bool           casting = false;
    size_t         pickLockProgress = 0;

    float          runAngleDest   = 0.f;
    uint64_t       runAngleSmooth = 0;
    uint64_t       turnAniSmooth  = 0;
    int            rotationAni    = 0;
    bool           g2Ctrl         = false;

    DialogMenu&    dlg;
    InventoryMenu& inv;

    void           setupSettings();
    bool           canInteract() const;
    void           marvinF8(uint64_t dt);
    void           marvinK(uint64_t dt);
    void           marvinO();
    void           toggleWalkMode();
    void           toggleSneakMode();
    void           moveFocus(FocusAction act);
    Focus          findFocus(Focus *prev);

    void           clrDraw();
    void           implMove(uint64_t dt);
    void           implMoveMobsi(Npc& pl, uint64_t dt);
    void           processPickLock(Npc& pl, Interactive& inter, KeyCodec::Action key);
    void           processLadder(Npc& pl, Interactive& inter, KeyCodec::Action key);
    void           quitPicklock(Npc& pl);
    void           setPos(std::array<float,3> a, uint64_t dt, float speed);
    void           assignRunAngle(Npc& pl, float rotation, uint64_t dt);
    void           setAnimRotate (Npc& pl, float rotation, int anim, bool force, uint64_t dt);
    void           processAutoRotate(Npc& pl, float& rot, uint64_t dt);


    //////////////////////////////////
    // Helper functions for movement
    //////////////////////////////////

    auto wantsToMoveForward() const -> bool {
      return movement.forwardBackward.value() > 0.f;
      }
    auto wantsToMoveBackward() const -> bool {
      return movement.forwardBackward.value() < 0.f;
      }

    auto wantsToStrafeRight() const -> bool {
      return movement.strafeRightLeft.value() > 0.f;
      }
    auto wantsToStrafeLeft() const -> bool {
      return movement.strafeRightLeft.value() < 0.f;
      }

    auto wantsToTurnRight() const -> bool {
      return movement.turnRightLeft.value() > 0.f;
      }
    auto wantsToTurnLeft() const -> bool {
      return movement.turnRightLeft.value() < 0.f;
      }

    /// @brief Analyses the input action mapping and updates the movement status accordingly.
    ///        Meant to be used when key is pressed or released.
    /// @param actionMapping - the pressed/released action
    /// @param pressed - true if the key was pressed, false if it was released
    auto handleMovementAction(KeyCodec::ActionMapping actionMapping, bool pressed) -> void;
  };
