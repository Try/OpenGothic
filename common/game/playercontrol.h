#pragma once

#include "world/focus.h"
#include "game/arpgnav.h"
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
    void  arpgSwapWeapon();                      // melee <-> bow
    void  arpgBlock(bool on);                    // middle mouse button held: parry
    void  arpgChargeStart();                     // right mouse button down: start charging the special attack
    void  arpgChargeRelease();                   // right mouse button up: release it
    float arpgSpecialLeft() const;               // 0..1 of the cooldown left
    void  arpgPush();                            // middle mouse button with a bow: push the enemies in front away
    void  arpgStop();                            // a window opened: drop every mouse order / charge / request
    bool  arpgCanPush() const;                   // a bow / crossbow is drawn
    float arpgPushLeft() const;                  // 0..1 of the push cooldown left
    static constexpr float SpecialCooldown = 5.f; // s
    static constexpr float PushCooldown    = 5.f; // s
    // skill bar: what the mouse buttons and the potion keys do now
    struct ArpgBar {
      const Item* weapon     = nullptr;  // left / right mouse button (nullptr = fists)
      bool        ranged     = false;    // bow / crossbow: middle button pushes, otherwise it blocks
      const Item* potion[2]  = {};       // what keys 1 / 2 drink now
      int32_t     potions[2] = {};       // how many potions of that kind
      bool        charging   = false;    // right mouse button held
      };
    ArpgBar arpgBar() const;
    // charged attack: -1 = not charging, otherwise seconds charged
    float arpgChargeTime() const { return arpgCharge==ChargeHold ? arpgChargeT : -1.f; }
    static constexpr float ChargeFull   = 2.0f;  // s to fully charge (without the skill tree)
    static constexpr float ChargeWindow = 0.2f;  // s after that to release for the full hit
    // gothic-diablo: the same with the skill trees (Skupienie, Szersze okno, Opanowanie)
    float arpgChargeTimeFull() const;
    float arpgChargeTimeWindow() const;
    float arpgSpecialCooldown() const;
    // slash effect of the charged attack
    struct ArpgSlash {
      uint64_t      time = 0;     // world tick of the release
      Tempest::Vec3 pos  = {};
      float         ang  = 0;     // hero facing, degrees
      float         dir  = 1;     // sweep direction
      uint8_t       kind = 0;     // 0 = melee crescent, 1 = arrow streak, 2 = push wave
      };
    const ArpgSlash& arpgSlash() const { return arpgSlashFx; }
    bool  arpgCanBlock() const;                  // a melee weapon (or fists) is drawn
    void  arpgVisibleObjects(std::vector<Focus>& out) const; // items and usable objects the hero can see
    float arpgCeiling() const;                   // indoors: everything above is cut away
    bool  arpgIsEnemy(const Npc& npc) const;     // would be attacked by a click
    bool  arpgCursorOnEnemy() const;             // Diathic: a living enemy under the mouse cursor (red sword cursor)
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
        Strike,   // Shift + click: attack in place towards the cursor
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
      int                        clearCount  = 0;      // path planning: checks in a row with a clear straight line
      int                        blockCount  = 0;      // path planning: checks in a row with a blocked straight line
      Tempest::Vec3              pathDest    = {};     // destination the current path was planned for
      bool                       gridPath    = false;  // Diathic: the path is from ArpgNav (around furniture)
      bool                       replan      = false;  // Diathic: stuck - plan again from here
      bool                       gridFailed  = false;  // Diathic: no way on the grid to this destination (not again)
      int                        swings      = 0;
      bool                       swingCheck  = false;  // a swing was started: does its animation really play?
      float                      actTimer    = 0;      // s, waiting in range for the action to succeed
      };
    enum ArpgStep : uint8_t {
      ArpgStop,
      ArpgTurn,
      ArpgMove,
      ArpgAttack, // a swing plays: no other animation over it (a swing from a run goes on as Gothic's run attack)
      };

    ArpgCursor     arpgCursor;
    ArpgClick      arpgClick;
    Focus          arpgHover;
    Tempest::Vec3  arpgGround      = {};
    bool           arpgGroundValid = false;

    Focus          arpgPick();
    ArpgStep       arpgClickTick(Npc& pl, float& rot, int& rotation, bool allowRot, uint64_t dt);
    Tempest::Vec3  arpgNextPoint(Npc& pl, const Tempest::Vec3& dest, float reach, uint64_t dt);
    bool           arpgLineClear(const Tempest::Vec3& from, const Tempest::Vec3& to) const;  // a shot (a ray at 90 cm)
    bool           arpgWalkClear(const Tempest::Vec3& from, const Tempest::Vec3& to);        // a walk (ArpgNav)
    bool           arpgGridPath (Npc& pl, const Tempest::Vec3& dest, float reach);
    ArpgNav        arpgNav;
    bool           arpgStartDodge(Npc& pl, float& rot);
    void           arpgPlanPath(Npc& pl, const Tempest::Vec3& dest);
    void           arpgPullPath(const Tempest::Vec3& pos);
    bool           arpgSwing(Npc& pl, float& rot, bool allowRot, float faceAng, float faceDa);
    float          arpgSteer(Npc& pl, const Tempest::Vec3& next, uint64_t dt);
    bool           arpgCanSee(const Tempest::Vec3& p, float baseY, const Interactive* self) const;
    bool           arpgFeelerClear(const Npc& pl, float ang, float len) const;
    float          arpgFreeDistance(const Npc& pl, float ang) const;

    bool           arpgBlocking  = false; // middle mouse button held with a melee weapon
    enum ArpgCharge : uint8_t {
      ChargeNone,
      ChargeWant,   // button down, waiting to start the swing (drawing the weapon, end of a swing)
      ChargeHold,   // swing frozen in its wind-up / bow drawn, charging
      ChargeFire,   // bow: released, waiting for the shot to go off
      };
    ArpgCharge     arpgCharge       = ChargeNone;
    bool           arpgChargeHeld   = false;
    float          arpgChargeT      = 0;
    float          arpgChargeWait   = 0;
    float          arpgChargeSide   = 1;
    bool           arpgChargeFull   = false;
    bool           arpgChargeLate   = false; // released late, with Pewna ręka: x2
    float          arpgSpecialCd    = 0;  // s left of the special attack cooldown (not a world tick: survives loading)
    float          arpgPushCd       = 0;  // s left of the bow push cooldown
    bool           arpgPushReq      = false;
    float          arpgPushWait     = 0;
    uint8_t        arpgPotionReq    = 0;  // potion key pressed: 1 = health, 2 = mana
    float          arpgPotionWait   = 0;  // s, how long the request is kept
    uint8_t        arpgRedraw       = 0;  // WeaponAction to draw again after drinking (0 = none)
    uint8_t        arpgSpellSlot    = 0;  // spell key pressed: Gothic's rune/scroll slot 3..10 (0 = none)
    bool           arpgSpellHeld    = false;
    float          arpgSpellWait    = 0;  // s, how long the request is kept
    float          arpgSpellTurn    = 0;  // s spent turning to the cursor before the cast
    bool           arpgSpellCasting = false; // investing a spell while its key is held
    void           arpgSpellKey(uint8_t slot, bool pressed);
    bool           arpgCursorAim(const Npc& pl, Tempest::Vec3& out) const;
    Npc*           arpgNextEnemy(Npc& pl, const Npc* except, float range) const;
    int            arpgStartRoll(Npc& pl, float& rot);
    float          arpgRollCd       = 0;  // s until the next dodge roll
    void           arpgPotion(uint8_t kind);
    const Item*    arpgPickPotion(const Npc& pl, uint8_t kind, int32_t& total) const;
    ArpgSlash      arpgSlashFx;
    void           arpgChargeCancel(Npc& pl);
    bool           arpgShoot(Npc& pl, float& rot, bool allowRot, float faceAng, Npc* tg);
    void           arpgDrawPreferred(Npc& pl);   // the last used weapon: bow (if any arrows) or melee
    bool           arpgHasRanged(Npc& pl) const;
    float          arpgShotRange() const;
    float          arpgFightFace(Npc& pl, float def);
    bool           arpgDodgeReq  = false; // dodge requested, waiting for the hero to be able to do it
    float          arpgDodgeWait = 0;     // s, how long the request is kept
    float          arpgDodgeLock = 0;     // s, dodge animation in progress: no steering
    // gothic-diablo: the roll with the skill tree Fikołek
    float          arpgRollTotal  = 0;     // s, the lock of this roll
    Tempest::Vec3  arpgRollDir    = {};    // where it goes (flat, normalized)
    float          arpgRollExtra  = 0;     // cm more than the animation (Dalszy przewrót)
    bool           arpgRollNoNpc  = false; // through enemies (Przez szereg): collision with npcs off
    bool           arpgSecondRoll = true;  // Drugi oddech: a roll right out of the last one is ready
    float          arpgSecondT    = 0;     // s until it is ready again
    std::vector<const Npc*> arpgRollTripped; // Podcięcie: thrown down by this roll
    void           arpgTickRoll(Npc& pl, float dtF);
    void           arpgEndRoll(Npc& pl);

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
