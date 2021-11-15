#include "ikarus.h"

#include <Tempest/Log>

using namespace Tempest;

Ikarus::Ikarus(GameScript& owner, Daedalus::DaedalusVM& vm)
  :owner(owner) {
  Log::i("DMA mod detected: Ikarus");

  // Note: no inline asm
  vm.registerInternalFunction("ASMINT_MyExternal",     [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASMINT_CallMyExternal", [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASMINT_Init",           [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASM_Open",              [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASM_Close",             [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASM",                   [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASM_Run",               [](Daedalus::DaedalusVM&){});
  vm.registerInternalFunction("ASM_RunOnce",           [](Daedalus::DaedalusVM&){});

  vm.registerInternalFunction("MEM_ReadInt",         [this](Daedalus::DaedalusVM& vm){ mem_readint(vm);  });
  vm.registerInternalFunction("MEM_WriteInt",        [this](Daedalus::DaedalusVM& vm){ mem_writeint(vm); });
  vm.registerInternalFunction("MEM_SearchVobByName", [this](Daedalus::DaedalusVM& vm){ mem_searchvobbyname(vm); });

  vm.registerInternalFunction("CALL__stdcall",       [this](Daedalus::DaedalusVM& vm){ call__stdcall(vm); });

  // Get and set instance offsets
  vm.registerInternalFunction("_^",                  [this](Daedalus::DaedalusVM& vm){ _deref(vm);       });

  // Address Operator
  vm.registerInternalFunction("MEM_GetIntAddress",   [this](Daedalus::DaedalusVM& vm){ _takeref(vm);     });

  vm.registerInternalFunction("_@",                  [this](Daedalus::DaedalusVM& vm){ _takeref(vm);     });
  vm.registerInternalFunction("_@s",                 [this](Daedalus::DaedalusVM& vm){ _takeref_s(vm);   });
  vm.registerInternalFunction("_@f",                 [this](Daedalus::DaedalusVM& vm){ _takeref_f(vm);   });
  }

bool Ikarus::isRequired(Daedalus::DaedalusVM& vm) {
  auto& dat = vm.getDATFile();
  return
      dat.hasSymbolName("MEM_InitAll") &&
      dat.hasSymbolName("MEM_ReadInt") &&
      dat.hasSymbolName("MEM_WriteInt") &&
      dat.hasSymbolName("_@") &&
      dat.hasSymbolName("_^");
  }

void Ikarus::mem_readint(Daedalus::DaedalusVM& vm) {
  auto address = vm.popInt();
  (void)address;
  vm.setReturn(0x1);
  }

void Ikarus::mem_writeint(Daedalus::DaedalusVM& vm) {
  auto val = vm.popInt();
  (void)val;
  auto address = vm.popInt();
  (void)address;
  }

void Ikarus::mem_searchvobbyname(Daedalus::DaedalusVM& vm) {
  // see ZS_STAND_PEDRO_LOOP in VarusBikerEdition mod
  auto name = vm.popString();
  (void)name;
  vm.setReturn(0); // NULL-like
  }

void Ikarus::call__stdcall(Daedalus::DaedalusVM& vm) {
  auto address = vm.popInt();
  (void)address;
  }

void Ikarus::_deref(Daedalus::DaedalusVM& vm) {
  auto val = vm.popInt();
  (void)val;
  vm.setReturn(0x1);
  }

void Ikarus::_takeref(Daedalus::DaedalusVM& vm) {
  auto val = vm.popInt();
  (void)val;
  vm.setReturn(0x1);
  }

void Ikarus::_takeref_s(Daedalus::DaedalusVM& vm) {
  auto val = vm.popString();
  (void)val;
  vm.setReturn(0x1);
  }

void Ikarus::_takeref_f(Daedalus::DaedalusVM& vm) {
  auto val = vm.popFloat();
  (void)val;
  vm.setReturn(0x1);
  }
