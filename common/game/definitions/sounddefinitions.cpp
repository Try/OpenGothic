#include "sounddefinitions.h"

#include <Tempest/Log>
#include <algorithm>
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


std::vector<std::string> SoundDefinitions::find(std::initializer_list<std::string_view> parts) const {
  std::vector<std::string> ret;
  for(auto& i:sfx) {
    for(auto p:parts)
      if(i.first.find(p)!=std::string::npos) {
        ret.push_back(i.first);
        break;
        }
    }
  std::sort(ret.begin(), ret.end(), [](const std::string& a, const std::string& b){
    return a.size()!=b.size() ? a.size()<b.size() : a<b;
    });
  return ret;
  }
