#include "musicdefinitions.h"

#include <Tempest/Log>

#include "gothic.h"

using namespace Tempest;

MusicDefinitions::MusicDefinitions() {
  vm = Gothic::inst().createPhoenixVm("Music.dat");

  vm->loaded_script().enumerate_instances_by_class_name("C_MusicTheme", [this](phoenix::daedalus::symbol& s) {
    themes.push_back(vm->init_instance<phoenix::daedalus::c_music_theme>(&s));
  });
  }

MusicDefinitions::~MusicDefinitions() {
  }

const phoenix::daedalus::c_music_theme* MusicDefinitions::operator[](std::string_view name) const {
  if(!vm)
    return nullptr;

  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"%.*s",int(name.size()),name.data());
  auto id = vm->loaded_script().find_symbol_by_name(buf);
  if(id==nullptr)
    return nullptr;
  for(auto& i:themes) {
    if(i->get_symbol()->index() == id->index())
      return i.get();
    }
  return nullptr;
  }
