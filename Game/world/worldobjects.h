#pragma once

#include <vector>
#include <memory>

#include <daedalus/DaedalusGameState.h>

#include "bullet.h"
#include "interactive.h"
#include "spaceindex.h"
#include "staticobj.h"
#include "game/perceptionmsg.h"

class Npc;
class Item;
class Vob;
class World;
class Serialize;
class TriggerEvent;
class AbstractTrigger;

class WorldObjects final {
  public:
    WorldObjects(World &owner);
    ~WorldObjects();

    enum SearchFlg : uint8_t {
      NoFlg     =0,
      NoDeath   =1,
      NoAngle   =2,
      NoRay     =4,
      FcOverride=8,
      };

    struct SearchOpt final {
      SearchOpt()=default;
      SearchOpt(float rangeMin, float rangeMax, float azi, TargetCollect collectAlgo=TARGET_COLLECT_CASTER, SearchFlg flags=NoFlg);
      float         rangeMin    = 0;
      float         rangeMax    = 0;
      float         azi         = 0;
      TargetCollect collectAlgo = TARGET_COLLECT_CASTER;
      SearchFlg     flags       = NoFlg;
      };

    void           load(Serialize& fout);
    void           save(Serialize& fout);
    void           tick(uint64_t dt);

    uint32_t       npcId(const Npc *ptr) const;
    uint32_t       itmId(const void* ptr) const;

    Npc*           addNpc(size_t itemInstance, const Daedalus::ZString& at);
    Npc*           addNpc(size_t itemInstance, const Tempest::Vec3&     at);
    Npc*           insertPlayer(std::unique_ptr<Npc>&& npc, const Daedalus::ZString& waypoint);
    auto           takeNpc(const Npc* npc) -> std::unique_ptr<Npc>;

    void           updateAnimation();

    bool           isTargeted(Npc& npc);
    Npc*           findHero();
    Npc*           findNpcByInstance(size_t instance);
    size_t         npcCount()    const { return npcArr.size(); }
    const Npc&     npc(size_t i) const { return *npcArr[i];    }
    Npc&           npc(size_t i)       { return *npcArr[i];    }
    void           detectNpcNear(std::function<void(Npc&)> f);
    void           detectNpc(const float x,const float y,const float z,const float r,std::function<void(Npc&)> f);

    size_t         itmCount()    const { return itemArr.size(); }
    Item&          itm(size_t i)       { return *itemArr[i];    }

    void           addTrigger(AbstractTrigger* trigger);
    void           triggerEvent(const TriggerEvent& e);
    void           triggerOnStart(bool wrldStartup);
    void           enableTicks (AbstractTrigger& t);
    void           disableTicks(AbstractTrigger& t);

    Item*          addItem(size_t itemInstance, const char *at);
    Item*          addItem(const ZenLoad::zCVobData &vob);
    Item*          takeItem(Item& it);
    void           removeItem(Item& it);
    size_t         hasItems(const char* tag, size_t itemCls);

    Bullet&        shootBullet(const Item &itmId, float x, float y, float z, float dx, float dy, float dz, float speed);

    void           addInteractive(Interactive*         obj);
    void           addStatic     (StaticObj*           obj);
    void           addRoot       (ZenLoad::zCVobData&& vob, bool startup);
    void           invalidateVobIndex();

    Interactive*   validateInteractive(Interactive *def);
    Npc*           validateNpc        (Npc         *def);
    Item*          validateItem       (Item        *def);

    Interactive*   findInteractive(const Npc& pl, Interactive *def, const SearchOpt& opt);
    Npc*           findNpc        (const Npc& pl, Npc* def, const SearchOpt& opt);
    Item*          findItem       (const Npc& pl, Item* def, const SearchOpt& opt);

    void           marchInteractives(Tempest::Painter &p, const Tempest::Matrix4x4 &mvp, int w, int h) const;

    Interactive*   aviableMob(const Npc& pl, const char* name);

    void           sendPassivePerc(Npc& self,Npc& other,Npc& victum,int32_t perc);
    void           sendPassivePerc(Npc& self,Npc& other,Npc& victum,Item& itm,int32_t perc);
    void           resetPositionToTA();

  private:
    World&                             owner;
    std::vector<std::unique_ptr<Vob>>  rootVobs;

    SpaceIndex<Interactive>            interactiveObj;
    SpaceIndex<Item>                   items;

    std::vector<StaticObj*>            objStatic;
    std::vector<std::unique_ptr<Item>> itemArr;

    std::list<Bullet>                  bullets;

    std::vector<std::unique_ptr<Npc>>  npcArr;
    std::vector<std::unique_ptr<Npc>>  npcInvalid;
    std::vector<Npc*>                  npcNear;

    std::vector<AbstractTrigger*>      triggers;
    std::vector<AbstractTrigger*>      triggersZn;
    std::vector<AbstractTrigger*>      triggersTk;

    std::vector<PerceptionMsg>         sndPerc;
    std::vector<TriggerEvent>          triggerEvents;

    template<class T>
    auto findObj(T &src, const Npc &pl, const SearchOpt& opt) -> typename std::remove_reference<decltype(src[0])>::type*;

    template<class T>
    bool testObj(T &src, const Npc &pl, const SearchOpt& opt);
    template<class T>
    bool testObj(T &src, const Npc &pl, const SearchOpt& opt, float& rlen);

    void           tickNear(uint64_t dt);
    void           tickTriggers(uint64_t dt);
    static bool    isTargetedBy(Npc& npc,Npc& by);
  };
