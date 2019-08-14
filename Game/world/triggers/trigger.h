#pragma once

#include <string>
#include <zenload/zTypes.h>

class Npc;

class TriggerEvent final {
  public:
    TriggerEvent()=default;
    TriggerEvent(std::string target,std::string emitter):target(std::move(target)), emitter(std::move(emitter)){}

    const std::string target;
    const std::string emitter;
  };

class Trigger {
  public:
    Trigger(ZenLoad::zCVobData&& data);
    virtual ~Trigger()=default;

    const std::string& name() const;
    virtual void onTrigger(const TriggerEvent& evt);
    virtual void onIntersect(Npc& n);
    virtual bool checkPos(float x,float y,float z) const;

  protected:
    ZenLoad::zCVobData data;
  };
