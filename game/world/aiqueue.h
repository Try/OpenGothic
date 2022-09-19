#pragma once

#include <daedalus/ZString.h>
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
      Daedalus::ZString s0;
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

    static AiAction aiLookAt(Npc* other);
    static AiAction aiStopLookAt();
    static AiAction aiRemoveWeapon();
    static AiAction aiTurnToNpc(Npc *other);
    static AiAction aiGoToNpc  (Npc *other);
    static AiAction aiGoToNextFp(const Daedalus::ZString& fp);
    static AiAction aiStartState(ScriptFn stateFn, int behavior, Npc *other, Npc* victum, const Daedalus::ZString& wp);
    static AiAction aiPlayAnim(const Daedalus::ZString& ani);
    static AiAction aiPlayAnimBs(const Daedalus::ZString& ani, BodyState bs);
    static AiAction aiWait(uint64_t dt);
    static AiAction aiStandup();
    static AiAction aiStandupQuick();
    static AiAction aiGoToPoint(const WayPoint &to);
    static AiAction aiEquipArmor(int32_t id);
    static AiAction aiEquipBestArmor();
    static AiAction aiEquipBestMeleWeapon();
    static AiAction aiEquipBestRangeWeapon();
    static AiAction aiUseMob(const Daedalus::ZString& name,int st);
    static AiAction aiUseItem(int32_t id);
    static AiAction aiUseItemToState(int32_t id, int32_t state);
    static AiAction aiTeleport(const WayPoint& to);
    static AiAction aiDrawWeapon();
    static AiAction aiReadyMeleWeapon();
    static AiAction aiReadyRangeWeapon();
    static AiAction aiReadySpell(int32_t spell, int32_t mana);
    static AiAction aiAtack();
    static AiAction aiFlee();
    static AiAction aiDodge();
    static AiAction aiUnEquipWeapons();
    static AiAction aiUnEquipArmor();
    static AiAction aiProcessInfo(Npc& other);
    static AiAction aiOutput(Npc &to, const Daedalus::ZString& text, int order);
    static AiAction aiOutputSvm(Npc &to, const Daedalus::ZString& text, int order);
    static AiAction aiOutputSvmOverlay(Npc &to, const Daedalus::ZString& text, int order);
    static AiAction aiStopProcessInfo();
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
    static AiAction aiPrintScreen(int time, const Daedalus::ZString& font, int x,int y, const Daedalus::ZString& msg);

  private:
    std::deque<AiAction> aiActions;
  };

