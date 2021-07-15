#include "musicdefinitions.h"

#include <Tempest/Log>

#include "gothic.h"

using namespace Tempest;

MusicDefinitions::MusicDefinitions() {
  vm = Gothic::inst().createVm("Music.dat");
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

const Daedalus::GEngineClasses::C_MusicTheme* MusicDefinitions::operator[](std::string_view name) const {
  if(!vm)
    return nullptr;

  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"%.*s",int(name.size()),name.data());
  auto id = vm->getDATFile().getSymbolIndexByName(buf);
  if(id==size_t(-1))
    return nullptr;
  for(auto& i:themes) {
    if(i.symId==id)
      return &i;
    }
  return nullptr;
  }
