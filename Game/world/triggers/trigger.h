#pragma once

#include <string>

#include <zenload/zTypes.h>

class Npc;

class Trigger {
  public:
    Trigger(ZenLoad::zCVobData&& data);
    virtual ~Trigger()=default;

    const std::string& name() const;
    virtual void onTrigger();
    virtual void onIntersect(Npc& n);

  protected:
    ZenLoad::zCVobData data;
  };
