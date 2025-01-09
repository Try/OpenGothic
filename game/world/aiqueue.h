#pragma once

#include <cstdint>
#include <deque>

#include "game/gamescript.h"
#include "game/constants.h"

class Npc;
class Item;
class WayPoint;
class Serialize;

class AiQueue {
  public:
    AiQueue();

    struct AiAction final {
      Action            act   =AI_None;
      Npc*              target=nullptr;
      Npc*              victum=nullptr;
      const WayPoint*   point =nullptr;
      Item*             item  =nullptr;
      ScriptFn          func  =0;
      int               i0    =0;
      int               i1    =0;
      std::string       s0;
      // Extended section, only for print-screen
      int               i2    =0;
      std::string       s1;
      };

    void     save(Serialize& fout) const;
    void     load(Serialize& fin);

    size_t   size() const { return aiActions.size(); }
    void     clear();
    void     pushBack (AiAction&& a);
    void     pushFront(AiAction&& a);
    AiAction pop();
    int      aiOutputOrderId() const;

    void     onWldItemRemoved(const Item& itm);

    static AiAction aiLookAt(const WayPoint* to);
    static AiAction aiLookAtNpc(Npc* other);
    static AiAction aiStopLookAt();
    static AiAction aiRemoveWeapon();
    static AiAction aiTurnToNpc(Npc *other);
    static AiAction aiWhirlToNpc(Npc *other);
    static AiAction aiGoToNpc  (Npc *other);
    static AiAction aiGoToNextFp(std::string_view fp);
    static AiAction aiStartState(ScriptFn stateFn, int behavior, Npc *other, Npc* victum, std::string_view wp);
    static AiAction aiPlayAnim(std::string_view ani);
    static AiAction aiPlayAnimBs(std::string_view ani, BodyState bs);
    static AiAction aiWait(uint64_t dt);
    static AiAction aiStandup();
    static AiAction aiStandupQuick();
    static AiAction aiGoToPoint(const WayPoint &to);
    static AiAction aiEquipArmor(int32_t id);
    static AiAction aiEquipBestArmor();
    static AiAction aiEquipBestMeleeWeapon();
    static AiAction aiEquipBestRangeWeapon();
    static AiAction aiUseMob(std::string_view name,int st);
    static AiAction aiUseItem(int32_t id);
    static AiAction aiUseItemToState(int32_t id, int32_t state);
    static AiAction aiTeleport(const WayPoint& to);
    static AiAction aiDrawWeapon();
    static AiAction aiReadyMeleeWeapon();
    static AiAction aiReadyRangeWeapon();
    static AiAction aiReadySpell(int32_t spell, int32_t mana);
    static AiAction aiAttack();
    static AiAction aiFlee();
    static AiAction aiDodge();
    static AiAction aiUnEquipWeapons();
    static AiAction aiUnEquipArmor();
    static AiAction aiProcessInfo(Npc& other);
    static AiAction aiOutput(Npc &to, std::string_view text, int order);
    static AiAction aiOutputSvm(Npc &to, std::string_view text, int order);
    static AiAction aiOutputSvmOverlay(Npc &to, std::string_view text, int order);
    static AiAction aiStopProcessInfo(int order);
    static AiAction aiContinueRoutine();
    static AiAction aiAlignToFp();
    static AiAction aiAlignToWp();
    static AiAction aiSetNpcsToState(ScriptFn func, int32_t radius);
    static AiAction aiSetWalkMode(WalkBit w);
    static AiAction aiFinishingMove(Npc& other);
    static AiAction aiTakeItem(Item& item);
    static AiAction aiGotoItem(Item& item);
    static AiAction aiPointAt(const WayPoint &to);
    static AiAction aiPointAtNpc(Npc& other);
    static AiAction aiStopPointAt();
    static AiAction aiPrintScreen(int time, std::string_view font, int x,int y, std::string_view msg);

  private:
    std::deque<AiAction> aiActions;
  };

