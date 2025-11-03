#pragma once

#include <zenkit/DaedalusVm.hh>

#include "scriptplugin.h"
#include "mem32.h"

class GameScript;

class Ikarus : public ScriptPlugin {
  public:
    Ikarus(GameScript& owner, zenkit::DaedalusVm& vm);

    static bool isRequired(zenkit::DaedalusScript& vm);

    using ptr32_t = Mem32::ptr32_t;

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

    template<class R, class...Args>
    void register_stdcall(ptr32_t addr, const std::function<R(Args...)>& callback) {
      register_stdcall_inner(addr, [callback](Ikarus& ikarus) {
        auto v = std::make_tuple(ikarus.call.pop<Args>()...);
        std::apply(callback, std::move(v));
        });
      }
    template <typename T>
    void register_stdcall(ptr32_t addr, const T& cb) {
      register_stdcall(addr, std::function {cb});
      }

  private:
    struct zString {
      int32_t _VTBL;
      int32_t _ALLOCATER;
      int32_t PTR;
      int32_t LEN;
      int32_t RES;
      };

    struct zCArray {
      ptr32_t ptr = 0;
      int32_t numAlloc   = 0;
      int32_t numInArray = 0;
      };

    struct oGame {
      int32_t _VTBL;
      ptr32_t _ZCSESSION_CSMAN  = 0;
      ptr32_t _ZCSESSION_WORLD  = 0;
      ptr32_t _ZCSESSION_CAMERA = 0;
      int32_t _ZCSESSION_AICAM;
      int32_t _ZCSESSION_CAMVOB;
      ptr32_t _ZCSESSION_VIEWPORT;
      int32_t CLIPRANGE;
      int32_t FOGRANGE;
      int32_t INSCRIPTSTARTUP;
      int32_t INLOADSAVEGAME;
      int32_t INLEVELCHANGE;
      int32_t ARRAY_VIEW[6];
      int32_t ARRAY_VIEW_VISIBLE[6];
      int32_t ARRAY_VIEW_ENABLED[6];
      int32_t SAVEGAMEMANAGER;
      int32_t GAME_TEXT;
      int32_t LOAD_SCREEN;
      int32_t SAVE_SCREEN;
      int32_t PAUSE_SCREEN;
      ptr32_t HPBAR;
      ptr32_t SWIMBAR;
      ptr32_t MANABAR;
      int32_t FOCUSBAR;
      int32_t SHOWPLAYERSTATUS;
      int32_t GAME_DRAWALL;
      int32_t GAME_FRAMEINFO;
      int32_t GAME_SHOWANIINFO;
      int32_t GAME_SHOWWAYNET;
      int32_t GAME_TESTMODE;
      int32_t GAME_EDITWAYNET;
      int32_t GAME_SHOWTIME;
      int32_t GAME_SHOWRANGES;
      int32_t DRAWWAYBOXES;
      int32_t SCRIPTSTARTUP;
      int32_t SHOWFREEPOINTS;
      int32_t SHOWROUTINENPC;
      int32_t LOADNEXTLEVEL;
      zString LOADNEXTLEVELNAME;
      zString LOADNEXTLEVELSTART;
      int32_t STARTPOS[3];
      int32_t GAMEINFO;
      int32_t PL_LIGHT;
      int32_t PL_LIGHTVAL;
      ptr32_t WLDTIMER;
      int32_t TIMESTEP;
      int32_t SINGLESTEP;
      int32_t GUILDS;
      int32_t INFOMAN;
      int32_t NEWSMAN;
      int32_t SVMMAN;
      int32_t TRADEMAN;
      int32_t PORTALMAN;
      int32_t SPAWNMAN;
      int32_t MUSIC_DELAY;
      int32_t WATCHNPC;
      int32_t M_BWORLDENTERED;
      int32_t M_FENTERWORLDTIMER;
      int32_t INITIAL_HOUR;
      int32_t INITIAL_MINUTE;
      int32_t DEBUGINSTANCES_ARRAY;
      int32_t DEBUGINSTANCES_NUMALLOC;
      int32_t DEBUGINSTANCES_NUMINARRAY;
      int32_t DEBUGCHANNELS;
      int32_t DEBUGALLINSTANCES;
      int32_t OLDROUTINEDAY;
      int32_t OBJROUTINELIST_COMPAREFUNC;
      int32_t OBJROUTINELIST_DATA;
      int32_t OBJROUTINELIST_NEXT;
      int32_t CURRENTOBJECTROUTINE;
      int32_t PROGRESSBAR;
      int32_t VISUALLIST_ARRAY;
      int32_t VISUALLIST_NUMALLOC;
      int32_t VISUALLIST_NUMINARRAY;
      };

