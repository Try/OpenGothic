#pragma once

#include <phoenix/vm.hh>

#include "scriptplugin.h"
#include "mem32.h"

class GameScript;

class Ikarus : public ScriptPlugin {
  public:
    Ikarus(GameScript& owner, phoenix::vm& vm);

    static bool isRequired(phoenix::script& vm);

  private:
    using ptr32_t = Mem32::ptr32_t;

    struct oGame {
      int data[16] = {};
      };

    struct zCParser {
      uint8_t padd0[24] = {};
      ptr32_t symtab_table_array = 0;        // 24
      uint8_t padd1[8] = {};
      ptr32_t sorted_symtab_table_array = 0; // 36
      uint8_t padd2[32] = {};
      ptr32_t stack = 0;                     // 72
      ptr32_t stack_stackPtr = 0;            // 76
      };

    struct memory_instance;

    std::string mem_getcommandline();
    void        mem_sendtospy(int cat, std::string_view msg);

    void        mem_setupexceptionhandler         ();
    void        mem_getaddress_init               ();
    void        mem_printstacktrace_implementation();
    int         mem_getfuncptr                    (int func);
    int         mem_getfuncid                     (int func);
    void        mem_replacefunc                   (int dest, int func);
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

    void        mem_trap_i32(int32_t, size_t, const std::shared_ptr<phoenix::instance>&, phoenix::symbol&);
    int32_t     mem_trap_i32(size_t, const std::shared_ptr<phoenix::instance>&, phoenix::symbol&);

    // ## Basic zCParser related functions ##
    int  _takeref    (int val);
    int  _takeref_s  (std::string_view val);
    int  _takeref_f  (float val);
    // ## Preliminary MEM_Alloc and MEM_Free ##
    int  mem_alloc           (int amount);
    void mem_free            (int address);

    // ## strings
    std::string str_substr(std::string_view str, int start, int count);
    int         str_len   (std::string_view str);

    // control-flow
    phoenix::naked_call repeat   (phoenix::vm& vm);
    void                loop_trap(phoenix::symbol* i);

    void call__stdcall(int address);
    int  hash(int x);

    phoenix::vm& vm;
    Mem32        allocator;
    
    struct Loop {
      uint32_t         pc = 0;
      phoenix::symbol* i  = nullptr;
      };
    std::vector<Loop> loop_start;

    uint32_t     versionHint   = 0;
    ptr32_t      oGame_Pointer = 0;
    oGame        gameProxy;
    zCParser     parserProxy;

    ptr32_t      symbolsPtr = 0;
  };

