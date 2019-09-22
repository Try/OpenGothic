#include "particlesdefinitions.h"

#include <Tempest/Log>

#include "graphics/particlefx.h"
#include "gothic.h"

using namespace Tempest;

ParticlesDefinitions::ParticlesDefinitions(Gothic& gothic) {
  vm = gothic.createVm(u"_work/Data/scripts/_compiled/PARTICLEFX.dat");
  }

ParticlesDefinitions::~ParticlesDefinitions() {
  vm->clearReferences(Daedalus::IC_Pfx);
  }

const ParticleFx* ParticlesDefinitions::get(const char *name) {
  auto it = pfx.find(name);
  if(it!=pfx.end())
    return it->second.get();
  auto def = implGet(name);
  if(def==nullptr)
    return nullptr;
  std::unique_ptr<ParticleFx> p{new ParticleFx(*def)};
  auto ret = pfx.insert(std::make_pair<std::string,std::unique_ptr<ParticleFx>>(name,std::move(p)));
  return ret.first->second.get();
  }

const Daedalus::GEngineClasses::C_ParticleFX* ParticlesDefinitions::implGet(const char *name) {
  static Daedalus::GEngineClasses::C_ParticleFX ret={};
  if(!vm)
    return nullptr;

  auto id = vm->getDATFile().getSymbolIndexByName(name);
  if(id==size_t(-1)) {
    Log::e("invalid particle system: \"",name,"\"");
    return nullptr;
    }

  vm->initializeInstance(ret, id, Daedalus::IC_Pfx);
  vm->clearReferences(Daedalus::IC_Pfx);
  return &ret;
  }