    struct oWorld {
      int32_t _VTBL;
      int32_t _ZCOBJECT_REFCTR;
      int32_t _ZCOBJECT_HASHINDEX;
      int32_t _ZCOBJECT_HASHNEXT;
      zString _ZCOBJECT_OBJECTNAME;
      int32_t GLOBALVOBTREE_PARENT;
      int32_t GLOBALVOBTREE_FIRSTCHILD;
      int32_t GLOBALVOBTREE_NEXT;
      int32_t GLOBALVOBTREE_PREV;
      int32_t GLOBALVOBTREE_DATA;
      int32_t FOUNDHIT;
      int32_t FOUNDVOB;
      int32_t FOUNDPOLY;
      int32_t FOUNDINTERSECTION[3];
      int32_t FOUNDPOLYNORMAL[3];
      int32_t FOUNDVERTEX;
      int32_t OWNERSESSION;
      int32_t CSPLAYER;
      zString M_STRLEVELNAME;
      int32_t COMPILED;
      int32_t COMPILEDEDITORMODE;
      int32_t TRACERAYIGNOREVOBFLAG;
      int32_t M_BISINVENTORYWORLD;
      int32_t WORLDRENDERMODE;
      ptr32_t WAYNET;
      int32_t MASTERFRAMECTR;
      int32_t VOBFARCLIPZ;
      int32_t VOBFARCLIPZSCALABILITY;
      int32_t TRACERAYVOBLIST_ARRAY;
      int32_t TRACERAYVOBLIST_NUMALLOC;
      int32_t TRACERAYVOBLIST_NUMINARRAY;
      int32_t TRACERAYTEMPIGNOREVOBLIST_ARRAY;
      int32_t TRACERAYTEMPIGNOREVOBLIST_NUMALLOC;
      int32_t TRACERAYTEMPIGNOREVOBLIST_NUMINARRAY;
      int32_t RENDERINGFIRSTTIME;
      int32_t SHOWWAYNET;
      int32_t SHOWTRACERAYLINES;
      int32_t PROGRESSBAR;
      int32_t UNARCHIVEFILELEN;
      int32_t UNARCHIVESTARTPOSVOBTREE;
      int32_t NUMVOBSINWORLD;
      int32_t PERFRAMECALLBACKLIST_ARRAY;
      int32_t PERFRAMECALLBACKLIST_NUMALLOC;
      int32_t PERFRAMECALLBACKLIST_NUMINARRAY;
      int32_t SKYCONTROLERINDOOR;
      ptr32_t SKYCONTROLEROUTDOOR;
      int32_t ACTIVESKYCONTROLER;
      int32_t ZONEGLOBALLIST_ARRAY;
      int32_t ZONEGLOBALLIST_NUMALLOC;
      int32_t ZONEGLOBALLIST_NUMINARRAY;
      int32_t ZONEACTIVELIST_ARRAY;
      int32_t ZONEACTIVELIST_NUMALLOC;
      int32_t ZONEACTIVELIST_NUMINARRAY;
      int32_t ZONEACTIVELIST_COMPARE;
      int32_t ZONELASTCLASSLIST_ARRAY;
      int32_t ZONELASTCLASSLIST_NUMALLOC;
      int32_t ZONELASTCLASSLIST_NUMINARRAY;
      int32_t ZONELASTCLASSLIST_COMPARE;
      int32_t ZONEBOXSORTER_VTBL;
      int32_t ZONEBOXSORTER_HANDLES_ARRAY;
      int32_t ZONEBOXSORTER_HANDLES_NUMALLOC;
      int32_t ZONEBOXSORTER_HANDLES_NUMINARRAY;
      int32_t ZONEBOXSORTER_NODELIST0_ARRAY;
      int32_t ZONEBOXSORTER_NODELIST0_NUMALLOC;
      int32_t ZONEBOXSORTER_NODELIST0_NUMINARRAY;
      int32_t ZONEBOXSORTER_NODELIST0_COMPARE;
      int32_t ZONEBOXSORTER_NODELIST1_ARRAY;
      int32_t ZONEBOXSORTER_NODELIST1_NUMALLOC;
      int32_t ZONEBOXSORTER_NODELIST1_NUMINARRAY;
      int32_t ZONEBOXSORTER_NODELIST1_COMPARE;
      int32_t ZONEBOXSORTER_NODELIST2_ARRAY;
      int32_t ZONEBOXSORTER_NODELIST2_NUMALLOC;
      int32_t ZONEBOXSORTER_NODELIST2_NUMINARRAY;
      int32_t ZONEBOXSORTER_NODELIST2_COMPARE;
      int32_t ZONEBOXSORTER_SORTED;
      int32_t ZONEACTIVEHANDLE_VTBL;
      int32_t ZONEACTIVEHANDLE_MYSORTER;
      int32_t ZONEACTIVEHANDLE_MINS[3];
      int32_t ZONEACTIVEHANDLE_MAXS[3];
      int32_t ZONEACTIVEHANDLE_INDEXBEGIN[3];
      int32_t ZONEACTIVEHANDLE_INDEXEND[3];
      int32_t ZONEACTIVEHANDLE_ACTIVELIST_ARRAY;
      int32_t ZONEACTIVEHANDLE_ACTIVELIST_NUMALLOC;
      int32_t ZONEACTIVEHANDLE_ACTIVELIST_NUMINARRAY;
      int32_t ADDZONESTOWORLD;
      int32_t SHOWZONESDEBUGINFO;
      int32_t CBSPTREE;
      int32_t BSPTREE_ACTNODEPTR;
      int32_t BSPTREE_ACTLEAFPTR;
      int32_t BSPTREE_BSPROOT;
      int32_t BSPTREE_MESH;
      int32_t BSPTREE_TREEPOLYLIST;
      int32_t BSPTREE_NODELIST;
      int32_t BSPTREE_LEAFLIST;
      int32_t BSPTREE_NUMNODES;
      int32_t BSPTREE_NUMLEAFS;
      int32_t BSPTREE_NUMPOLYS;
      int32_t BSPTREE_RENDERVOBLIST_ARRAY;
      int32_t BSPTREE_RENDERVOBLIST_NUMALLOC;
      int32_t BSPTREE_RENDERVOBLIST_NUMINARRAY;
      int32_t BSPTREE_RENDERLIGHTLIST_ARRAY;
      int32_t BSPTREE_RENDERLIGHTLIST_NUMALLOC;
      int32_t BSPTREE_RENDERLIGHTLIST_NUMINARRAY;
      int32_t BSPTREE_SECTORLIST_ARRAY;
      int32_t BSPTREE_SECTORLIST_NUMALLOC;
      int32_t BSPTREE_SECTORLIST_NUMINARRAY;
      int32_t BSPTREE_PORTALLIST_ARRAY;
      int32_t BSPTREE_PORTALLIST_NUMALLOC;
      int32_t BSPTREE_PORTALLIST_NUMINARRAY;
      int32_t BSPTREE_BSPTREEMODE;
      int32_t BSPTREE_WORLDRENDERMODE;
      int32_t BSPTREE_VOBFARCLIPZ;
      int32_t BSPTREE_VOBFARPLANE[4];
      int32_t BSPTREE_VOBFARPLANESIGNBITS;
      int32_t BSPTREE_DRAWVOBBBOX3D;
      int32_t BSPTREE_LEAFSRENDERED;
      int32_t BSPTREE_VOBSRENDERED;
      int32_t BSPTREE_M_BRENDEREDFIRSTTIME;
      int32_t BSPTREE_MASTERFRAMECTR;
      int32_t BSPTREE_ACTPOLYPTR;
      int32_t BSPTREE_COMPILED;
      int32_t ACTIVEVOBLIST_ARRAY;
      int32_t ACTIVEVOBLIST_NUMALLOC;
      int32_t ACTIVEVOBLIST_NUMINARRAY;
      int32_t WALKLIST_ARRAY;
      int32_t WALKLIST_NUMALLOC;
      int32_t WALKLIST_NUMINARRAY;
      int32_t VOBHASHTABLESTART[2048];
      int32_t VOBHASHTABLEMIDDLE[2048];
      int32_t VOBHASHTABLEEND[2048];
      zString WORLDFILENAME;
      zString WORLDNAME;
      int32_t VOBLIST;
      int32_t VOBLIST_NPCS;
      int32_t VOBLIST_ITEMS;
      };

