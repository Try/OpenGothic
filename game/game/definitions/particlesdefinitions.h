#pragma once

#include <phoenix/vm.hh>
#include <phoenix/ext/daedalus_classes.hh>

#include <unordered_map>
#include <memory>
#include <mutex>

#include "graphics/visualfx.h"

class ParticleFx;

class ParticlesDefinitions final {
  public:
    ParticlesDefinitions();
    ~ParticlesDefinitions();

    const ParticleFx* get(std::string_view name, bool relaxed);
    const ParticleFx* get(const ParticleFx* base, const VisualFx::Key* key);

  private:
    std::recursive_mutex                                        sync;
    std::unique_ptr<phoenix::vm>                       vm;

    std::unordered_map<std::string,          std::unique_ptr<ParticleFx>> pfx;
    std::unordered_map<const VisualFx::Key*, std::unique_ptr<ParticleFx>> pfxKey;

    const ParticleFx* implGet(std::string_view name, bool relaxed);
    const ParticleFx* implGet(const ParticleFx& base, const VisualFx::Key& key);

    std::shared_ptr<phoenix::c_particle_fx> implGetDirect(std::string_view name, bool relaxed);
  };
