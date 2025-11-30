#pragma once

#include <cstdint>
#include <string_view>

class ScriptPlugin {
  public:
    ScriptPlugin() = default;
    virtual ~ScriptPlugin() = default;

    virtual void tick(uint64_t dt);

    virtual void eventPlayAni(std::string_view ani);
  };

