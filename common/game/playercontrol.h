#pragma once

#include "world/focus.h"
#include "utils/keycodec.h"
#include "constants.h"

#include <array>
#include <vector>

#include <Tempest/Matrix4x4>

class DialogMenu;
class InventoryMenu;
class DbgPainter;
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

    void  onKeyPressed (KeyCodec::Action a, Tempest::Event::KeyType key, KeyCodec::Mapping mapping);
    void  onKeyReleased(KeyCodec::Action a, KeyCodec::Mapping mapping);
    bool  isPressed(KeyCodec::Action a) const;
    void  onRotateMouse(float dAngleX, float dAngleY);

    void  drawVobRay(DbgPainter& p) const;

    void  changeZoom(int delta);
    void  tickFocus();
    void  clearFocus();

    bool  interact(Interactive& it);
    bool  interact(Npc&         other);
    bool  interact(Item&        item);

    void  clearInput();

    void  setTarget(Npc* other);
    void  actionFocus(Npc& other);
    void  emptyFocus();

    Focus focus() const;
    bool  hasActionFocus() const;

    bool  tickMove(uint64_t dt);
    bool  tickCameraMove(uint64_t dt);

    // gothic-diablo: mouse cursor in ARPG mode (Diablo-like click to move / interact / attack)
    struct ArpgCursor {
      bool               valid   = false;
      Tempest::Vec3      rayFrom = {};   // world space point on the near plane under the cursor
      Tempest::Vec3      rayTo   = {};   // world space point on the far plane under the cursor
      Tempest::Matrix4x4 viewProj;
      Tempest::Vec2      pixel   = {};   // cursor position, window pixels
      Tempest::Vec2      screen  = {};   // window size, pixels
      };
    void  setArpgCursor(const ArpgCursor& c);
    void  arpgMouseDown();
    void  arpgMouseDown(const Focus& f);         // click on a label (show-all key held)
    void  arpgMouseUp();
    void  arpgDodge();                           // side step / jump back
    void  arpgVisibleObjects(std::vector<Focus>& out) const; // items and usable objects the hero can see
    float arpgCeiling() const;                   // indoors: everything above is cut away
    bool  arpgIsEnemy(const Npc& npc) const;     // would be attacked by a click
    bool  arpgAttackQueued() const;              // an attack waits for its moment (combo window)

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
        /// gothic-diablo: both directions held -> the one pressed last wins
        /// (also covers a key-up that got lost while a menu had the keyboard)
        auto value() const -> float {
          const bool m = this->anyMain(), r = this->anyReverse();
          if(m && r)
            return lastPressed;
          return (m ? 1.f : 0.f) + (r ? -1.f : 0.f);
          }

        float lastPressed = 0.f;

        /// True if only one of directions is active
        /// (e.g. false if none or both directions are active).
        auto any() const -> bool {
          return this->value() != 0;
          }

        void reset() {
          this->main.fill(false);
          this->reverse.fill(false);
          lastPressed = 0.f;
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
    uint64_t       turnAniSmooth  = 0;
    int            rotationAni    = 0;
    bool           g2Ctrl         = false;

    // gothic-diablo: click-to-move / interact / attack
    struct ArpgClick {
      enum Kind : uint8_t {
        None,
        Ground,
        PickUp,
        Use,
        Talk,
        Loot,
        Attack,
        };
      Kind                       kind        = None;
      bool                       held        = false;  // left mouse button still down
      Focus                      target;
      Tempest::Vec3              point       = {};
      std::vector<Tempest::Vec3> path;
      size_t                     pathIdx     = 0;
      float                      repathTimer = 0;
      bool                       useWaynet   = false;
      Tempest::Vec3              stuckPos    = {};
      float                      stuckTimer  = 0;
      int                        stuckCount  = 0;
      int                        avoidSide   = 0;      // +1 / -1: side we walk around obstacles on
      float                      detourAng   = 0;      // direction along the obstacle we bumped into
      float                      detourTime  = 0;
      uint64_t                   lastFail    = 0;      // last handled MoveAlgo::MoveFail::time
      int                        swings      = 0;
      float                      actTimer    = 0;      // s, waiting in range for the action to succeed
      };
    enum ArpgStep : uint8_t {
      ArpgStop,
      ArpgTurn,
      ArpgMove,
      };

    ArpgCursor     arpgCursor;
    ArpgClick      arpgClick;
    Focus          arpgHover;
    Tempest::Vec3  arpgGround      = {};
    bool           arpgGroundValid = false;

    Focus          arpgPick();
    ArpgStep       arpgClickTick(Npc& pl, float& rot, int& rotation, bool allowRot, uint64_t dt);
    Tempest::Vec3  arpgNextPoint(Npc& pl, const Tempest::Vec3& dest, uint64_t dt);
    bool           arpgLineClear(const Tempest::Vec3& from, const Tempest::Vec3& to) const;
    bool           arpgStartDodge(Npc& pl, float& rot);
    float          arpgSteer(Npc& pl, const Tempest::Vec3& next, uint64_t dt);
    bool           arpgCanSee(const Tempest::Vec3& p, float baseY, const Interactive* self) const;
    bool           arpgFeelerClear(const Npc& pl, float ang, float len) const;
    float          arpgFreeDistance(const Npc& pl, float ang) const;

    bool           arpgDodgeReq  = false; // dodge requested, waiting for the hero to be able to do it
    float          arpgDodgeWait = 0;     // s, how long the request is kept
    float          arpgDodgeLock = 0;     // s, dodge animation in progress: no steering

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
    Focus          findFocus(const Focus* prev) const;

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
