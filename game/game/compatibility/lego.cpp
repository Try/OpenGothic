#include "lego.h"
#include "ikarus.h"

#include <Tempest/Log>

using namespace Tempest;

LeGo::LeGo(GameScript& /*owner*/, Ikarus& ikarus, phoenix::vm& vm) : ikarus(ikarus), vm(vm) {
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

  // ## PermMem
  vm.override_function("CREATE", [this](int inst) { return create(inst); });

  vm.override_function("LOCALS", [](){
    //NOTE: push local-variables to in-flight memory and restore at function end
    Log::e("TODO: LeGo-LOCALS.");
    });
  }

bool LeGo::isRequired(phoenix::vm& vm) {
  return
      vm.find_symbol_by_name("LeGo_InitFlags") != nullptr &&
      vm.find_symbol_by_name("LeGo_Init") != nullptr &&
         Ikarus::isRequired(vm);
  }

int LeGo::create(int instId) {
  auto* sym  = vm.find_symbol_by_index(uint32_t(instId));
  if(sym==nullptr) {
    Log::e("LeGo::create invalid symbold id (",instId,")");
    return 0;
    }

  auto  ptr  = ikarus.mem_alloc(128);
  auto  inst = std::make_shared<Ikarus::memory_instance>(ptr);
  vm.unsafe_call(sym);
  return ptr;
  }
