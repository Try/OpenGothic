#pragma once

#include <vector>
#include <memory>

#include <daedalus/DaedalusGameState.h>

#include "game/perceptionmsg.h"

#include "interactive.h"
#include "movetrigger.h"
#include "spaceindex.h"
#include "trigger.h"
#include "zonetrigger.h"

class Npc;
class Item;
class World;
class Serialize;

class WorldObjects final {
  public:
    WorldObjects(World &owner);
    ~WorldObjects();

    enum SearchFlg : uint8_t {
      NoFlg  =0,
      NoDeath=1,
      NoAngle=2,
      NoRay  =4
      };

    struct SearchOpt final {
      float         rangeMin    = 0;
      float         rangeMax    = 0;
      float         azi         = 0;
      TargetCollect collectAlgo = TARGET_COLLECT_CASTER;
      SearchFlg     flags       = NoFlg;
      };

    void           load(Serialize& fout);
    void           save(Serialize& fout);
    void           tick(uint64_t dt);

    uint32_t       npcId(const void* ptr) const;
    uint32_t       itmId(const void* ptr) const;

    Npc*           addNpc(size_t itemInstance, const char *at);
    Npc *insertPlayer(std::unique_ptr<Npc>&& npc, const char *waypoint);
    auto           takeNpc(const Npc* npc) -> std::unique_ptr<Npc>;

    void           updateAnimation();

    Npc*           findHero();
    size_t         npcCount()    const { return npcArr.size(); }
    const Npc&     npc(size_t i) const { return *npcArr[i];    }
    Npc&           npc(size_t i)       { return *npcArr[i];    }
    void           detectNpc(const float x,const float y,const float z,std::function<void(Npc&)> f);

    size_t         itmCount()    const { return itemArr.size(); }
    Item&          itm(size_t i)       { return *itemArr[i];    }

    void           addTrigger(ZenLoad::zCVobData&& vob);
    Trigger*       findTrigger(const char* name);
    Trigger*       findTrigger(const std::array<float,3>& xyz);
    Trigger*       findTrigger(float x,float y,float z);

    Item*          addItem(size_t itemInstance, const char *at);
    Item*          addItem(const ZenLoad::zCVobData &vob);
    Item*          takeItem(Item& it);
    void           removeItem(Item& it);
    size_t         hasItems(const std::string& tag,size_t itemCls);
    void           addInteractive(const ZenLoad::zCVobData &vob);

    Interactive*   validateInteractive(Interactive *def);
    Npc*           validateNpc        (Npc         *def);
    Item*          validateItem       (Item        *def);

    Interactive*   findInteractive(const Npc& pl, Interactive *def, const Tempest::Matrix4x4 &v, int w, int h, const SearchOpt& opt);
    Npc*           findNpc        (const Npc& pl, Npc* def, const Tempest::Matrix4x4 &v, int w, int h, const SearchOpt& opt);
    Item*          findItem       (const Npc& pl, Item* def, const Tempest::Matrix4x4 &v, int w, int h, const SearchOpt& opt);

    void           marchInteractives(Tempest::Painter &p, const Tempest::Matrix4x4 &mvp, int w, int h) const;

    Interactive*   aviableMob(const Npc& pl,const std::string& name);
    bool           aiUseMob  (Npc &pl, const std::string& name);

    void           sendPassivePerc(Npc& self,Npc& other,Npc& victum,int32_t perc);
    void           resetPositionToTA();

  private:
    World&                             owner;
    SpaceIndex<Interactive>            interactiveObj;
    SpaceIndex<std::unique_ptr<Item>>  itemArr;

    std::vector<std::unique_ptr<Npc>>  npcArr;
    std::vector<Npc*>                  npcNear;

    std::vector<Trigger>               triggers;
    std::vector<MoveTrigger>           triggersMv;
    std::vector<ZoneTrigger>           triggersZn;

    std::vector<PerceptionMsg>         sndPerc;

    template<class T,class E>
    E*   validateObj(T &src,E* e);

    template<class T>
    auto findObj(T &src,const Npc &pl, const Tempest::Matrix4x4 &mvp,
                 int w, int h,const SearchOpt& opt) -> typename std::remove_reference<decltype(src[0])>::type*;
    template<class T>
    bool testObj(T &src,const Npc &pl, const Tempest::Matrix4x4 &mvp,
                 int w, int h,const SearchOpt& opt,float& rlen);
    template<class T>
    bool testObjRange(T &src,const Npc &pl, const SearchOpt& opt);
    template<class T>
    bool testObjRange(T &src,const Npc &pl, const SearchOpt& opt,float& rlen);

    void           tickNear(uint64_t dt);
  };
