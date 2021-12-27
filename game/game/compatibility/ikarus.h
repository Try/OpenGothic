#pragma once

#include <daedalus/DaedalusVM.h>

#include "scriptplugin.h"
#include "mem32.h"

class GameScript;

class Ikarus : public ScriptPlugin {
  public:
    Ikarus(GameScript& owner, Daedalus::DaedalusVM& vm);

    static bool isRequired(Daedalus::DaedalusVM& vm);

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

    void  mem_setupexceptionhandler         (Daedalus::DaedalusVM &vm);
    void  mem_getaddress_init               (Daedalus::DaedalusVM &vm);
    void  mem_printstacktrace_implementation(Daedalus::DaedalusVM &vm);
    void  mem_getfuncptr                    (Daedalus::DaedalusVM &vm);
    void  mem_replacefunc                   (Daedalus::DaedalusVM &vm);
    void  mem_searchvobbyname               (Daedalus::DaedalusVM &vm);

    // ## Basic Read Write ##
    void  mem_readint                       (Daedalus::DaedalusVM &vm);
    void  mem_writeint                      (Daedalus::DaedalusVM &vm);
    void  mem_copybytes                     (Daedalus::DaedalusVM &vm);
    void  mem_getcommandline                (Daedalus::DaedalusVM &vm);

    // pointers
    void  mem_ptrtoinst                     (Daedalus::DaedalusVM &vm);

    // ## Basic zCParser related functions ##
    void  _takeref    (Daedalus::DaedalusVM &vm);
    void  _takeref_s  (Daedalus::DaedalusVM &vm);
    void  _takeref_f  (Daedalus::DaedalusVM &vm);

    // ## Preliminary MEM_Alloc and MEM_Free ##
    void  mem_alloc                        (Daedalus::DaedalusVM &vm);
    void  mem_free                         (Daedalus::DaedalusVM &vm);

    void  call__stdcall      (Daedalus::DaedalusVM &vm);

    Mem32    allocator;
    uint32_t versionHint   = 0;
    ptr32_t  oGame_Pointer = 0;
    oGame    gameProxy;
    zCParser parserProxy;
    // GameScript& owner;
  };

