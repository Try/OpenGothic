#include "sounddefinitions.h"

#include <Tempest/Log>
#include <gothic.h>
#include "utils/string_frm.h"

using namespace Tempest;

SoundDefinitions::SoundDefinitions() {
  auto vm = Gothic::inst().createPhoenixVm("Sfx.dat");

  vm->enumerate_instances_by_class_name("C_SFX", [this, &vm](phoenix::symbol& s) {
    try {
      this->sfx[s.name()] = vm->init_instance<phoenix::c_sfx>(&s);
      } catch(const phoenix::script_error&) {
      // There was an error during initialization. Ignore it.
      }
  });
  }

const phoenix::c_sfx& SoundDefinitions::operator[](std::string_view name) const {
  std::string buf(name); // FIXME
  auto i = sfx.find(buf);
  if(i!=sfx.end())
    return *i->second;
  static const phoenix::c_sfx s {};
  return s;
  }

