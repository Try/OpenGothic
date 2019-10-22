#include "musicdefinitions.h"

#include <Tempest/Log>
#include "gothic.h"

using namespace Tempest;

MusicDefinitions::MusicDefinitions(Gothic& gothic) {
  vm = gothic.createVm(u"Music.dat");
  }

MusicDefinitions::~MusicDefinitions() {
  vm->clearReferences(Daedalus::IC_MusicTheme);
  }

const Daedalus::GEngineClasses::C_MusicTheme &MusicDefinitions::get(const char *name) {
  static Daedalus::GEngineClasses::C_MusicTheme ret={};
  if(!vm)
    return ret;
  auto id = vm->getDATFile().getSymbolIndexByName(name);
  if(id==size_t(-1)) {
    Log::e("invalid music theme: \"",name,"\"");
    return ret;
    }

  vm->initializeInstance(mm, id, Daedalus::IC_MusicTheme);
  return mm;
  }
