#pragma once

#include <cstdint>

#include <daedalus/DaedalusStdlib.h>

class Npc;
class GameScript;

class FightAlgo final {
  public:
    FightAlgo();

    enum Action : uint8_t {
      MV_NULL     = 0,
      MV_MOVE     = 1,
      MV_JUMPBACK = 2,
      MV_ATACK    = 3,
      MV_ATACKL   = 4,
      MV_ATACKR   = 5,
      MV_STRAFEL  = 6,
      MV_STRAFER  = 7,
      MV_BLOCK    = 8,

      MV_MAX      = 6
      };

    Action tick(Npc& npc, Npc& tg, GameScript &owner, uint64_t dt);
    void   consumeAction();
    void   consumeAndWait(float dt);
    void   onClearTarget();
    void   onTakeHit();

    float  prefferedAtackDistance(const Npc &npc, const Npc &tg, GameScript &owner) const;
    bool   isInAtackRange        (const Npc &npc, const Npc &tg, GameScript &owner);
    bool   isInGRange            (const Npc &npc, const Npc &tg, GameScript &owner);

  private:
    void   fillQueue(Npc &npc, Npc &tg, GameScript& owner);
    void   fillQueue(GameScript& owner,const Daedalus::GEngineClasses::C_FightAI& src);
    Action nextFromQueue(GameScript& owner);

    static float  gRange         (GameScript &owner,const Npc &npc);
    static float  weaponRange    (GameScript &owner,const Npc &npc);
    static float  weaponOnlyRange(GameScript &owner,const Npc &npc);

    uint16_t                       waitT=0;
    Daedalus::GEngineClasses::Move queueId=Daedalus::GEngineClasses::Move(0);
    Action                         tr   [MV_MAX]={};
    bool                           hitFlg=false;
  };
