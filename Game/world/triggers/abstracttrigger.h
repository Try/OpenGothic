#pragma once

#include <string>
#include <zenload/zTypes.h>

class Npc;
class World;

class TriggerEvent final {
  public:
    TriggerEvent()=default;
    TriggerEvent(std::string target,std::string emitter):target(std::move(target)), emitter(std::move(emitter)){}
    TriggerEvent(bool startup):wrldStartup(startup){}

    const std::string target;
    const std::string emitter;
    bool              wrldStartup=false;
  };

class AbstractTrigger {
  public:
    AbstractTrigger(ZenLoad::zCVobData&& data,World& owner);
    virtual ~AbstractTrigger()=default;

    ZenLoad::zCVobData::EVobType vobType() const;
    const std::string&           name() const;
    virtual void                 onTrigger(const TriggerEvent& evt);
    virtual void                 onIntersect(Npc& n);

    virtual bool                 hasVolume() const;
    virtual bool                 checkPos(float x,float y,float z) const;

  protected:
    ZenLoad::zCVobData data;
    World&             owner;
  };
