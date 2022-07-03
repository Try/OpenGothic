#include "lego.h"
#include "ikarus.h"

#include <Tempest/Log>

using namespace Tempest;

LeGo::LeGo(GameScript& /*owner*/, phoenix::daedalus::vm& /*vm*/) {
  Log::i("DMA mod detected: LeGo");
  }

bool LeGo::isRequired(phoenix::daedalus::vm& vm) {
  return
      vm.find_symbol_by_name("LeGo_InitFlags") != nullptr &&
      vm.find_symbol_by_name("LeGo_Init") != nullptr &&
      Ikarus::isRequired(vm);
  }
