#pragma once

#include <string>
#include <zenload/zTypes.h>

class Npc;
class World;

class TriggerEvent final {
  public:
    TriggerEvent()=default;
    TriggerEvent(std::string target,std::string emitter):target(std::move(target)), emitter(std::move(emitter)){}
    TriggerEvent(std::string target,std::string emitter,uint64_t t)
      :target(std::move(target)), emitter(std::move(emitter)),timeBarrier(t){}
    TriggerEvent(bool startup):wrldStartup(startup){}

    const std::string target;
    const std::string emitter;
    bool              wrldStartup = false;
    uint64_t          timeBarrier = 0;
  };

class AbstractTrigger {
  public:
    AbstractTrigger(ZenLoad::zCVobData&& data,World& owner);
    virtual ~AbstractTrigger()=default;

    ZenLoad::zCVobData::EVobType vobType() const;
    const std::string&           name() const;

    void                         processOnStart(const TriggerEvent& evt);
    void                         processEvent(const TriggerEvent& evt);
    virtual void                 onIntersect(Npc& n);
    virtual void                 tick(uint64_t dt);

    virtual bool                 hasVolume() const;
    virtual bool                 checkPos(float x,float y,float z) const;

  protected:
    ZenLoad::zCVobData           data;
    World&                       owner;
    std::vector<Npc*>            intersect;
    uint32_t                     emitCount=0;

    virtual void                 onTrigger(const TriggerEvent& evt);
    virtual void                 onUntrigger(const TriggerEvent& evt);

    void                         enableTicks();
    void                         disableTicks();
  };
