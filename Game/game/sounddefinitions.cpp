#include "sounddefinitions.h"

#include <Tempest/Log>
#include <gothic.h>

using namespace Tempest;

SoundDefinitions::SoundDefinitions(Gothic &gothic) {
  auto vm = gothic.createVm(u"_work/Data/Scripts/_compiled/SFX.dat");

  vm->getDATFile().iterateSymbolsOfClass("C_SFX",[this,&vm](size_t i,Daedalus::PARSymbol& s){
    Daedalus::GEngineClasses::C_SFX sfx;
    vm->initializeInstance(sfx, i, Daedalus::IC_Sfx);
    this->sfx[s.name] = std::move(sfx);
    });
  }

const Daedalus::GEngineClasses::C_SFX& SoundDefinitions::getSfx(const char *name) {
  auto i = sfx.find(name);
  if(i!=sfx.end())
    return i->second;
  static Daedalus::GEngineClasses::C_SFX s;
  return s;
  }

