#pragma once

#include <phoenix/vm.hh>

#include "scriptplugin.h"
#include "mem32.h"

class GameScript;

class Ikarus : public ScriptPlugin {
  public:
    Ikarus(GameScript& owner, phoenix::vm& vm);

    static bool isRequired(phoenix::script& vm);

    using ptr32_t = Mem32::ptr32_t;

    struct memory_instance : public phoenix::transient_instance {
      explicit memory_instance(Ikarus &owner, ptr32_t address) : owner(owner), address(address) {}

      void set_int(phoenix::symbol const& sym, uint16_t index, std::int32_t value) override;
      std::int32_t get_int(phoenix::symbol const& sym, uint16_t index) const override;

      void set_float(phoenix::symbol const& sym, uint16_t index, float value) override;
      float get_float(phoenix::symbol const& sym, uint16_t index) const override;

      void set_string(phoenix::symbol const& sym, uint16_t index, std::string_view value) override;
      const std::string& get_string(phoenix::symbol const& sym, uint16_t index) const override;

      Ikarus &owner;
      ptr32_t address = 0;
      };

    //  MEM_Alloc and MEM_Free ##
    int  mem_alloc  (int amount);
    void mem_free   (int address);
    int  mem_realloc(int address, int oldsz, int size);

  private:
    struct oGame {
      int     _VTBL;
      ptr32_t _ZCSESSION_CSMAN  = 0;
      ptr32_t _ZCSESSION_WORLD  = 0;
      ptr32_t _ZCSESSION_CAMERA = 0;

      uint8_t padd0[276] = {};

      ptr32_t INFOMAN = 0;
      };

    struct zCArray {
      ptr32_t ptr = 0;
      int32_t numAlloc   = 0;
      int32_t numInArray = 0;
      };

    struct zCParser {
      uint8_t  padd0[24] = {};
      zCArray  symtab_table;                  // 24
      ptr32_t  sorted_symtab_table_array = 0; // 36
      uint8_t  padd2[32] = {};
      ptr32_t  stack = 0;                     // 72
      ptr32_t  stack_stackPtr = 0;            // 76
      };

    std::string mem_getcommandline();
    void        mem_sendtospy(int cat, std::string_view msg);

    void        mem_setupexceptionhandler         ();
    void        mem_getaddress_init               ();
    void        mem_printstacktrace_implementation();
    int         mem_getfuncoffset                 (phoenix::func func);
    int         mem_getfuncid                     (phoenix::func func);
    void        mem_callbyid                      (int symbId);
    int         mem_getfuncptr                    (int symbId);
    void        mem_replacefunc                   (phoenix::func dest, phoenix::func func);
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
    // pointers
    std::shared_ptr<phoenix::instance> mem_ptrtoinst(ptr32_t address);

    // ## Basic zCParser related functions ##
    int         _takeref    (int val);
    int         _takeref_s  (std::string_view val);
    int         _takeref_f  (float val);

    // ## strings
    std::string str_substr(std::string_view str, int start, int count);
    int         str_len   (std::string_view str);
    int         str_toint (std::string_view str);

    // ## ini-file
    std::string mem_getgothopt          (std::string_view section, std::string_view option);
    std::string mem_getmodopt           (std::string_view section, std::string_view option);
    bool        mem_gothoptaectionexists(std::string_view section);
    bool        mem_gothoptexists       (std::string_view section, std::string_view option);
    bool        mem_modoptsectionexists (std::string_view section);
    bool        mem_modoptexists        (std::string_view section, std::string_view option);
    void        mem_setgothopt          (std::string_view section, std::string_view option, std::string_view value);

    // control-flow
    phoenix::naked_call repeat   (phoenix::vm& vm);
    phoenix::naked_call while_   (phoenix::vm& vm);
    void                loop_trap(phoenix::symbol* i);
    void                loop_out(phoenix::vm& vm);

    void call__stdcall(int address);
    int  hash(int x);

    phoenix::vm& vm;
    Mem32        allocator;
    
    struct Loop {
      uint32_t         pc = 0;
      phoenix::symbol* i  = nullptr;
      int32_t          loopLen = 0;
      };
    std::vector<Loop> loop_start;

    uint32_t     versionHint   = 0;
    zCParser     parserProxy;

    ptr32_t      oGame_Pointer = 0;
    oGame        gameProxy;

    ptr32_t      symbolsPtr = 0;
  };

