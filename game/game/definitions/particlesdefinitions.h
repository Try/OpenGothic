#pragma once

#include <daedalus/DaedalusStdlib.h>

#include <unordered_map>
#include <memory>
#include <mutex>

#include "graphics/visualfx.h"

class ParticleFx;

class ParticlesDefinitions final {
  public:
    ParticlesDefinitions();
    ~ParticlesDefinitions();

    const ParticleFx* get(const char* name);
    const ParticleFx* get(const ParticleFx* base, const VisualFx::Key* key);

  private:
    std::mutex                                                  sync;
    std::unique_ptr<Daedalus::DaedalusVM>                       vm;

    std::unordered_map<std::string,          std::unique_ptr<ParticleFx>> pfx;
    std::unordered_map<const VisualFx::Key*, std::unique_ptr<ParticleFx>> pfxKey;

    const ParticleFx* implGet(const char* name);
    const ParticleFx* implGet(const ParticleFx& base, const VisualFx::Key& key);

    bool implGet(const char* name, Daedalus::GEngineClasses::C_ParticleFX &ret);
  };
