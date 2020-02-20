#include "musicdefinitions.h"

#include <Tempest/Log>
#include "gothic.h"

using namespace Tempest;

MusicDefinitions::MusicDefinitions(Gothic& gothic) {
  vm = gothic.createVm(u"Music.dat");
  vm->getDATFile().iterateSymbolsOfClass("C_MusicTheme",[this](size_t i,Daedalus::PARSymbol& /*s*/){
    Theme theme={};

    vm->initializeInstance(theme, i, Daedalus::IC_MusicTheme);
    vm->clearReferences(Daedalus::IC_MusicTheme);

    theme.symId = i;
    themes.push_back(theme);
    });
  }

MusicDefinitions::~MusicDefinitions() {
  vm->clearReferences(Daedalus::IC_MusicTheme);
  }

const Daedalus::GEngineClasses::C_MusicTheme* MusicDefinitions::get(const char *name) {
  if(!vm)
    return nullptr;
  auto id = vm->getDATFile().getSymbolIndexByName(name);
  if(id==size_t(-1)) {
    static std::unordered_set<size_t> s;
    if(s.find(id)==s.end()) {
      s.insert(id);
      Log::e("invalid music theme: \"",name,"\"");
      }
    return nullptr;
    }
  for(auto& i:themes) {
    if(i.symId==id)
      return &i;
    }
  return nullptr;
  }
