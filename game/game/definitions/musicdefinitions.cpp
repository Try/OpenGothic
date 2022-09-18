#include "musicdefinitions.h"

#include <Tempest/Log>

#include "gothic.h"

using namespace Tempest;

MusicDefinitions::MusicDefinitions() {
  vm = Gothic::inst().createPhoenixVm("Music.dat");

  vm->enumerate_instances_by_class_name("C_MusicTheme", [this](phoenix::symbol& s) {
    themes.push_back(vm->init_instance<phoenix::c_music_theme>(&s));
  });
  }

MusicDefinitions::~MusicDefinitions() {
  }

const phoenix::c_music_theme* MusicDefinitions::operator[](std::string_view name) const {
  if(!vm)
    return nullptr;

  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"%.*s",int(name.size()),name.data());
  auto id = vm->find_symbol_by_name(buf);
  if(id==nullptr)
    return nullptr;
  for(auto& i:themes) {
    auto* sym = vm->find_symbol_by_instance(i);
    if(sym->index() == id->index())
      return i.get();
    }
  return nullptr;
  }
