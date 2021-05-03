#include "spelldefinitions.h"

#include <daedalus/DaedalusVM.h>
#include <Tempest/Log>
#include <cctype>

using namespace Tempest;

SpellDefinitions::SpellDefinitions(Daedalus::DaedalusVM &vm) : vm(vm) {
  size_t count=0;
  vm.getDATFile().iterateSymbolsOfClass("C_Spell", [&count](size_t,Daedalus::PARSymbol&){
    ++count;
    });
  spl.resize(count);
  count=0;
  vm.getDATFile().iterateSymbolsOfClass("C_Spell", [&](size_t i,Daedalus::PARSymbol& p){
    vm.initializeInstance(spl[count], i, Daedalus::IC_Spell);
    spl[count].instName = p.name;
    ++count;
    });
  }

SpellDefinitions::~SpellDefinitions() {
  vm.clearReferences(Daedalus::IC_Spell);
  }

const Daedalus::GEngineClasses::C_Spell &SpellDefinitions::find(const char* instanceName) const {
  char format[64]={};
  std::snprintf(format,sizeof(format),"SPELL_%s",instanceName);
  for(auto& i:format)
    i = char(std::toupper(i));

  for(auto& i:spl)
    if(i.instName==format)
      return i;
  Log::d("invalid spell [",instanceName,"]");
  static Daedalus::GEngineClasses::C_Spell szero={};
  return szero;
  }
