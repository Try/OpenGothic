#pragma once

#include <daedalus/DaedalusStdlib.h>

#include <unordered_map>
#include <memory>

class Gothic;
class ParticleFx;

class ParticlesDefinitions final {
  public:
    ParticlesDefinitions(Gothic &gothic);
    ~ParticlesDefinitions();

    const ParticleFx *get(const char* name);

  private:
    std::unique_ptr<Daedalus::DaedalusVM>                       vm;
    std::unordered_map<std::string,std::unique_ptr<ParticleFx>> pfx;

    const Daedalus::GEngineClasses::C_ParticleFX *implGet(const char* name);
  };
