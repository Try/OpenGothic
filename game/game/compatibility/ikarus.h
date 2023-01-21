#pragma once

#include <phoenix/vm.hh>

#include "scriptplugin.h"
#include "mem32.h"

class GameScript;

class Ikarus : public ScriptPlugin {
  public:
    Ikarus(GameScript& owner, phoenix::vm& vm);

    static bool isRequired(phoenix::vm& vm);

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

    void  mem_setupexceptionhandler         ();
    void  mem_getaddress_init               ();
    void  mem_printstacktrace_implementation();
    int   mem_getfuncptr                     (int func);
    void  mem_replacefunc                   (int dest, int func);
    int   mem_searchvobbyname                (std::string_view name);

    // ## Basic Read Write ##
    int         mem_readint       (int address);
    void        mem_writeint      (int address, int val);
    void        mem_copybytes     (int src, int dst, int size);
    std::string mem_getcommandline();

    // pointers
    std::shared_ptr<phoenix::instance>  mem_ptrtoinst(int address);

    // ## Basic zCParser related functions ##
    int  _takeref    (int val);
    int  _takeref_s  (std::string_view val);
    int  _takeref_f  (float val);

    // ## Preliminary MEM_Alloc and MEM_Free ##
    int  mem_alloc           (int amount);
    void mem_free            (int address);

    void  call__stdcall      (int address);

    Mem32    allocator;
    uint32_t versionHint   = 0;
    ptr32_t  oGame_Pointer = 0;
    oGame    gameProxy;
    zCParser parserProxy;
    phoenix::vm& vm;
    // GameScript& owner;
  };

