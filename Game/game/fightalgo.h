#pragma once

#include <cstdint>

#include <daedalus/DaedalusStdlib.h>

class Npc;
class WorldScript;

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

    Action tick(Npc& npc, Npc& tg, WorldScript &owner, uint64_t dt);
    void   consumeAction();
    void   consumeAndWait(float dt);
    void   onClearTarget();
    void   onTakeHit();

    float  prefferedAtackDistance(const Npc &npc, const Npc &tg, WorldScript &owner) const;
    bool   isInAtackRange        (const Npc &npc, const Npc &tg, WorldScript &owner);
    bool   isInGRange            (const Npc &npc, const Npc &tg, WorldScript &owner);

  private:
    void   fillQueue(Npc &npc, Npc &tg, WorldScript& owner);
    void   fillQueue(WorldScript& owner,const Daedalus::GEngineClasses::C_FightAI& src);
    Action nextFromQueue(WorldScript& owner);

    static float  gRange         (WorldScript &owner,const Npc &npc);
    static float  weaponRange    (WorldScript &owner,const Npc &npc);
    static float  weaponOnlyRange(WorldScript &owner,const Npc &npc);

    uint16_t                       waitT=0;
    Daedalus::GEngineClasses::Move queueId=Daedalus::GEngineClasses::Move(0);
    Action                         tr   [MV_MAX]={};
    bool                           hitFlg=false;
  };
