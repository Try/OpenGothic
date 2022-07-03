#include "spelldefinitions.h"

#include <Tempest/Log>
#include <cctype>

using namespace Tempest;

SpellDefinitions::SpellDefinitions(phoenix::daedalus::vm &vm) : vm(vm) {
  vm.enumerate_instances_by_class_name("C_Spell", [this, &vm](phoenix::daedalus::symbol& sym){
    spl.push_back(vm.init_instance<phoenix::daedalus::c_spell>(&sym));
    });
  }

SpellDefinitions::~SpellDefinitions() {
  }

const phoenix::daedalus::c_spell& SpellDefinitions::find(std::string_view instanceName) const {
  char format[64]={};
  std::snprintf(format,sizeof(format),"SPELL_%.*s",int(instanceName.size()),instanceName.data());
  for(auto& i:format)
    i = char(std::toupper(i));

  for(auto& i:spl) { // TODO: optimize
    auto sym = vm.find_symbol_by_instance(i);
    if(sym->name()==format)
      return *i;
  }
  Log::d("invalid spell [",instanceName.data(),"]");
  static phoenix::daedalus::c_spell szero={};
  return szero;
  }
