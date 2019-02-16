#pragma once

#include <string>

#include <zenload/zTypes.h>

class Trigger final {
  public:
    Trigger(const ZenLoad::zCVobData& data);

    const std::string& name() const;
  private:
    ZenLoad::zCVobData data;
  };
