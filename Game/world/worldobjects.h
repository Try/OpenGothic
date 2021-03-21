#pragma once

#include <vector>
#include <memory>

#include <daedalus/DaedalusGameState.h>

#include "bullet.h"
#include "spaceindex.h"
#include "game/gametime.h"
#include "game/perceptionmsg.h"
#include "game/constants.h"

class Npc;
class Item;
class Vob;
class StaticObj;
class Interactive;
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
    void           tick(uint64_t dt, uint64_t dtPlayer);

    Npc*           addNpc(size_t itemInstance, const Daedalus::ZString& at);
    Npc*           addNpc(size_t itemInstance, const Tempest::Vec3&     at);
    Npc*           insertPlayer(std::unique_ptr<Npc>&& npc, const Daedalus::ZString& waypoint);
    auto           takeNpc(const Npc* npc) -> std::unique_ptr<Npc>;

    void           updateAnimation();

    bool           isTargeted(Npc& npc);
    Npc*           findHero();
    Npc*           findNpcByInstance(size_t instance);
    void           detectNpcNear(const std::function<void(Npc&)>& f);
    void           detectNpc (const float x, const float y, const float z, const float r, const std::function<void(Npc&)>&  f);
    void           detectItem(const float x, const float y, const float z, const float r, const std::function<void(Item&)>& f);

    uint32_t       npcId(const Npc *ptr) const;
    size_t         npcCount()    const { return npcArr.size(); }
    const Npc&     npc(size_t i) const { return *npcArr[i];    }
    Npc&           npc(size_t i)       { return *npcArr[i];    }

    size_t         itmCount()    const { return itemArr.size(); }
    Item&          itm(size_t i)       { return *itemArr[i];    }
    uint32_t       itmId(const void* ptr) const;

    size_t         mobsiCount()    const { return interactiveObj.size();        }
    Interactive&   mobsi(size_t i)       { return **(interactiveObj.begin()+i); }
    uint32_t       mobsiId(const void* ptr) const;

    void           addTrigger(AbstractTrigger* trigger);
    void           triggerEvent(const TriggerEvent& e);
    void           execTriggerEvent(const TriggerEvent& e);
    void           triggerOnStart(bool firstTime);
    void           enableTicks (AbstractTrigger& t);
    void           disableTicks(AbstractTrigger& t);

    void           runEffect(Effect&& e);
    void           stopEffect(const VisualFx& vfx);

    Item*          addItem(size_t itemInstance, const char *at);
    Item*          addItem(const ZenLoad::zCVobData &vob);
    Item*          addItem(size_t itemInstance, const Tempest::Vec3& pos);
    Item*          addItem(size_t itemInstance, const Tempest::Vec3& pos, const Tempest::Vec3& dir);
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

    void           marchInteractives(DbgPainter& p) const;

    Interactive*   aviableMob(const Npc& pl, const char* name);
    void           setMobRoutine(gtime time, const Daedalus::ZString& scheme, int32_t state);

    void           sendPassivePerc(Npc& self,Npc& other,Npc& victum,int32_t perc);
    void           sendPassivePerc(Npc& self,Npc& other,Npc& victum,Item& itm,int32_t perc);
    void           resetPositionToTA();

  private:
    struct MobRoutine {
      gtime   time;
      int32_t state = 0;
      };

    struct MobStates {
      Daedalus::ZString       scheme;
      std::vector<MobRoutine> routines;
      int32_t                 curState = 0;
      int32_t                 stateByTime(gtime t) const;
      void                    save(Serialize& fout);
      void                    load(Serialize& fin);
      };

    struct EffectState {
      Effect   eff;
      uint64_t timeUntil = 0;
      };

    World&                             owner;
    std::vector<std::unique_ptr<Vob>>  rootVobs;

    SpaceIndex<Interactive>            interactiveObj;
    SpaceIndex<Item>                   items;

    std::vector<StaticObj*>            objStatic;
    std::vector<std::unique_ptr<Item>> itemArr;
    std::list<MobStates>               routines;

    std::list<Bullet>                  bullets;
    std::vector<EffectState>           effects;

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

    void             setMobState(const char* scheme, int32_t st);

    void             tickNear(uint64_t dt);
    void             tickTriggers(uint64_t dt);
    static bool      isTargetedBy(Npc& npc,Npc& by);
  };
