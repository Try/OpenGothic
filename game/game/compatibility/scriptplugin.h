#pragma once

#include <cstdint>

class ScriptPlugin {
  public:
    ScriptPlugin() = default;
    virtual ~ScriptPlugin() = default;

    virtual void tick(uint64_t dt);
  };

