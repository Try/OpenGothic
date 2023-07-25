#include "lego.h"
#include "ikarus.h"

#include <Tempest/Log>

using namespace Tempest;

LeGo::LeGo(GameScript& /*owner*/, phoenix::vm& vm) {
  Log::i("DMA mod detected: LeGo");

  // ## FrameFunctions
  vm.override_function("FF_APPLYONCEEXTGT", [](int func, int delay, int cycles) {
    Log::e("not implemented call [FF_APPLYONCEEXTGT]");
    });

  // HookEngine
  vm.override_function("HookEngineF", [](int address, int oldInstr, int function){
    Log::e("not implemented call [HookEngineF]");
    });
  vm.override_function("HookEngineI", [](int address, int oldInstr, int function){
    Log::e("not implemented call [HookEngineI]");
    });

  vm.override_function("CC_Register", [](int func, std::string_view prefix, std::string_view desc){
    Log::e("not implemented call [CC_Register]");
    });
  vm.override_function("_RENDER_INIT", [](){
    Log::e("not implemented call [_RENDER_INIT]");
    });
  vm.override_function("PRINT_FIXPS", [](){
    Log::e("not implemented call [PRINT_FIXPS]");
    });

  vm.override_function("BUFFLIST_INIT", [](){
    // too much stuff inside it
    Log::e("not implemented call [BUFFLIST_INIT]");
    });
  //
  vm.override_function("_PM_CREATEFOREACHTABLE", [](){});
  }

bool LeGo::isRequired(phoenix::vm& vm) {
  return
      vm.find_symbol_by_name("LeGo_InitFlags") != nullptr &&
      vm.find_symbol_by_name("LeGo_Init") != nullptr &&
      Ikarus::isRequired(vm);
  }
