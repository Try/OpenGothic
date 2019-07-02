#include "musicdefinitions.h"

#include <Tempest/Log>
#include "gothic.h"

using namespace Tempest;

MusicDefinitions::MusicDefinitions(Gothic& gothic) {
  vm = gothic.createVm(u"_work/Data/Scripts/_compiled/Music.dat");
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
