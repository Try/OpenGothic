#pragma once

#include <zenkit/vobs/Misc.hh>
#include <zenkit/vobs/Trigger.hh>
#include <string>

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
      T_ToggleEnable,
      T_Touch,
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
      zenkit::MoverMessageType msg = zenkit::MoverMessageType::FIXED_DIRECT;
      int32_t                  key = 0;
      } move;
  };

class AbstractTrigger : public Vob {
  public:
    AbstractTrigger(Vob* parent, World& world, const zenkit::VirtualObject& data, Flags flags);
    virtual ~AbstractTrigger();

    std::string_view             name() const;
    bool                         isEnabled() const;
    bool                         hasDelayedEvents() const;

    void                         processDelayedEvents();
    void                         processEvent(const TriggerEvent& evt);
    virtual void                 onIntersect(Npc& n);
    virtual void                 tick(uint64_t dt);

    virtual bool                 hasVolume() const;
    bool                         checkPos(const Tempest::Vec3& pos) const;

    void                         save(Serialize& fout) const override;
    void                         load(Serialize &fin) override;

  protected:
    struct Cb : DynamicWorld::BBoxCallback {
      Cb(AbstractTrigger* tg):tg(tg) {}
      void onCollide(DynamicWorld::BulletBody &other) override;
      AbstractTrigger* tg;
      };

    virtual void                 onTrigger(const TriggerEvent& evt);
    virtual void                 onUntrigger(const TriggerEvent& evt);
    virtual void                 onGotoMsg(const TriggerEvent& evt);
    void                         moveEvent() override;

    void                         enableTicks();
    void                         disableTicks();
    const std::vector<Npc*>&     intersections() const;

    void                         implProcessEvent(const TriggerEvent& evt);

  private:
    Cb                           callback;
    DynamicWorld::BBoxBody       box;
    CollisionZone                boxNpc;
    Tempest::Vec3                bboxSize, bboxOrigin;

    uint64_t                     fireDelay          = 0;
    uint64_t                     retriggerDelay     = 0;
    uint32_t                     maxActivationCount = uint32_t(-1);
    bool                         reactToOnTrigger   = true;
    bool                         sendUntrigger      = true;
    bool                         reactToOnTouch     = true;
    bool                         respondToNpc       = true;
    bool                         respondToPlayer    = true;
    bool                         respondToObject    = true;

    uint32_t                     emitCount = 0;
    bool                         disabled  = false;
    uint64_t                     emitTimeLast = 0;

    TriggerEvent                 delayedEvent;
    bool                         ticksEnabled = false;

  protected:
    std::string                  vobName;
    std::string                  target;
  };
