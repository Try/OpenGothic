#include "particlesdefinitions.h"

#include <Tempest/Log>
#include "gothic.h"

using namespace Tempest;

ParticlesDefinitions::ParticlesDefinitions(Gothic& gothic) {
  vm = gothic.createVm(u"_work/Data/Scripts/_compiled/Music.dat");
  }

const Daedalus::GEngineClasses::C_ParticleFX &ParticlesDefinitions::get(const char *name) {
  static Daedalus::GEngineClasses::C_ParticleFX ret={};
  if(!vm)
    return ret;
  auto id = vm->getDATFile().getSymbolIndexByName(name);
  if(id==size_t(-1)) {
    Log::e("invalid particle system: \"",name,"\"");
    return ret;
    }

  vm->initializeInstance(&mm, id, Daedalus::IC_Pfx);
  return mm;
  }