    struct GameMgr {
      };

    struct zCTimer {
      int32_t FACTORMOTION;
      int32_t FRAMETIMEFLOAT;
      int32_t TOTALTIMEFLOAT;
      int32_t FRAMETIMEFLOATSECS;
      int32_t TOTALTIMEFLOATSECS;
      int32_t LASTTIMER;
      int32_t FRAMETIME;
      int32_t TOTALTIME;
      int32_t MINFRAMETIME;
      int32_t FORCEDMAXFRAMETIME;
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

    static int  mkf(int v);
    static int  truncf(int v);
    static int  roundf(int v);
    static int  addf(int a, int b);
    static int  subf(int a, int b);
    static int  mulf(int a, int b);
    static int  divf(int a, int b);

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
    // pointers
    std::shared_ptr<zenkit::DaedalusInstance> mem_ptrtoinst(ptr32_t address);

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
    zenkit::DaedalusNakedCall repeat   (zenkit::DaedalusVm& vm);
    zenkit::DaedalusNakedCall while_   (zenkit::DaedalusVm& vm);
    void                      loop_trap(zenkit::DaedalusSymbol* i);
    void                      loop_out (zenkit::DaedalusVm& vm);

    void call_zstringptrparam(std::string_view ptr);
    void call_intparam(int p);
    void call__thiscall(int32_t pthis, ptr32_t func);
    void call__stdcall(ptr32_t func);
    void register_stdcall_inner(ptr32_t addr, std::function<void(Ikarus&)> f);

