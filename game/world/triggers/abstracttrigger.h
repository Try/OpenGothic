#pragma once

#include <string>
#include <zenload/zTypes.h>

#include "world/objects/vob.h"
#include "world/collisionzone.h"
#include "physics/dynamicworld.h"

class Npc;
class World;

class TriggerEvent final {
  public:
    enum Type : uint8_t {
      T_Trigger,
      T_Untrigger,
      T_Enable,
      T_Disable,
      T_ToogleEnable,
      T_Activate,
      T_StartupFirstTime,
      T_Startup,
      T_Move,
      };

    TriggerEvent()=default;
    TriggerEvent(std::string target, std::string emitter, Type type):target(std::move(target)), emitter(std::move(emitter)), type(type){}
    TriggerEvent(std::string target, std::string emitter, uint64_t t, Type type)
      :target(std::move(target)), emitter(std::move(emitter)),type(type),timeBarrier(t){}

    void              save(Serialize& fout) const;
    void              load(Serialize &fin);

    std::string       target;
    std::string       emitter;
    Type              type        = T_Trigger;
    uint64_t          timeBarrier = 0;
    struct {
      ZenLoad::MoverMessage msg = ZenLoad::MoverMessage::GOTO_KEY_FIXED_DIRECTLY;
      int32_t               key = 0;
      } move;
  };

class AbstractTrigger : public Vob {
  public:
    AbstractTrigger(Vob* parent, World& world, ZenLoad::zCVobData&& data, bool startup);
    virtual ~AbstractTrigger();

    ZenLoad::zCVobData::EVobType vobType() const;
    const std::string&           name() const;
    bool                         isEnabled() const;

    void                         processOnStart(const TriggerEvent& evt);
    void                         processEvent(const TriggerEvent& evt);
    virtual void                 onIntersect(Npc& n);
    virtual void                 tick(uint64_t dt);

    virtual bool                 hasVolume() const;
    bool                         checkPos(const Tempest::Vec3& pos) const;

    void                         save(Serialize& fout) const override;
    void                         load(Serialize &fin) override;

  protected:
    enum ReactFlg:uint8_t {
      ReactToOnTrigger = 1,
      ReactToOnTouch   = 1<<1,
      ReactToOnDamage  = 1<<2,
      RespondToObject  = 1<<3,
      RespondToPC      = 1<<4,
      RespondToNPC     = 1<<5,
      StartEnabled     = 1<<6,
      };

    struct Cb : DynamicWorld::BBoxCallback {
      Cb(AbstractTrigger* tg):tg(tg) {}
      void onCollide(DynamicWorld::BulletBody &other) override;
      AbstractTrigger* tg;
      };

    ZenLoad::zCVobData           data;

    virtual void                 onTrigger(const TriggerEvent& evt);
    virtual void                 onUntrigger(const TriggerEvent& evt);
    virtual void                 onGotoMsg(const TriggerEvent& evt);
    void                         moveEvent() override;

    bool                         hasFlag(ReactFlg flg) const;

    void                         enableTicks();
    void                         disableTicks();
    const std::vector<Npc*>&     intersections() const;

  private:
    Cb                           callback;
    DynamicWorld::BBoxBody       box;
    CollisionZone                boxNpc;
    Tempest::Vec3                bboxSize, bboxOrigin;

    std::vector<Npc*>            intersect;
    uint32_t                     emitCount = 0;
    bool                         disabled  = false;
  };
