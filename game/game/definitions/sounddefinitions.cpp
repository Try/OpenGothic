#include "sounddefinitions.h"

#include <Tempest/Log>
#include <gothic.h>

using namespace Tempest;

SoundDefinitions::SoundDefinitions() {
  auto vm = Gothic::inst().createPhoenixVm("Sfx.dat");

  vm->enumerate_instances_by_class_name("C_SFX", [this, &vm](phoenix::symbol& s) {
    this->sfx[s.name()] = vm->init_instance<phoenix::c_sfx>(&s);
  });
  }

const phoenix::c_sfx& SoundDefinitions::operator[](std::string_view name) const {
  char buf[256] = {};
  std::snprintf(buf,sizeof(buf),"%.*s",int(name.size()),name.data());
  auto i = sfx.find(buf);
  if(i!=sfx.end())
    return *i->second;
  static phoenix::c_sfx s {};
  return s;
  }