    int  hash(int x);

    zenkit::DaedalusVm& vm;
    Mem32               allocator;
    
    struct Loop {
      uint32_t                pc = 0;
      zenkit::DaedalusSymbol* i  = nullptr;
      int32_t                 loopLen = 0;
      };
    std::vector<Loop> loop_start;

    struct Call {
      std::vector<int>         iprm;
      std::vector<std::string> sprm;

      template<class T>
      T pop();

      template<>
      int pop<int>() {
        if(iprm.size()==0)
          return 0;
        auto ret = iprm.back();
        iprm.pop_back();
        return ret;
        }

      template<>
      ptr32_t pop<ptr32_t>() {
        if(iprm.size()==0)
          return 0;
        auto ret = iprm.back();
        iprm.pop_back();
        return ptr32_t(ret);
        }

      template<>
      std::string pop<std::string>() {
        if(sprm.size()==0)
          return 0;
        auto ret = std::move(sprm.back());
        sprm.pop_back();
        return std::string(ret);
        }
      };
    Call         call;
    std::unordered_map<ptr32_t, std::function<void(Ikarus&)>> stdcall_overrides;

    uint32_t     versionHint   = 0;
    zCParser     parserProxy;

    zCTimer      zTimer = {};

    ptr32_t      oGame_Pointer = 0;
    oGame        memGame;

    ptr32_t      gameman_Ptr = 0;

    ptr32_t      symbolsPtr = 0;

    int32_t      invMaxItems = 9;

  friend class LeGo;
  };

