#pragma once

#include <zenkit/DaedalusScript.hh>

#include "game/gamescript.h"

#include "cpu32.h"
#include "mem32.h"

class DirectMemory {
  public:
    DirectMemory(GameScript& owner, zenkit::DaedalusVm& vm);

    static bool isRequired(zenkit::DaedalusScript& vm);

    auto        demangleAddress(uint32_t addr) -> std::string_view;

    // hooks
    void        tick(uint64_t dt);
    void        eventPlayAni(std::string_view ani);

  private:
    using ptr32_t      = Mem32::ptr32_t;
    using zCParser     = Compatibility::zCParser;
    using zCPar_Symbol = Compatibility::zCPar_Symbol;
    using zCTimer      = Compatibility::zCTimer;
    using oGame        = Compatibility::oGame;
    using zString      = Compatibility::zString;

    struct memory_instance : public zenkit::DaedalusTransientInstance {
      explicit memory_instance(DirectMemory &owner, ptr32_t address) : owner(owner), address(address) {}

      void set_int(zenkit::DaedalusSymbol const& sym, uint16_t index, std::int32_t value) override;
      std::int32_t get_int(zenkit::DaedalusSymbol const& sym, uint16_t index) const override;

      void set_float(zenkit::DaedalusSymbol const& sym, uint16_t index, float value) override;
      float get_float(zenkit::DaedalusSymbol const& sym, uint16_t index) const override;

      void set_string(zenkit::DaedalusSymbol const& sym, uint16_t index, std::string_view value) override;
      const std::string& get_string(zenkit::DaedalusSymbol const& sym, uint16_t index) const override;

      DirectMemory& owner;
      ptr32_t       address = 0;
      };

    struct ScriptVar {
      zString data = {};
      };

    GameScript&         gameScript;
    zenkit::DaedalusVm& vm;
    Mem32               mem32;
    Cpu32               cpu;

    uint32_t    versionHint     = 504628679; // G2
    //int32_t     invMaxItems     = 9;
    zCTimer     zTimer          = {};
    oGame       memGame         = {};
    char        menuName[10]    = {};   //TODO: connect it to the game
    float       spawnRange      = 1000; // 10 meters, for now

    ptr32_t     oGame_Pointer   = 0;
    ptr32_t     gameman_Ptr     = 0;
    ptr32_t     zFactory_Ptr    = 0;
    ptr32_t     fontMan_Ptr     = 0;
    ptr32_t     focusList[6]    = {};

    ptr32_t     scriptVariables = 0;
    ptr32_t     scriptSymbols   = 0;

    void        setupFunctionTable();
    void        setupIkarusLoops();
    void        setupEngineMemory();
    void        setupEngineText();

    auto        repeat(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        while_(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        mem_goto(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    void        loop_trap(zenkit::DaedalusSymbol* i);

    uint32_t    traceBackExpression(zenkit::DaedalusVm& vm, uint32_t pc);
    auto        findSymbolByAddress(uint32_t addr) -> const zenkit::DaedalusSymbol*;

    struct LPayload {
      zenkit::DaedalusSymbol* i   = nullptr;
      int32_t                 len = 0;
      };
    std::unordered_map<uint32_t,LPayload>      loopPayload;
    std::unordered_map<uint32_t,uint32_t>      loopBacktrack;
    std::vector<const zenkit::DaedalusSymbol*> symbolsByAddress;

    // memory mappings
    void        memoryCallback(zCParser& p, std::memory_order ord);
    void        memoryCallback(ScriptVar& v, uint32_t index, std::memory_order ord);
    void        memoryCallback(zCPar_Symbol& s, uint32_t index, std::memory_order ord);
    void        memAssignString(zString& str, std::string_view cstr);
    void        memFromString(std::string& dst, const zString& str);

    // stacktrace & logs
    void        memPrintstacktraceImplementation();
    void        memSendToSpy(int cat, std::string_view msg);
    void        memReplaceFunc(zenkit::DaedalusFunction dest, zenkit::DaedalusFunction func);

    ptr32_t     ASMINT_InternalStack = 0;
    ptr32_t     ASMINT_CallTarget    = 0;
    void        ASMINT_Init();
    void        ASMINT_CallMyExternal();

    // pointers
    void        setupMemoryFunctions();
    auto        mem_ptrtoinst(ptr32_t address) -> std::shared_ptr<zenkit::DaedalusInstance>;
    int         mem_insttoptr(int index);
    auto        _takeref(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    int         mem_readint       (int address);
    void        mem_writeint      (int address, int val);
    void        mem_copybytes     (int src, int dst, int size);
    std::string mem_readstring    (int address);
    auto        mem_readstatarr(  zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    // pointers: MEM_Alloc and MEM_Free ##
    int         mem_alloc  (int amount);
    int         mem_alloc  (int amount, const char* comment);
    void        mem_free   (int address);
    int         mem_realloc(int address, int oldsz, int size);

    // mem-func
    void        setupDirectFunctions();
    int         mem_getfuncidbyoffset(int off);
    void        mem_assigninst       (int index, int ptr);

    void        directCall(zenkit::DaedalusVm& vm, zenkit::DaedalusSymbol& func);
    auto        mem_callbyid (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        mem_callbyptr(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    // ## Direct calls by id/ptr
    auto        memint_stackpushint (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    //TODO: MEMINT_StackPushString
    auto        memint_stackpushinst(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    //---
    auto        mem_popintresult   (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        mem_popstringresult(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        mem_popinstresult  (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;

    // math
    void        setupMathFunctions();
    static int  mkf(int v);
    static int  truncf(int v);
    static int  roundf(int v);
    static int  addf(int a, int b);
    static int  subf(int a, int b);
    static int  mulf(int a, int b);
    static int  divf(int a, int b);

    // strings
    void        setupStringsFunctions();
    // winapi
    void        setupWinapiFunctions();
    void        setupUtilityFunctions();
    void        setupHookEngine();
    void        setupzCParserFunctions();
    void        setupInitFileFunctions();
    // gothic-ui
    void        setupUiFunctions();
    void        setupFontFunctions();
    //
    void        setupNpcFunctions();
};
