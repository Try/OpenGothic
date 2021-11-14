#include "ikarus.h"

#include <Tempest/Log>

using namespace Tempest;

Ikarus::Ikarus(GameScript& owner, Daedalus::DaedalusVM& vm)
  :owner(owner) {
  Log::i("DMA mod detected: Ikarus");

  vm.registerExternalFunction("MEM_ReadInt",  [this](Daedalus::DaedalusVM& vm){ mem_readint(vm);  });
  vm.registerExternalFunction("MEM_WriteInt", [this](Daedalus::DaedalusVM& vm){ mem_writeInt(vm); });
  }

bool Ikarus::isRequired(Daedalus::DaedalusVM& vm) {
  return vm.getDATFile().hasSymbolName("MEM_InitAll");
  }

void Ikarus::mem_readint(Daedalus::DaedalusVM& vm) {
  auto address = vm.popInt();
  (void)address;
  vm.setReturn(0x1);
  }

void Ikarus::mem_writeInt(Daedalus::DaedalusVM& vm) {
  auto val = vm.popInt();
  (void)val;
  auto address = vm.popInt();
  (void)address;
  }
