#pragma once

#include <vector>
#include <memory>

#include <zenkit/vobs/Misc.hh>

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
class CsCamera;
class CollisionZone;

class WorldObjects final {
  public:
    WorldObjects(World &owner);
    ~WorldObjects();

    enum SearchFlg : uint8_t {
      NoFlg         = 0,
      NoDeath       = 1,
      NoUnconscious = 2,
      NoAngle       = 4,
      NoRay         = 8,
      FcOverride    = 16,
      };

    struct SearchOpt final {
      SearchOpt()=default;
      SearchOpt(float rangeMin, float rangeMax, float azi, TargetCollect collectAlgo=TARGET_COLLECT_CASTER, TargetType collectType=TARGET_TYPE_ALL, SearchFlg flags=NoFlg);
      float         rangeMin    = 0;
      float         rangeMax    = 0;
      float         azi         = 0;
      TargetCollect collectAlgo = TARGET_COLLECT_CASTER;
      TargetType    collectType = TARGET_TYPE_ALL;
      SearchFlg     flags       = NoFlg;
      };

    void           load(Serialize& fout);
    void           save(Serialize& fout);
    void           tick(uint64_t dt, uint64_t dtPlayer);

    Npc*           addNpc(size_t itemInstance, std::string_view     at);
    Npc*           addNpc(size_t itemInstance, const Tempest::Vec3& at);
    Npc*           insertPlayer(std::unique_ptr<Npc>&& npc, std::string_view at);
    auto           takeNpc(const Npc* npc) -> std::unique_ptr<Npc>;
    void           removeNpc(Npc& npc);

    void           updateAnimation(uint64_t dt);

    bool           isTargeted(Npc& npc);
    Npc*           findHero();
    Npc*           findNpcByInstance(size_t instance, size_t n = 0);
    Item*          findItemByInstance(size_t instance, size_t n = 0);
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

    void           setCurrentCs(CsCamera* cs);
    CsCamera*      currentCs() const;

    void           addTrigger(AbstractTrigger* trigger);
    void           enableDefTrigger(AbstractTrigger& trigger);
    void           triggerEvent(const TriggerEvent& e);
    bool           triggerOnStart(bool firstTime);
    void           execDelayedEvents();
    bool           execTriggerEvent(const TriggerEvent& e);
    void           enableTicks (AbstractTrigger& t);
    void           disableTicks(AbstractTrigger& t);
    void           enableCollizionZone (CollisionZone& z);
    void           disableCollizionZone(CollisionZone& z);

    void           runEffect(Effect&& e);
    void           stopEffect(const VisualFx& vfx);

    Item*          addItem   (size_t itemInstance, std::string_view at);
    Item*          addItem   (const zenkit::VItem &vob);
    Item*          addItem   (size_t itemInstance, const Tempest::Vec3& pos);
    Item*          addItem   (size_t itemInstance, const Tempest::Vec3& pos, const Tempest::Vec3& dir);
    Item*          addItemDyn(size_t itemInstance, const Tempest::Matrix4x4& pos, size_t owner);
    auto           takeItem(Item& it) -> std::unique_ptr<Item>;
    void           removeItem(Item& it);
    size_t         hasItems(std::string_view tag, size_t itemCls);
    void           onItemRemoved(const Item& itm);

    Bullet&        shootBullet(const Item &itmId, const Tempest::Vec3& pos, const Tempest::Vec3& dir, float tgRange, float speed);

    void           addInteractive(Interactive*         obj);
    void           addStatic     (StaticObj*           obj);
    void           addRoot       (const std::shared_ptr<zenkit::VirtualObject>& vob, bool startup);
    void           invalidateVobIndex();

    Interactive*   validateInteractive(Interactive *def);
    Npc*           validateNpc        (Npc         *def);
    Item*          validateItem       (Item        *def);

    bool           testFocusNpc       (const Npc &pl, Npc *def, const SearchOpt& opt);

    Interactive*   findInteractive(const Npc& pl, Interactive *def, const SearchOpt& opt);
    Npc*           findNpcNear    (const Npc& pl, Npc* def, const SearchOpt& opt);
    Item*          findItem       (const Npc& pl, Item* def, const SearchOpt& opt);

    void           marchInteractives(DbgPainter& p) const;

    Interactive*   availableMob(const Npc& pl, std::string_view name);
    void           setMobRoutine(gtime time, std::string_view scheme, int32_t state);

    void           sendPassivePerc  (Npc& self, Npc& other, Npc* victum, Item* itm, int32_t perc);
    void           sendImmediatePerc(Npc& self, Npc& other, Npc& victum, Item* itm, int32_t perc);

    void           resetPositionToTA();

  private:
    struct MobRoutine {
      gtime   time;
      int32_t state = 0;
      };

    struct MobStates {
      std::string             scheme;
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

    std::vector<CollisionZone*>        collisionZn;
    std::vector<std::unique_ptr<Vob>>  rootVobs;

    SpaceIndex<Interactive>            interactiveObj;
    SpaceIndex<Item>                   items;

    std::vector<StaticObj*>            objStatic;
    std::vector<std::unique_ptr<Item>> itemArr;
    std::list<MobStates>               routines;

    std::list<Bullet>                  bullets;
    std::vector<EffectState>           effects;

    std::vector<std::unique_ptr<Npc>>  npcArr;
    std::vector<std::unique_ptr<Npc>>  npcInvalid; // dead or invalid TA
    std::vector<std::unique_ptr<Npc>>  npcRemoved; // removed, but may have a dangling references in game
    std::vector<Npc*>                  npcNear;

    std::vector<AbstractTrigger*>      triggers;
    std::vector<AbstractTrigger*>      triggersTk;
    std::vector<AbstractTrigger*>      triggersDef;
    std::vector<PerceptionMsg>         sndPerc;
    std::vector<TriggerEvent>          triggerEvents;
    CsCamera*                          currentCsCamera = nullptr;

    template<class T>
    auto findObj(T &src, const Npc &pl, const SearchOpt& opt) -> typename std::remove_reference<decltype(src[0])>::type;

    template<class T>
    bool testObj(T &src, const Npc &pl, const SearchOpt& opt);
    template<class T>
    bool testObj(T &src, const Npc &pl, const SearchOpt& opt, float& rlen);

    void             setMobState(std::string_view scheme, int32_t st);
    void             passivePerceptionProcess(PerceptionMsg& msg, Npc& npc, Npc& pl);

    void             tickNear(uint64_t dt);
    void             tickTriggers(uint64_t dt);
    static bool      isTargetedBy(Npc& npc,Npc& by);
  };
