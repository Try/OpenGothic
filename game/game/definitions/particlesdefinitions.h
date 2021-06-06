#pragma once

#include <daedalus/DaedalusStdlib.h>

#include <unordered_map>
#include <memory>
#include <mutex>

class ParticleFx;

class ParticlesDefinitions final {
  public:
    ParticlesDefinitions();
    ~ParticlesDefinitions();

    const ParticleFx* get(const char* name);
    const ParticleFx* get(const Daedalus::GEngineClasses::C_ParticleFXEmitKey& k);

  private:
    std::mutex                                                  sync;
    std::unique_ptr<Daedalus::DaedalusVM>                       vm;

    std::unordered_map<std::string,std::unique_ptr<ParticleFx>> pfx;
    std::unordered_map<size_t,     std::unique_ptr<ParticleFx>> pfxKey;

    const ParticleFx* implGet(const char* name);
    const ParticleFx* implGet(const Daedalus::GEngineClasses::C_ParticleFXEmitKey& k);

    bool implGet(const char* name, Daedalus::GEngineClasses::C_ParticleFX &ret);
  };
