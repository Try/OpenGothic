#pragma once

#include <cstdint>

#include <phoenix/ext/daedalus_classes.hh>

class Npc;
class GameScript;
class Serialize;

class FightAlgo final {
  public:
    FightAlgo();

    void load(Serialize& fin);
    void save(Serialize& fout);

    enum Action : uint8_t {
      MV_NULL     = 0,
      MV_MOVEG    = 1,
      MV_MOVEA    = 2,
      MV_JUMPBACK = 3,
      MV_ATACK    = 4,
      MV_ATACKL   = 5,
      MV_ATACKR   = 6,
      MV_STRAFEL  = 7,
      MV_STRAFER  = 8,
      MV_BLOCK    = 9,
      MV_WAIT     = 10,
      MV_WAITLONG = 11,
      MV_TURN2HIT = 12,

      MV_MAX      = 6
      };

    Action nextFromQueue(Npc &npc, Npc &tg, GameScript& owner);
    void   consumeAction();
    void   onClearTarget();
    void   onTakeHit();

    bool   hasInstructions() const;
    bool   fetchInstructions(Npc &npc, Npc &tg, GameScript& owner);

    float  baseDistance          (const Npc &npc, const Npc &tg,  GameScript &owner) const;
    float  prefferedAtackDistance(const Npc &npc, const Npc &tg, GameScript &owner) const;
    float  prefferedGDistance    (const Npc &npc, const Npc &tg, GameScript &owner) const;

    bool   isInAtackRange        (const Npc &npc, const Npc &tg, GameScript &owner) const;
    bool   isInWRange            (const Npc &npc, const Npc &tg, GameScript &owner) const;
    bool   isInGRange            (const Npc &npc, const Npc &tg, GameScript &owner) const;
    bool   isInFocusAngle        (const Npc &npc, const Npc &tg) const;

  private:
    void   fillQueue(Npc &npc, Npc &tg, GameScript& owner);
    bool   fillQueue(GameScript& owner,const phoenix::c_fight_ai& src);

    static float  weaponRange(GameScript &owner,const Npc &npc);

    phoenix::c_fight_ai_move           queueId=phoenix::c_fight_ai_move::nop;
    Action                             tr   [MV_MAX]={};
    bool                               hitFlg=false;
  };
