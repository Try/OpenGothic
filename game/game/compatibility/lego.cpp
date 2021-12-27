#include "lego.h"
#include "ikarus.h"

#include <Tempest/Log>

using namespace Tempest;

LeGo::LeGo(GameScript& /*owner*/, Daedalus::DaedalusVM& /*vm*/) {
  Log::i("DMA mod detected: LeGo");
  }

bool LeGo::isRequired(Daedalus::DaedalusVM& vm) {
  auto& dat = vm.getDATFile();
  return
      dat.hasSymbolName("LeGo_InitFlags") &&
      dat.hasSymbolName("LeGo_Init") &&
      Ikarus::isRequired(vm);
  }
