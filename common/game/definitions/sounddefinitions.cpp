#include "sounddefinitions.h"

#include <Tempest/Log>
#include <gothic.h>

using namespace Tempest;

SoundDefinitions::SoundDefinitions() {
  auto vm = Gothic::inst().createPhoenixVm("Sfx.dat");

  vm->enumerate_instances_by_class_name("C_SFX", [this, &vm](zenkit::DaedalusSymbol& s) {
    try {
      this->sfx[s.name()] = vm->init_instance<zenkit::ISoundEffect>(&s);
      }
    catch(const zenkit::DaedalusScriptError&) {
      // There was an error during initialization. Ignore it.
      }
  });
  }

const zenkit::ISoundEffect& SoundDefinitions::operator[](std::string_view name) const {
  std::string buf(name); // FIXME
  auto i = sfx.find(buf);
  if(i!=sfx.end())
    return *i->second;
  static const zenkit::ISoundEffect s {};
  return s;
  }

