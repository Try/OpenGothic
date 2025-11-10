#pragma once

#include <zenkit/DaedalusVm.hh>

#include "scriptplugin.h"
#include "mem32instances.h"
#include "mem32.h"
#include "cpu32.h"

class GameScript;

class Ikarus : public ScriptPlugin {
  public:
    Ikarus(GameScript& owner, zenkit::DaedalusVm& vm);

    static bool isRequired(zenkit::DaedalusScript& vm);

    using ptr32_t  = Compatibility::ptr32_t;
    using zCParser = Compatibility::zCParser;
    using zCTimer  = Compatibility::zCTimer;
    using oGame    = Compatibility::oGame;
    using zString  = Compatibility::zString;

    struct memory_instance : public zenkit::DaedalusTransientInstance {
      explicit memory_instance(Ikarus &owner, ptr32_t address) : owner(owner), address(address) {}

      void set_int(zenkit::DaedalusSymbol const& sym, uint16_t index, std::int32_t value) override;
      std::int32_t get_int(zenkit::DaedalusSymbol const& sym, uint16_t index) const override;

      void set_float(zenkit::DaedalusSymbol const& sym, uint16_t index, float value) override;
      float get_float(zenkit::DaedalusSymbol const& sym, uint16_t index) const override;

      void set_string(zenkit::DaedalusSymbol const& sym, uint16_t index, std::string_view value) override;
      const std::string& get_string(zenkit::DaedalusSymbol const& sym, uint16_t index) const override;

      Ikarus &owner;
      ptr32_t address = 0;
      };

    //  MEM_Alloc and MEM_Free ##
    int  mem_alloc  (int amount);
    int  mem_alloc  (int amount, const char* comment);
    void mem_free   (int address);
    int  mem_realloc(int address, int oldsz, int size);

    template <typename T>
    void register_stdcall(ptr32_t addr, const T& cb) {
      cpu.register_stdcall(addr, std::function {cb});
      }

    template <typename T>
    void register_thiscall(ptr32_t addr, const T& cb) {
      cpu.register_thiscall(addr, std::function {cb});
      }

  private:
    struct ScriptVar;

    void        setupEngineMemory();
    void        setupEngineText();
    void        memoryCallbackParser   (zCParser& p, std::memory_order ord);
    void        memoryCallbackParserVar(ScriptVar& v, uint32_t index, std::memory_order ord);

    std::string mem_getcommandline();
    void        mem_sendtospy(int cat, std::string_view msg);

    static int  mkf(int v);
    static int  truncf(int v);
    static int  roundf(int v);
    static int  addf(int a, int b);
    static int  subf(int a, int b);
    static int  mulf(int a, int b);
    static int  divf(int a, int b);

    void        ASM_Open(int);

    void        mem_setupexceptionhandler         ();
    void        mem_getaddress_init               ();
    void        mem_printstacktrace_implementation();
    int         mem_getfuncoffset                 (zenkit::DaedalusFunction func);
    int         mem_getfuncid                     (zenkit::DaedalusFunction func);
    void        mem_callbyid                      (int symbId);
    int         mem_getfuncptr                    (int symbId);
    void        mem_replacefunc                   (zenkit::DaedalusFunction dest, zenkit::DaedalusFunction func);
    int         mem_getfuncidbyoffset             (int off);
    void        mem_assigninst                    (int sym, int ptr);

    int         mem_searchvobbyname               (std::string_view name);
    int         mem_getsymbolindex                (std::string_view name);
    int         mem_getsymbolbyindex              (int index);
    int         mem_getsystemtime();

    // ## Basic Read Write ##
    int         mem_readint       (int address);
    void        mem_writeint      (int address, int val);
    void        mem_copybytes     (int src, int dst, int size);
    std::string mem_readstring    (int address);
    int         _mem_readstatarr  (int address, int off);
    zenkit::DaedalusNakedCall mem_readstatarr(zenkit::DaedalusVm& vm);
    // pointers
    auto        mem_ptrtoinst(ptr32_t address) -> std::shared_ptr<zenkit::DaedalusInstance>;
    int         mem_insttoptr(int index);
    auto        _takeref(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;

    ptr32_t     ASMINT_InternalStack = 0;
    ptr32_t     ASMINT_CallTarget    = 0;
    void        ASMINT_Init();
    void        ASMINT_CallMyExternal();

    // ## strings
    std::string str_fromchar(int ptr);
    std::string str_substr(std::string_view str, int start, int count);
    int         str_toint (std::string_view str);
    std::string str_upper (std::string_view str);

    // ## ini-file
    std::string mem_getgothopt          (std::string_view section, std::string_view option);
    std::string mem_getmodopt           (std::string_view section, std::string_view option);
    bool        mem_gothoptaectionexists(std::string_view section);
    bool        mem_gothoptexists       (std::string_view section, std::string_view option);
    bool        mem_modoptsectionexists (std::string_view section);
    bool        mem_modoptexists        (std::string_view section, std::string_view option);
    void        mem_setgothopt          (std::string_view section, std::string_view option, std::string_view value);

    // ## Windows api (basic)
    int        getusernamea(ptr32_t lpBuffer, ptr32_t pcbBuffer);
    void       getlocaltime(ptr32_t lpSystemTime);

    // control-flow
    zenkit::DaedalusNakedCall repeat   (zenkit::DaedalusVm& vm);
    zenkit::DaedalusNakedCall while_   (zenkit::DaedalusVm& vm);
    void                      loop_trap(zenkit::DaedalusSymbol* i);
    void                      loop_out (zenkit::DaedalusVm& vm);

    int  hash(int x);

    std::string_view demangleAddress(ptr32_t addr);

    GameScript&         gameScript;
    zenkit::DaedalusVm& vm;
    Mem32               allocator;
    Cpu32               cpu;

    struct Loop {
      uint32_t                pc = 0;
      zenkit::DaedalusSymbol* i  = nullptr;
      int32_t                 loopLen = 0;
      };
    std::vector<Loop> loop_start;

    struct ScriptVar {
      Compatibility::zString data = {};
      };

    uint32_t     versionHint     = 0;
    ptr32_t      MEMINT_StackPos = 0;
    int32_t      invMaxItems     = 9;
    zCTimer      zTimer          = {};
    oGame        memGame = {};

    ptr32_t      oGame_Pointer = 0;
    ptr32_t      gameman_Ptr  = 0;
    ptr32_t      symbolsPtr   = 0;
    ptr32_t      zFactory_Ptr = 0;
    ptr32_t      focusList[6] = {};

    ptr32_t      scriptVariables = 0;

  friend class LeGo;
  friend class Cpu32;
  };
