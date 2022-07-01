#include "sounddefinitions.h"

#include <Tempest/Log>
#include <gothic.h>

using namespace Tempest;

SoundDefinitions::SoundDefinitions() {
  auto vm = Gothic::inst().createPhoenixVm("Sfx.dat");
  auto parent = vm->loaded_script().find_symbol_by_name("C_SFX_DEF");

  for (auto& sym : vm->loaded_script().symbols()) {
    if (sym.type() == phoenix::daedalus::dt_instance && sym.parent() == parent->index()) {
      this->sfx[sym.name()] = vm->init_instance<phoenix::daedalus::c_sfx>(&sym);
    }
  }

  }

const phoenix::daedalus::c_sfx& SoundDefinitions::operator[](std::string_view name) const {
  char buf[256] = {};
  std::snprintf(buf,sizeof(buf),"%.*s",int(name.size()),name.data());
  auto i = sfx.find(buf);
  if(i!=sfx.end())
    return *i->second;
  static phoenix::daedalus::c_sfx s {};
  return s;
  }

