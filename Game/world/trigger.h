#pragma once

#include <string>

#include <zenload/zTypes.h>

class Trigger {
  public:
    Trigger(ZenLoad::zCVobData&& data);
    virtual ~Trigger()=default;

    const std::string& name() const;
    virtual void onTrigger();

  protected:
    ZenLoad::zCVobData data;
  };
