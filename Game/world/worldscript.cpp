#include "worldscript.h"

#include "gothic.h"
#include "npc.h"
#include "item.h"

#include <fstream>
#include <Tempest/Log>

using namespace Tempest;
using namespace Daedalus::GameState;

static std::string addExt(const std::string& s,const char* ext){
  if(s.size()>0 && s.back()=='.')
    return s+&ext[1];
  return s+ext;
  }

struct WorldScript::ScopeVar final {
  ScopeVar(Daedalus::DaedalusVM& vm,const char* name,Npc& n)
    :ScopeVar(vm,name, ZMemory::toBigHandle(n.handle()), Daedalus::IC_Npc){
    }

  ScopeVar(Daedalus::DaedalusVM& vm,const char* name,Npc* n)
    :ScopeVar(vm,name,ZMemory::toBigHandle(n ? n->handle() : NpcHandle()), Daedalus::IC_Npc){
    }

  ScopeVar(Daedalus::DaedalusVM& vm,const char* name, ZMemory::BigHandle h, Daedalus::EInstanceClass instanceClass)
    :vm(vm),name(name) {
    Daedalus::PARSymbol& s = vm.getDATFile().getSymbolByName(name);
    cls    = s.instanceDataClass;
    handle = s.instanceDataHandle;
    s.instanceDataHandle = h;
    s.instanceDataClass  = instanceClass;
    }
  ScopeVar(const ScopeVar&)=delete;
  ~ScopeVar(){
    Daedalus::PARSymbol& s = vm.getDATFile().getSymbolByName(name);
    s.instanceDataHandle = handle;
    s.instanceDataClass  = cls;
    }

  ZMemory::BigHandle       handle;
  Daedalus::EInstanceClass cls;
  Daedalus::DaedalusVM&    vm;
  const char*              name="";
  };


WorldScript::WorldScript(World &owner, Gothic& gothic, const char *world)
  :vm(gothic.path()+world),owner(owner){
  Daedalus::registerGothicEngineClasses(vm);
  initCommon();
  }

void WorldScript::initCommon() {
  auto& tbl = vm.getDATFile().getSymTable();
  for(auto& i:tbl.symbolsByName) {
    if(i.first.size()==0 || std::isdigit(i.first[0]))
      continue;
    //Log::d(i.first);
    vm.registerExternalFunction(i.first,[this](Daedalus::DaedalusVM& vm){return notImplementedRoutine(vm);});
    }

  vm.registerExternalFunction("concatstrings", &WorldScript::concatstrings);
  vm.registerExternalFunction("inttostring",   &WorldScript::inttostring  );
  vm.registerExternalFunction("floattostring", &WorldScript::floattostring);
  vm.registerExternalFunction("inttofloat",    &WorldScript::inttofloat   );
  vm.registerExternalFunction("floattoint",    &WorldScript::floattoint   );

  vm.registerExternalFunction("hlp_random",          [this](Daedalus::DaedalusVM& vm){ hlp_random(vm);         });
  vm.registerExternalFunction("hlp_strcmp",          &WorldScript::hlp_strcmp                                   );
  vm.registerExternalFunction("hlp_isvalidnpc",      [this](Daedalus::DaedalusVM& vm){ hlp_isvalidnpc(vm);     });
  vm.registerExternalFunction("hlp_getinstanceid",   [this](Daedalus::DaedalusVM& vm){ hlp_getinstanceid(vm);  });
  vm.registerExternalFunction("hlp_getnpc",          [this](Daedalus::DaedalusVM& vm){ hlp_getnpc(vm);         });
  vm.registerExternalFunction("hlp_isvaliditem",     [this](Daedalus::DaedalusVM& vm){ hlp_isvaliditem(vm);    });
  vm.registerExternalFunction("hlp_isitem",          [this](Daedalus::DaedalusVM& vm){ hlp_isitem(vm);         });

  vm.registerExternalFunction("wld_insertnpc",       [this](Daedalus::DaedalusVM& vm){ wld_insertnpc(vm);  });
  vm.registerExternalFunction("wld_insertitem",      [this](Daedalus::DaedalusVM& vm){ wld_insertitem(vm); });
  vm.registerExternalFunction("wld_settime",         [this](Daedalus::DaedalusVM& vm){ wld_settime(vm);    });
  vm.registerExternalFunction("wld_getday",          [this](Daedalus::DaedalusVM& vm){ wld_getday(vm);     });
  vm.registerExternalFunction("wld_playeffect",      [this](Daedalus::DaedalusVM& vm){ wld_playeffect(vm); });
  vm.registerExternalFunction("wld_stopeffect",      [this](Daedalus::DaedalusVM& vm){ wld_stopeffect(vm); });
  vm.registerExternalFunction("wld_getplayerportalguild",
                                                     [this](Daedalus::DaedalusVM& vm){ wld_getplayerportalguild(vm); });
  vm.registerExternalFunction("wld_setguildattitude",[this](Daedalus::DaedalusVM& vm){ wld_setguildattitude(vm);     });
  vm.registerExternalFunction("wld_getguildattitude",[this](Daedalus::DaedalusVM& vm){ wld_getguildattitude(vm);     });
  vm.registerExternalFunction("wld_istime",          [this](Daedalus::DaedalusVM& vm){ wld_istime(vm);               });
  vm.registerExternalFunction("wld_isfpavailable",   [this](Daedalus::DaedalusVM& vm){ wld_isfpavailable(vm);        });
  vm.registerExternalFunction("wld_ismobavailable",  [this](Daedalus::DaedalusVM& vm){ wld_ismobavailable(vm);       });
  vm.registerExternalFunction("wld_setmobroutine",   [this](Daedalus::DaedalusVM& vm){ wld_setmobroutine(vm);        });
  vm.registerExternalFunction("wld_assignroomtoguild",
                                                     [this](Daedalus::DaedalusVM& vm){ wld_assignroomtoguild(vm);    });

  vm.registerExternalFunction("mdl_setvisual",       [this](Daedalus::DaedalusVM& vm){ mdl_setvisual(vm);        });
  vm.registerExternalFunction("mdl_setvisualbody",   [this](Daedalus::DaedalusVM& vm){ mdl_setvisualbody(vm);    });
  vm.registerExternalFunction("mdl_setmodelfatness", [this](Daedalus::DaedalusVM& vm){ mdl_setmodelfatness(vm);  });
  vm.registerExternalFunction("mdl_applyoverlaymds", [this](Daedalus::DaedalusVM& vm){ mdl_applyoverlaymds(vm);  });
  vm.registerExternalFunction("mdl_applyoverlaymdstimed",
                                                     [this](Daedalus::DaedalusVM& vm){ mdl_applyoverlaymdstimed(vm); });
  vm.registerExternalFunction("mdl_removeoverlaymds",[this](Daedalus::DaedalusVM& vm){ mdl_removeoverlaymds(vm); });
  vm.registerExternalFunction("mdl_setmodelscale",   [this](Daedalus::DaedalusVM& vm){ mdl_setmodelscale(vm);    });
  vm.registerExternalFunction("mdl_startfaceani",    [this](Daedalus::DaedalusVM& vm){ mdl_startfaceani(vm);     });

  vm.registerExternalFunction("npc_settofightmode",  [this](Daedalus::DaedalusVM& vm){ npc_settofightmode(vm);   });
  vm.registerExternalFunction("npc_settofistmode",   [this](Daedalus::DaedalusVM& vm){ npc_settofistmode(vm);    });
  vm.registerExternalFunction("npc_isinstate",       [this](Daedalus::DaedalusVM& vm){ npc_isinstate(vm);        });
  vm.registerExternalFunction("npc_wasinstate",      [this](Daedalus::DaedalusVM& vm){ npc_wasinstate(vm);       });
  vm.registerExternalFunction("npc_getdisttowp",     [this](Daedalus::DaedalusVM& vm){ npc_getdisttowp(vm);      });
  vm.registerExternalFunction("npc_exchangeroutine", [this](Daedalus::DaedalusVM& vm){ npc_exchangeroutine(vm);  });
  vm.registerExternalFunction("npc_isdead",          [this](Daedalus::DaedalusVM& vm){ npc_isdead(vm);           });
  vm.registerExternalFunction("npc_knowsinfo",       [this](Daedalus::DaedalusVM& vm){ npc_knowsinfo(vm);        });
  vm.registerExternalFunction("npc_settalentskill",  [this](Daedalus::DaedalusVM& vm){ npc_settalentskill(vm);   });
  vm.registerExternalFunction("npc_gettalentskill",  [this](Daedalus::DaedalusVM& vm){ npc_gettalentskill(vm);   });
  vm.registerExternalFunction("npc_settalentvalue",  [this](Daedalus::DaedalusVM& vm){ npc_settalentvalue(vm);   });
  vm.registerExternalFunction("npc_gettalentvalue",  [this](Daedalus::DaedalusVM& vm){ npc_gettalentvalue(vm);   });
  vm.registerExternalFunction("npc_setrefusetalk",   [this](Daedalus::DaedalusVM& vm){ npc_setrefusetalk(vm);    });
  vm.registerExternalFunction("npc_refusetalk",      [this](Daedalus::DaedalusVM& vm){ npc_refusetalk(vm);       });
  vm.registerExternalFunction("npc_hasitems",        [this](Daedalus::DaedalusVM& vm){ npc_hasitems(vm);         });
  vm.registerExternalFunction("npc_removeinvitems",  [this](Daedalus::DaedalusVM& vm){ npc_removeinvitems(vm);   });
  vm.registerExternalFunction("npc_getbodystate",    [this](Daedalus::DaedalusVM& vm){ npc_getbodystate(vm);     });
  vm.registerExternalFunction("npc_getlookattarget", [this](Daedalus::DaedalusVM& vm){ npc_getlookattarget(vm);  });
  vm.registerExternalFunction("npc_getdisttonpc",    [this](Daedalus::DaedalusVM& vm){ npc_getdisttonpc(vm);     });
  vm.registerExternalFunction("npc_hasequippedarmor",[this](Daedalus::DaedalusVM& vm){ npc_hasequippedarmor(vm); });
  vm.registerExternalFunction("npc_getattitude",     [this](Daedalus::DaedalusVM& vm){ npc_getattitude(vm);      });
  vm.registerExternalFunction("npc_setperctime",     [this](Daedalus::DaedalusVM& vm){ npc_setperctime(vm);      });
  vm.registerExternalFunction("npc_percenable",      [this](Daedalus::DaedalusVM& vm){ npc_percenable(vm);       });
  vm.registerExternalFunction("npc_percdisable",     [this](Daedalus::DaedalusVM& vm){ npc_percdisable(vm);      });
  vm.registerExternalFunction("npc_getnearestwp",    [this](Daedalus::DaedalusVM& vm){ npc_getnearestwp(vm);     });
  vm.registerExternalFunction("npc_clearaiqueue",    [this](Daedalus::DaedalusVM& vm){ npc_clearaiqueue(vm);     });
  vm.registerExternalFunction("npc_isplayer",        [this](Daedalus::DaedalusVM& vm){ npc_isplayer(vm);         });
  vm.registerExternalFunction("npc_getstatetime",    [this](Daedalus::DaedalusVM& vm){ npc_getstatetime(vm);     });
  vm.registerExternalFunction("npc_setstatetime",    [this](Daedalus::DaedalusVM& vm){ npc_setstatetime(vm);     });
  vm.registerExternalFunction("npc_changeattribute", [this](Daedalus::DaedalusVM& vm){ npc_changeattribute(vm);  });
  vm.registerExternalFunction("npc_isonfp",          [this](Daedalus::DaedalusVM& vm){ npc_isonfp(vm);           });
  vm.registerExternalFunction("npc_getheighttonpc",  [this](Daedalus::DaedalusVM& vm){ npc_getheighttonpc(vm);   });
  vm.registerExternalFunction("npc_getequippedmeleweapon",
                                                     [this](Daedalus::DaedalusVM& vm){ npc_getequippedmeleweapon(vm); });
  vm.registerExternalFunction("npc_getequippedrangedweapon",
                                                     [this](Daedalus::DaedalusVM& vm){ npc_getequippedrangedweapon(vm); });
  vm.registerExternalFunction("npc_getequippedarmor",[this](Daedalus::DaedalusVM& vm){ npc_getequippedarmor(vm); });
  vm.registerExternalFunction("npc_canseenpc",       [this](Daedalus::DaedalusVM& vm){ npc_canseenpc(vm);        });
  vm.registerExternalFunction("npc_hasequippedmeleeweapon",
                                                     [this](Daedalus::DaedalusVM& vm){ npc_hasequippedmeleeweapon(vm); });
  vm.registerExternalFunction("npc_hasequippedrangedweapon",
                                                     [this](Daedalus::DaedalusVM& vm){ npc_hasequippedrangedweapon(vm); });
  vm.registerExternalFunction("npc_getactivespell",  [this](Daedalus::DaedalusVM& vm){ npc_getactivespell(vm);   });
  vm.registerExternalFunction("npc_getactivespellisscroll",
                                                     [this](Daedalus::DaedalusVM& vm){ npc_getactivespellisscroll(vm); });
  vm.registerExternalFunction("npc_canseenpcfreelos",[this](Daedalus::DaedalusVM& vm){ npc_canseenpcfreelos(vm); });
  vm.registerExternalFunction("npc_isinfightmode",   [this](Daedalus::DaedalusVM& vm){ npc_isinfightmode(vm);    });
  vm.registerExternalFunction("npc_settarget",       [this](Daedalus::DaedalusVM& vm){ npc_settarget(vm);        });
  vm.registerExternalFunction("npc_gettarget",       [this](Daedalus::DaedalusVM& vm){ npc_gettarget(vm);        });
  vm.registerExternalFunction("npc_sendpassiveperc", [this](Daedalus::DaedalusVM& vm){ npc_sendpassiveperc(vm);  });
  vm.registerExternalFunction("npc_checkinfo",       [this](Daedalus::DaedalusVM& vm){ npc_checkinfo(vm);        });
  vm.registerExternalFunction("npc_getportalguild",  [this](Daedalus::DaedalusVM& vm){ npc_getportalguild(vm);   });
  vm.registerExternalFunction("npc_isinplayersroom", [this](Daedalus::DaedalusVM& vm){ npc_isinplayersroom(vm);  });
  vm.registerExternalFunction("npc_getreadiedweapon",[this](Daedalus::DaedalusVM& vm){ npc_getreadiedweapon(vm); });
  vm.registerExternalFunction("npc_isdrawingspell",  [this](Daedalus::DaedalusVM& vm){ npc_isdrawingspell(vm);   });
  vm.registerExternalFunction("npc_perceiveall",     [this](Daedalus::DaedalusVM& vm){ npc_perceiveall(vm);      });
  vm.registerExternalFunction("npc_stopani",         [this](Daedalus::DaedalusVM& vm){ npc_stopani(vm);          });

  vm.registerExternalFunction("ai_output",           [this](Daedalus::DaedalusVM& vm){ ai_output(vm);            });
  vm.registerExternalFunction("ai_stopprocessinfos", [this](Daedalus::DaedalusVM& vm){ ai_stopprocessinfos(vm);  });
  vm.registerExternalFunction("ai_processinfos",     [this](Daedalus::DaedalusVM& vm){ ai_processinfos(vm);      });
  vm.registerExternalFunction("ai_standup",          [this](Daedalus::DaedalusVM& vm){ ai_standup(vm);           });
  vm.registerExternalFunction("ai_standupquick",     [this](Daedalus::DaedalusVM& vm){ ai_standupquick(vm);      });
  vm.registerExternalFunction("ai_continueroutine",  [this](Daedalus::DaedalusVM& vm){ ai_continueroutine(vm);   });
  vm.registerExternalFunction("ai_printscreen",      [this](Daedalus::DaedalusVM& vm){ printscreen(vm);          });
  vm.registerExternalFunction("ai_stoplookat",       [this](Daedalus::DaedalusVM& vm){ ai_stoplookat(vm);        });
  vm.registerExternalFunction("ai_lookatnpc",        [this](Daedalus::DaedalusVM& vm){ ai_lookatnpc(vm);         });
  vm.registerExternalFunction("ai_removeweapon",     [this](Daedalus::DaedalusVM& vm){ ai_removeweapon(vm);      });
  vm.registerExternalFunction("ai_turntonpc",        [this](Daedalus::DaedalusVM& vm){ ai_turntonpc(vm);         });
  vm.registerExternalFunction("ai_outputsvm",        [this](Daedalus::DaedalusVM& vm){ ai_outputsvm(vm);         });
  vm.registerExternalFunction("ai_outputsvm_overlay",[this](Daedalus::DaedalusVM& vm){ ai_outputsvm_overlay(vm); });
  vm.registerExternalFunction("ai_startstate",       [this](Daedalus::DaedalusVM& vm){ ai_startstate(vm);        });
  vm.registerExternalFunction("ai_playani",          [this](Daedalus::DaedalusVM& vm){ ai_playani(vm);           });
  vm.registerExternalFunction("ai_setwalkmode",      [this](Daedalus::DaedalusVM& vm){ ai_setwalkmode(vm);       });
  vm.registerExternalFunction("ai_wait",             [this](Daedalus::DaedalusVM& vm){ ai_wait(vm);              });
  vm.registerExternalFunction("ai_waitms",           [this](Daedalus::DaedalusVM& vm){ ai_waitms(vm);            });
  vm.registerExternalFunction("ai_aligntowp",        [this](Daedalus::DaedalusVM& vm){ ai_aligntowp(vm);         });
  vm.registerExternalFunction("ai_gotowp",           [this](Daedalus::DaedalusVM& vm){ ai_gotowp(vm);            });
  vm.registerExternalFunction("ai_gotofp",           [this](Daedalus::DaedalusVM& vm){ ai_gotofp(vm);            });
  vm.registerExternalFunction("ai_playanibs",        [this](Daedalus::DaedalusVM& vm){ ai_playanibs(vm);         });
  vm.registerExternalFunction("ai_equiparmor",       [this](Daedalus::DaedalusVM& vm){ ai_equiparmor(vm);        });
  vm.registerExternalFunction("ai_equipbestmeleeweapon",
                                                     [this](Daedalus::DaedalusVM& vm){ ai_equipbestmeleeweapon(vm);  });
  vm.registerExternalFunction("ai_equipbestrangedweapon",
                                                     [this](Daedalus::DaedalusVM& vm){ ai_equipbestrangedweapon(vm); });
  vm.registerExternalFunction("ai_usemob",           [this](Daedalus::DaedalusVM& vm){ ai_usemob(vm);            });
  vm.registerExternalFunction("ai_teleport",         [this](Daedalus::DaedalusVM& vm){ ai_teleport(vm);          });
  vm.registerExternalFunction("ai_stoppointat",      [this](Daedalus::DaedalusVM& vm){ ai_stoppointat(vm);       });
  vm.registerExternalFunction("ai_readymeleeweapon", [this](Daedalus::DaedalusVM& vm){ ai_readymeleeweapon(vm);  });
  vm.registerExternalFunction("ai_readyrangedweapon",[this](Daedalus::DaedalusVM& vm){ ai_readyrangedweapon(vm); });
  vm.registerExternalFunction("ai_readyspell",       [this](Daedalus::DaedalusVM& vm){ ai_readyspell(vm);        });
  vm.registerExternalFunction("ai_attack",           [this](Daedalus::DaedalusVM& vm){ ai_atack(vm);             });
  vm.registerExternalFunction("ai_flee",             [this](Daedalus::DaedalusVM& vm){ ai_flee(vm);              });
  vm.registerExternalFunction("ai_dodge",            [this](Daedalus::DaedalusVM& vm){ ai_dodge(vm);             });
  vm.registerExternalFunction("ai_unequipweapons",   [this](Daedalus::DaedalusVM& vm){ ai_unequipweapons(vm);    });
  vm.registerExternalFunction("ai_gotonpc",          [this](Daedalus::DaedalusVM& vm){ ai_gotonpc(vm);           });

  vm.registerExternalFunction("mob_hasitems",        [this](Daedalus::DaedalusVM& vm){ mob_hasitems(vm);         });

  vm.registerExternalFunction("ta_min",              [this](Daedalus::DaedalusVM& vm){ ta_min(vm);               });

  vm.registerExternalFunction("log_createtopic",     [this](Daedalus::DaedalusVM& vm){ log_createtopic(vm);      });
  vm.registerExternalFunction("log_settopicstatus",  [this](Daedalus::DaedalusVM& vm){ log_settopicstatus(vm);   });
  vm.registerExternalFunction("log_addentry",        [this](Daedalus::DaedalusVM& vm){ log_addentry(vm);         });

  vm.registerExternalFunction("equipitem",           [this](Daedalus::DaedalusVM& vm){ equipitem(vm);            });
  vm.registerExternalFunction("createinvitem",       [this](Daedalus::DaedalusVM& vm){ createinvitem(vm);        });
  vm.registerExternalFunction("createinvitems",      [this](Daedalus::DaedalusVM& vm){ createinvitems(vm);       });

  vm.registerExternalFunction("info_addchoice",      [this](Daedalus::DaedalusVM& vm){ info_addchoice(vm);       });
  vm.registerExternalFunction("info_clearchoices",   [this](Daedalus::DaedalusVM& vm){ info_clearchoices(vm);    });
  vm.registerExternalFunction("infomanager_hasfinished",
                                                     [this](Daedalus::DaedalusVM& vm){ infomanager_hasfinished(vm); });

  vm.registerExternalFunction("introducechapter",    [this](Daedalus::DaedalusVM& vm){ introducechapter(vm);     });
  vm.registerExternalFunction("playvideo",           [this](Daedalus::DaedalusVM& vm){ playvideo(vm);            });
  vm.registerExternalFunction("printscreen",         [this](Daedalus::DaedalusVM& vm){ printscreen(vm);          });
  vm.registerExternalFunction("print",               [this](Daedalus::DaedalusVM& vm){ print(vm);                });
  vm.registerExternalFunction("perc_setrange",       [this](Daedalus::DaedalusVM& vm){ perc_setrange(vm);        });

  DaedalusGameState::GameExternals ext;
  ext.wld_insertnpc      = [this](NpcHandle h,const std::string& s){ owner.onInserNpc(h,s); };
  ext.post_wld_insertnpc = [this](NpcHandle h){ onNpcReady(h); };

  ext.wld_insertitem     = notImplementedFn<void,ItemHandle>();
  ext.wld_removenpc      = notImplementedFn<void,NpcHandle>();
  ext.log_createtopic    = notImplementedFn<void,std::string>();
  ext.log_settopicstatus = notImplementedFn<void,std::string>();
  ext.log_addentry       = notImplementedFn<void,std::string,std::string>();
  vm.getGameState().setGameExternals(ext);

  spellFxInstanceNames = vm.getDATFile().getSymbolIndexByName("spellFxInstanceNames");
  spellFxAniLetters    = vm.getDATFile().getSymbolIndexByName("spellFxAniLetters");

  cFocusNorm  = getFocus("Focus_Normal");
  cFocusMele  = getFocus("Focus_Melee");
  cFocusRange = getFocus("Focus_Ranged");
  cFocusMage  = getFocus("Focus_Magic");

  ZS_Dead         = getAiState(getSymbolIndex("ZS_Dead")).funcIni;
  ZS_Unconscious  = getAiState(getSymbolIndex("ZS_Unconscious")).funcIni;

  auto& currency  = vm.getDATFile().getSymbolByName("TRADE_CURRENCY_INSTANCE");
  itMi_Gold       = vm.getDATFile().getSymbolIndexByName(currency.getString(0));
  if(itMi_Gold>0){ // FIXME
    InfoHandle h = vm.getGameState().createItem();
    vm.initializeInstance(ZMemory::toBigHandle(h), itMi_Gold, Daedalus::IC_Item);
    auto& it = vmItem(h);
    goldTxt = it.name;
    vm.getGameState().removeItem(h);
    }
  auto& tradeMul = vm.getDATFile().getSymbolByName("TRADE_VALUE_MULTIPLIER");
  tradeValMult   = tradeMul.getFloat();

  auto& vtime     = vm.getDATFile().getSymbolByName("VIEW_TIME_PER_CHAR");
  viewTimePerChar = vtime.getFloat(0);
  if(viewTimePerChar<=0.f)
    viewTimePerChar=0.55f;

  auto gilMax = vm.getDATFile().getSymbolByName("GIL_MAX");
  gilCount=size_t(gilMax.getInt(0));

  auto tableSz = vm.getDATFile().getSymbolByName("TAB_ANZAHL");
  auto guilds  = vm.getDATFile().getSymbolByName("GIL_ATTITUDES");
  gilAttitudes.resize(gilCount*gilCount,ATT_HOSTILE);

  size_t tbSz=size_t(std::sqrt(tableSz.getInt()));
  for(size_t i=0;i<tbSz;++i)
    for(size_t r=0;r<tbSz;++r) {
      gilAttitudes[i*gilCount+r]=guilds.getInt(i*tbSz+r);
      gilAttitudes[r*gilCount+i]=guilds.getInt(r*tbSz+i);
      }


  auto id = vm.getDATFile().getSymbolIndexByName("Gil_Values");
  if(id!=0){
    auto gil = vm.getGameState().createGilValues();
    vm.initializeInstance(ZMemory::toBigHandle(gil), id, Daedalus::IC_GilValues);
    cGuildVal = vm.getGameState().getGilValues(gil);

    for(size_t i=0;i<Guild::GIL_PUBLIC;++i){
      cGuildVal.water_depth_knee   [i]=cGuildVal.water_depth_knee   [Guild::GIL_HUMAN];
      cGuildVal.water_depth_chest  [i]=cGuildVal.water_depth_chest  [Guild::GIL_HUMAN];
      cGuildVal.jumpup_height      [i]=cGuildVal.jumpup_height      [Guild::GIL_HUMAN];
      cGuildVal.swim_time          [i]=cGuildVal.swim_time          [Guild::GIL_HUMAN];
      cGuildVal.dive_time          [i]=cGuildVal.dive_time          [Guild::GIL_HUMAN];
      cGuildVal.step_height        [i]=cGuildVal.step_height        [Guild::GIL_HUMAN];
      cGuildVal.jumplow_height     [i]=cGuildVal.jumplow_height     [Guild::GIL_HUMAN];
      cGuildVal.jumpmid_height     [i]=cGuildVal.jumpmid_height     [Guild::GIL_HUMAN];
      cGuildVal.slide_angle        [i]=cGuildVal.slide_angle        [Guild::GIL_HUMAN];
      cGuildVal.slide_angle2       [i]=cGuildVal.slide_angle2       [Guild::GIL_HUMAN];
      cGuildVal.disable_autoroll   [i]=cGuildVal.disable_autoroll   [Guild::GIL_HUMAN];
      cGuildVal.surface_align      [i]=cGuildVal.surface_align      [Guild::GIL_HUMAN];
      cGuildVal.climb_heading_angle[i]=cGuildVal.climb_heading_angle[Guild::GIL_HUMAN];
      cGuildVal.climb_horiz_angle  [i]=cGuildVal.climb_horiz_angle  [Guild::GIL_HUMAN];
      cGuildVal.climb_ground_angle [i]=cGuildVal.climb_ground_angle [Guild::GIL_HUMAN];
      cGuildVal.fight_range_base   [i]=cGuildVal.fight_range_base   [Guild::GIL_HUMAN];
      cGuildVal.fight_range_fist   [i]=cGuildVal.fight_range_fist   [Guild::GIL_HUMAN];
      cGuildVal.fight_range_g      [i]=cGuildVal.fight_range_g      [Guild::GIL_HUMAN];
      cGuildVal.fight_range_1hs    [i]=cGuildVal.fight_range_1hs    [Guild::GIL_HUMAN];
      cGuildVal.fight_range_1ha    [i]=cGuildVal.fight_range_1ha    [Guild::GIL_HUMAN];
      cGuildVal.fight_range_2hs    [i]=cGuildVal.fight_range_2hs    [Guild::GIL_HUMAN];
      cGuildVal.fight_range_2ha    [i]=cGuildVal.fight_range_2ha    [Guild::GIL_HUMAN];
      cGuildVal.falldown_height    [i]=cGuildVal.falldown_height    [Guild::GIL_HUMAN];
      cGuildVal.falldown_damage    [i]=cGuildVal.falldown_damage    [Guild::GIL_HUMAN];
      cGuildVal.blood_disabled     [i]=cGuildVal.blood_disabled     [Guild::GIL_HUMAN];
      cGuildVal.blood_max_distance [i]=cGuildVal.blood_max_distance [Guild::GIL_HUMAN];
      cGuildVal.blood_amount       [i]=cGuildVal.blood_amount       [Guild::GIL_HUMAN];
      cGuildVal.blood_flow         [i]=cGuildVal.blood_flow         [Guild::GIL_HUMAN];
      cGuildVal.blood_emitter      [i]=cGuildVal.blood_emitter      [Guild::GIL_HUMAN];
      cGuildVal.blood_texture      [i]=cGuildVal.blood_texture      [Guild::GIL_HUMAN];
      cGuildVal.turn_speed         [i]=cGuildVal.turn_speed         [Guild::GIL_HUMAN];
      }
    }
  }

void WorldScript::initDialogs(Gothic& gothic) {
  loadDialogOU(gothic);
  if(!dialogs)
    dialogs.reset(new ZenLoad::zCCSLib(""));

  vm.getDATFile().iterateSymbolsOfClass("C_Info", [this](size_t i,Daedalus::PARSymbol&){
    InfoHandle h = vm.getGameState().createInfo();
    // Daedalus::GEngineClasses::C_Info& info = vm.getGameState().getInfo(h);
    vm.initializeInstance(ZMemory::toBigHandle(h), i, Daedalus::IC_Info);
    dialogsInfo.push_back(h);
    });
  }

void WorldScript::loadDialogOU(Gothic &gothic) {
  const char* names[]={"OU.DAT","OU.BIN"};
  const char* dir  []={"_work/data/Scripts/content/CUTSCENE/","_work/DATA/scripts/content/CUTSCENE/"};

  for(auto n:names){
    for(auto d:dir){
      std::string full = gothic.path()+"/"+d+n;
      bool exist=false;
      {
        std::ifstream f(full.c_str());
        exist=f.is_open();
      }
      if(exist){
        dialogs.reset(new ZenLoad::zCCSLib(full));
        return;
        }
      }
    }
  }

Daedalus::GEngineClasses::C_Focus WorldScript::getFocus(const char *name) {
  auto id = vm.getDATFile().getSymbolIndexByName(name);
  if(id==0){
    Daedalus::GEngineClasses::C_Focus r={};
    return r;
    }
  auto foc = vm.getGameState().createFocus();
  vm.initializeInstance(ZMemory::toBigHandle(foc), id, Daedalus::IC_Focus);

  auto ret = vm.getGameState().getFocus(foc);
  //vm.getGameState().remove***(foc); TODO: cleanup
  return ret;
  }

DaedalusGameState &WorldScript::getGameState() {
  return vm.getGameState();
  }

Daedalus::PARSymbol &WorldScript::getSymbol(const char *s) {
  return vm.getDATFile().getSymbolByName(s);
  }

Daedalus::PARSymbol &WorldScript::getSymbol(const size_t s) {
  return vm.getDATFile().getSymbolByIndex(s);
  }

size_t WorldScript::getSymbolIndex(const char* s) {
  return vm.getDATFile().getSymbolIndexByName(s);
  }

size_t WorldScript::getSymbolIndex(const std::string &s) {
  return vm.getDATFile().getSymbolIndexByName(s);
  }

const AiState &WorldScript::getAiState(size_t id) {
  auto it = aiStates.find(id);
  if(it!=aiStates.end())
    return it->second;
  auto ins = aiStates.emplace(id,AiState(*this,id));
  return ins.first->second;
  }

Daedalus::GEngineClasses::C_Npc& WorldScript::vmNpc(Daedalus::GameState::NpcHandle handle) {
  return vm.getGameState().getNpc(handle);
  }

Daedalus::GEngineClasses::C_Item& WorldScript::vmItem(ItemHandle handle) {
  return vm.getGameState().getItem(handle);
  }

std::vector<WorldScript::DlgChoise> WorldScript::dialogChoises(Daedalus::GameState::NpcHandle player,
                                                               Daedalus::GameState::NpcHandle hnpc,
                                                               const std::vector<uint32_t>& except) {
  auto& npc = vmNpc(hnpc);
  auto& pl  = vmNpc(player);

  ScopeVar self (vm,"self", ZMemory::toBigHandle(hnpc),   Daedalus::IC_Npc);
  ScopeVar other(vm,"other",ZMemory::toBigHandle(player), Daedalus::IC_Npc);

  std::vector<InfoHandle> hDialog;
  for(auto& infoHandle : dialogsInfo) {
    Daedalus::GEngineClasses::C_Info& info = vm.getGameState().getInfo(infoHandle);
    if(info.npc==int32_t(npc.instanceSymbol)) {
      hDialog.push_back(infoHandle);
      }
    }

  std::vector<DlgChoise> choise;

  for(int important=1;important>=0;--important){
    for(auto& i:hDialog) {
      const Daedalus::GEngineClasses::C_Info& info = vm.getGameState().getInfo(i);
      if(info.important!=important)
        continue;
      bool npcKnowsInfo = doesNpcKnowInfo(pl,info.instanceSymbol);
      if(npcKnowsInfo && !info.permanent)
        continue;

      if(info.important && info.permanent){
        bool skip=false;
        for(auto i:except)
          if(i==info.information){
            skip=true;
            break;
            }
        if(skip)
          continue;
        }

      bool valid=false;
      if(info.condition)
        valid = runFunction(info.condition)!=0;

      if(valid) {
        DlgChoise ch;
        ch.title    = info.description;
        ch.scriptFn = info.information;
        ch.handle   = i;
        ch.isTrade  = info.trade!=0;
        ch.sort     = info.nr;
        choise.emplace_back(std::move(ch));
        }
      }
    if(!choise.empty()){
      sort(choise);
      return choise;
      }
    }
  sort(choise);
  return choise;
  }

std::vector<WorldScript::DlgChoise> WorldScript::updateDialog(const WorldScript::DlgChoise &dlg, Npc& /*pl*/,Npc&) {
  const Daedalus::GEngineClasses::C_Info& info = getGameState().getInfo(dlg.handle);
  std::vector<WorldScript::DlgChoise>     ret;

  for(size_t i=0;i<info.subChoices.size();++i){
    auto& sub = info.subChoices[i];
    //bool npcKnowsInfo = doesNpcKnowInfo(pl.instanceSymbol(),info.instanceSymbol);
    //if(npcKnowsInfo)
    //  continue;

    bool valid=false;
    if(info.condition)
      valid = runFunction(info.condition)!=0;

    WorldScript::DlgChoise ch;
    ch.title    = sub.text;
    ch.scriptFn = sub.functionSym;
    ch.handle   = dlg.handle;
    ch.isTrade  = false;
    ch.sort     = int(i);
    ret.push_back(ch);
    }

  sort(ret);
  return ret;
  }

void WorldScript::exec(const WorldScript::DlgChoise &dlg,
                       Daedalus::GameState::NpcHandle player,
                       Daedalus::GameState::NpcHandle hnpc) {
  ScopeVar self (vm,"self", ZMemory::toBigHandle(hnpc),   Daedalus::IC_Npc);
  ScopeVar other(vm,"other",ZMemory::toBigHandle(player), Daedalus::IC_Npc);

  Daedalus::GEngineClasses::C_Info& info = vm.getGameState().getInfo(dlg.handle);

  auto pl = vmNpc(player);
  if(info.information==dlg.scriptFn) {
    setNpcInfoKnown(pl,info);
    } else {
    for(size_t i=0;i<info.subChoices.size();){
      if(info.subChoices[i].functionSym==dlg.scriptFn)
        info.subChoices.erase(info.subChoices.begin()+int(i)); else
        ++i;
      }
    }
  runFunction(dlg.scriptFn);
  }

int WorldScript::printCannotUseError(Npc& npc, int32_t atr, int32_t nValue) {
  if(!hasSymbolName("G_CanNotUse"))
    return 0;
  auto id = vm.getDATFile().getSymbolIndexByName("G_CanNotUse");
  vm.pushInt(npc.isPlayer() ? 1 : 0);
  vm.pushInt(atr);
  vm.pushInt(nValue);
  ScopeVar self (vm,"self",npc);
  return runFunction(id,false);
  }

int WorldScript::printCannotCastError(Npc &npc, int32_t plM, int32_t itM) {
  if(!hasSymbolName("G_CanNotCast"))
    return 0;
  auto id = vm.getDATFile().getSymbolIndexByName("G_CanNotCast");
  vm.pushInt(npc.isPlayer() ? 1 : 0);
  vm.pushInt(itM);
  vm.pushInt(plM);
  ScopeVar self (vm,"self",npc);
  return runFunction(id,false);
  }

int WorldScript::printCannotBuyError(Npc &npc) {
  if(!hasSymbolName("player_trade_not_enough_gold"))
    return 0;
  auto id = vm.getDATFile().getSymbolIndexByName("player_trade_not_enough_gold");
  ScopeVar self (vm,"self",npc);
  return runFunction(id,true);
  }

int WorldScript::invokeState(NpcHandle hnpc, NpcHandle oth, const char *name) {
  auto& dat = vm.getDATFile();
  if(name==nullptr || !dat.hasSymbolName(name))
    return 1;

  auto id = dat.getSymbolIndexByName(name);

  ScopeVar self (vm,"self", ZMemory::toBigHandle(hnpc), Daedalus::IC_Npc);
  ScopeVar other(vm,"other",ZMemory::toBigHandle(oth),  Daedalus::IC_Npc);

  return runFunction(id);
  }

int WorldScript::invokeState(Npc* npc, Npc* oth, Npc* v, size_t fn) {
  if(fn==0)
    return 1;
  if(oth==nullptr){
    // auto& lvl = vm.getDATFile().getSymbolByName("PC_Levelinspektor");
    // NpcHandle hnpc = ZMemory::handleCast<NpcHandle>(lvl.instanceDataHandle);
    // auto n = getNpc(hnpc);
    oth=owner.player();//FIXME: PC_Levelinspektor
    }
  if(v==nullptr)
    v=owner.player();

  ScopeVar self  (vm,"self",  npc);
  ScopeVar other (vm,"other", oth);
  ScopeVar victum(vm,"victim",v);
  return runFunction(fn);
  }

int WorldScript::invokeItem(Npc *npc,size_t fn) {
  if(fn==0)
    return 1;

  auto hnpc = npc ? npc->handle() : NpcHandle();
  ScopeVar self (vm,"self", ZMemory::toBigHandle(hnpc), Daedalus::IC_Npc);
  return runFunction(fn);
  }

int WorldScript::invokeMana(Npc &npc, Item &) {
  auto& dat = vm.getDATFile();
  if(!dat.hasSymbolName("Spell_ProcessMana"))
    return Npc::SpellCode::SPL_SENDSTOP;

  auto fn = dat.getSymbolIndexByName("Spell_ProcessMana");
  if(fn==0)
    return Npc::SpellCode::SPL_SENDSTOP;

  ScopeVar self (vm,"self",npc);

  vm.pushInt(npc.attribute(Npc::ATR_MANA));
  return runFunction(fn,false);
  }

int WorldScript::invokeSpell(Npc &npc, Item &it) {
  auto& spellInst = vm.getDATFile().getSymbolByIndex(spellFxInstanceNames);
  auto& tag       = spellInst.getString(size_t(it.spellId()));
  auto  str       = "Spell_Cast_" + tag;

  auto& dat = vm.getDATFile();
  auto  fn  = dat.getSymbolIndexByName(str);
  if(fn==0)
    return 0;

  ScopeVar self(vm,"self",npc);
  return runFunction(fn);
  }

int WorldScript::spellCastAnim(Npc&, Item &it) {
  auto& spellAni = vm.getDATFile().getSymbolByIndex(spellFxAniLetters);
  auto& tag      = spellAni.getString(size_t(it.spellId()));
  if(tag=="FIB")
    return Npc::Anim::MagFib;
  if(tag=="WND")
    return Npc::Anim::MagWnd;
  if(tag=="HEA")
    return Npc::Anim::MagHea;
  if(tag=="PYR")
    return Npc::Anim::MagPyr;
  if(tag=="FEA")
    return Npc::Anim::MagFea;
  if(tag=="TRF")
    return Npc::Anim::MagTrf;
  if(tag=="SUM")
    return Npc::Anim::MagSum;
  if(tag=="MSD")
    return Npc::Anim::MagMsd;
  if(tag=="STM")
    return Npc::Anim::MagStm;
  if(tag=="FRZ")
    return Npc::Anim::MagFrz;
  if(tag=="SLE")
    return Npc::Anim::MagSle;
  if(tag=="WHI")
    return Npc::Anim::MagWhi;
  if(tag=="SCK")
    return Npc::Anim::MagSck;
  if(tag=="FBT")
    return Npc::Anim::MagFbt;
  return Npc::Anim::MagFib;
  }

bool WorldScript::aiUseMob(Npc &pl, const std::string &name) {
  return owner.aiUseMob(pl,name);
  }

bool WorldScript::aiOutput(Npc &from, Npc &/*to*/, const std::string &outputname) {
  return owner.aiOutput(from,outputname.c_str());
  }

bool WorldScript::aiClose() {
  return owner.aiCloseDialog();
  }

bool WorldScript::isDead(const Npc &pl) {
  return pl.isState(ZS_Dead);
  }

bool WorldScript::isUnconscious(const Npc &pl) {
  return pl.isState(ZS_Unconscious);
  }

const std::string &WorldScript::messageByName(const std::string& id) const {
  if(!dialogs->messageExists(id)){
    static std::string empty;
    return empty;
    }
  return dialogs->getMessageByName(id).text;
  }

uint32_t WorldScript::messageTime(const std::string &id) const {
  auto& txt = messageByName(id);
  return uint32_t(txt.size()*viewTimePerChar);
  }

void WorldScript::useInteractive(NpcHandle hnpc,const std::string& func) {
  auto& dat = vm.getDATFile();
  if(!dat.hasSymbolName(func))
    return;

  ScopeVar self(vm,"self",ZMemory::toBigHandle(hnpc),Daedalus::IC_Npc);
  try {
    runFunction(func);
    }
  catch (...) {
    Log::i("unable to use interactive [",func,"]");
    }
  }

WorldScript::Attitude WorldScript::guildAttitude(const Npc &p0, const Npc &p1) const {
  auto selfG = std::min(gilCount-1,p0.guild());
  auto npcG  = std::min(gilCount-1,p1.guild());
  auto ret   = gilAttitudes[selfG*gilCount+npcG];
  return Attitude(ret);
  }

bool WorldScript::hasSymbolName(const std::string &fn) {
  return vm.getDATFile().hasSymbolName(fn);
  }

int32_t WorldScript::runFunction(const std::string& fname) {
  if(!hasSymbolName(fname))
    throw std::runtime_error("script bad call");
  auto id = vm.getDATFile().getSymbolIndexByName(fname);
  return runFunction(id,true);
  }

int32_t WorldScript::runFunction(const size_t fid,bool clearStk) {
  if(invokeRecursive) {
    //Log::d("WorldScript: invokeRecursive");
    clearStk=false;
    //return 0;
    }
  invokeRecursive++;

  auto&       dat  = vm.getDATFile();
  auto&       sym  = dat.getSymbolByIndex(fid);
  const char* call = sym.name.c_str();(void)call; //for debuging

  vm.prepareRunFunction();
  int32_t ret = vm.runFunctionBySymIndex(fid,clearStk);
  invokeRecursive--;
  return ret;
  }

uint64_t WorldScript::tickCount() const {
  return owner.tickCount();
  }

template<class Ret,class ... Args>
std::function<Ret(Args...)> WorldScript::notImplementedFn(){
  struct _{
    static Ret fn(Args...){
      static bool first=true;
      if(first){
        Log::e("not implemented routine call");
        first=false;
        }
      return Ret();
      }
    };
  return std::function<Ret(Args...)>(_::fn);
  }

void WorldScript::notImplementedRoutine(Daedalus::DaedalusVM &vm) {
  static std::unordered_set<std::string> s;
  auto fn = vm.getCallStack();

  if(s.find(fn[0])==s.end()){
    s.insert(fn[0]);
    Log::e("not implemented call [",fn[0],"]");
    }
  }

void WorldScript::onNpcReady(NpcHandle handle) {
  auto  hnpc    = ZMemory::handleCast<NpcHandle>(handle);
  auto& npcData = vm.getGameState().getNpc(hnpc);

  Npc* npc=getNpc(hnpc);
  if(npc==nullptr)
    return;

  if(!npcData.name[0].empty())
    npc->setName(npcData.name[0]);

  if(npcData.daily_routine!=0) {
    vm.prepareRunFunction();

    ScopeVar self(vm, "self", ZMemory::toBigHandle(handle), Daedalus::IC_Npc);
    vm.setCurrentInstance(vm.getDATFile().getSymbolIndexByName("self"));

    vm.runFunctionBySymIndex(npcData.daily_routine,false);
    }
  }

Npc* WorldScript::getNpc(NpcHandle handle) {
  auto hnpc = ZMemory::handleCast<NpcHandle>(handle);
  if(!hnpc.isValid())
    return nullptr;
  auto& npcData = vm.getGameState().getNpc(hnpc);
  assert(npcData.userPtr); // engine bug, if null
  return reinterpret_cast<Npc*>(npcData.userPtr);
  }

Item *WorldScript::getItem(ItemHandle handle) {
  auto h = ZMemory::handleCast<ItemHandle>(handle);
  if(!h.isValid())
    return nullptr;
  auto& itData = vm.getGameState().getItem(h);
  assert(itData.userPtr); // engine bug, if null
  return reinterpret_cast<Item*>(itData.userPtr);
  }

Item *WorldScript::getItemById(size_t id) {
  auto& handle = vm.getDATFile().getSymbolByIndex(id);
  if(handle.instanceDataClass!= Daedalus::EInstanceClass::IC_Item)
    return nullptr;

  ItemHandle hnpc = ZMemory::handleCast<ItemHandle>(handle.instanceDataHandle);
  return getItem(hnpc);
  }

Npc* WorldScript::getNpcById(size_t id) {
  auto& handle = vm.getDATFile().getSymbolByIndex(id);
  if(handle.instanceDataClass!= Daedalus::EInstanceClass::IC_Npc)
    return nullptr;

  NpcHandle hnpc = ZMemory::handleCast<NpcHandle>(handle.instanceDataHandle);
  return getNpc(hnpc);
  }

Npc* WorldScript::inserNpc(const char *npcInstance, const char *at) {
  size_t id = vm.getDATFile().getSymbolIndexByName(npcInstance);
  if(id==0)
    return nullptr;
  return inserNpc(id,at);
  }

void WorldScript::removeItem(Item &it) {
  owner.removeItem(it);
  }

void WorldScript::setInstanceNPC(const char *name, Npc &npc) {
  assert(vm.getDATFile().hasSymbolName(name));
  vm.setInstance(name, ZMemory::toBigHandle(npc.handle()), Daedalus::EInstanceClass::IC_Npc);
  }

Npc* WorldScript::inserNpc(size_t npcInstance, const char* at) {
  auto pos = owner.findPoint(at);
  if(pos==nullptr){
    Log::e("inserNpc: invalid waypoint");
    return nullptr;
    }
  auto h = vm.getGameState().insertNPC(npcInstance,at);
  return getNpc(h);
  }

const std::string& WorldScript::popString(Daedalus::DaedalusVM &vm) {
  uint32_t arr;
  uint32_t idx = vm.popVar(arr);

  return vm.getDATFile().getSymbolByIndex(idx).getString(arr, vm.getCurrentInstanceDataPtr());
  }

Npc *WorldScript::popInstance(Daedalus::DaedalusVM &vm) {
  uint32_t arr_self = 0;
  uint32_t idx      = vm.popVar(arr_self);
  return getNpcById(idx);
  }

Item *WorldScript::popItem(Daedalus::DaedalusVM &vm) {
  uint32_t arr_self = 0;
  uint32_t idx      = vm.popVar(arr_self);
  return getItemById(idx);
  }



void WorldScript::concatstrings(Daedalus::DaedalusVM &vm) {
  const std::string& s2 = popString(vm);
  const std::string& s1 = popString(vm);

  vm.setReturn(s1 + s2);
  }

void WorldScript::inttostring(Daedalus::DaedalusVM &vm){
  int32_t x = vm.popDataValue<int32_t>();
  vm.setReturn(std::to_string(x)); //TODO: std::move?
  }

void WorldScript::floattostring(Daedalus::DaedalusVM &vm) {
  auto x = vm.popFloatValue();
  vm.setReturn(std::to_string(x)); //TODO: std::move?
  }

void WorldScript::floattoint(Daedalus::DaedalusVM &vm) {
  auto x = vm.popFloatValue();
  vm.setReturn(int32_t(x));
  }

void WorldScript::inttofloat(Daedalus::DaedalusVM &vm) {
  auto x = vm.popDataValue();
  vm.setReturn(float(x));
  }

void WorldScript::hlp_random(Daedalus::DaedalusVM &vm) {
  uint32_t mod = uint32_t(std::max(1,vm.popDataValue()));
  vm.setReturn(int32_t(randGen() % mod));
  }

void WorldScript::hlp_strcmp(Daedalus::DaedalusVM &vm) {
  auto& s1 = popString(vm);
  auto& s2 = popString(vm);
  vm.setReturn(s1 == s2 ? 1 : 0);
  }

void WorldScript::wld_settime(Daedalus::DaedalusVM &vm) {
  auto minute = vm.popDataValue();
  auto hour   = vm.popDataValue();
  owner.setDayTime(hour,minute);
  }

void WorldScript::wld_getday(Daedalus::DaedalusVM &vm) {
  auto d = owner.time().day();
  vm.setReturn(int32_t(d));
  }

void WorldScript::wld_playeffect(Daedalus::DaedalusVM &vm) {
  int32_t            a        = vm.popDataValue();
  int32_t            b        = vm.popDataValue();
  int32_t            c        = vm.popDataValue();
  int32_t            d        = vm.popDataValue();
  auto               npc1     = popInstance(vm);
  auto               npc0     = popInstance(vm);
  const std::string& visual   = popString(vm);

  Log::i("effect not implemented [",visual,"]");
  }

void WorldScript::wld_stopeffect(Daedalus::DaedalusVM &vm) {
  const std::string& visual   = popString(vm);
  Log::i("effect not implemented [",visual,"]");
  }

void WorldScript::wld_getplayerportalguild(Daedalus::DaedalusVM &vm) {
  const int GIL_PUBLIC = 15; // _Intern/Constants.d
  vm.setReturn(GIL_PUBLIC);  // TODO: guild id for a room
  }

void WorldScript::wld_setguildattitude(Daedalus::DaedalusVM &vm) {
  size_t  gil2 = size_t(vm.popDataValue());
  int32_t att  = vm.popDataValue();
  size_t  gil1 = size_t(vm.popDataValue());
  if(gilCount==0 || gil1>=gilCount || gil2>=gilCount)
    return;
  gilAttitudes[gil1*gilCount+gil2]=att;
  gilAttitudes[gil2*gilCount+gil1]=att;
  }

void WorldScript::wld_getguildattitude(Daedalus::DaedalusVM &vm) {
  int32_t g0 = vm.popDataValue();
  int32_t g1 = vm.popDataValue();
  if(g0<0 || g1<0 || gilCount==0) {
    vm.setReturn(ATT_HOSTILE); // error
    return;
    }

  auto selfG = std::min(gilCount-1,size_t(g0));
  auto npcG  = std::min(gilCount-1,size_t(g1));
  auto ret   = gilAttitudes[selfG*gilCount+npcG];
  vm.setReturn(ret);
  }

void WorldScript::wld_istime(Daedalus::DaedalusVM &vm) {
  int32_t min1  = vm.popDataValue();
  int32_t hour1 = vm.popDataValue();
  int32_t min0  = vm.popDataValue();
  int32_t hour0 = vm.popDataValue();

  gtime begin{hour0,min0}, end{hour1,min1};
  gtime now = owner.time();
  now = gtime(0,now.hour(),now.minute());

  if(begin<=end && begin<=now && now<end)
    vm.setReturn(1);
  else if(end<begin && (now<end || begin<=now))
    vm.setReturn(1);
  else
    vm.setReturn(0);
  }

void WorldScript::wld_isfpavailable(Daedalus::DaedalusVM &vm) {
  auto& name = popString(vm);
  auto  self = popInstance(vm);

  if(self==nullptr){
    vm.setReturn(0);
    return;
    }
  auto wp = owner.findFreePoint(self->position(),name.c_str());
  vm.setReturn(wp ? 1 : 0); //TODO: check point - it must be of use free
  }

void WorldScript::wld_ismobavailable(Daedalus::DaedalusVM &vm) {
  auto& name = popString(vm);
  auto  self = popInstance(vm);
  if(self==nullptr){
    vm.setReturn(0);
    return;
    }

  auto wp = owner.aviableMob(*self,name);
  vm.setReturn(wp ? 1 : 0);
  }

void WorldScript::wld_setmobroutine(Daedalus::DaedalusVM &vm) {
  int   st   = vm.popDataValue();
  auto& name = popString(vm);
  int   mm   = vm.popDataValue();
  int   hh   = vm.popDataValue();
  Log::i("TODO: wld_setmobroutine(",hh,",",mm,",",name,",",st,")");
  }

void WorldScript::wld_assignroomtoguild(Daedalus::DaedalusVM &vm) {
  int   g    = vm.popDataValue();
  auto& name = popString(vm);
  Log::i("TODO: wld_assignroomtoguild(",name,",",g,")");
  }

void WorldScript::mdl_setvisual(Daedalus::DaedalusVM &vm) {
  const std::string& visual = popString(vm);
  auto               npc    = popInstance(vm);
  if(npc==nullptr)
    return;

  auto skelet = Resources::loadSkeleton (visual);
  npc->setVisual(skelet);
  }

void WorldScript::mdl_setvisualbody(Daedalus::DaedalusVM &vm) {
  int32_t     armor        = vm.popDataValue();
  int32_t     teethTexNr   = vm.popDataValue();
  int32_t     headTexNr    = vm.popDataValue();
  auto&       head         = popString(vm);
  int32_t     bodyTexColor = vm.popDataValue();
  int32_t     bodyTexNr    = vm.popDataValue();
  auto&       body         = popString(vm);
  auto        npc          = popInstance(vm);

  auto  vname = addExt(body,".MDM");
  auto  vhead = head.empty() ? StaticObjects::Mesh() : owner.getView(addExt(head,".MMB"),headTexNr,teethTexNr,bodyTexColor);
  auto  vbody = body.empty() ? StaticObjects::Mesh() : owner.getView(vname,bodyTexNr,0,bodyTexColor);

  if(npc==nullptr)
    return;

  npc->setPhysic(owner.getPhysic(vname));
  npc->setVisualBody(std::move(vhead),std::move(vbody),bodyTexNr,bodyTexColor);

  if(armor>=0) {
    if(npc->hasItem(uint32_t(armor))==0)
      npc->addItem(uint32_t(armor),1);
    npc->useItem(uint32_t(armor),true);
    }
  }

void WorldScript::mdl_setmodelfatness(Daedalus::DaedalusVM &vm) {
  float    fat = vm.popFloatValue();
  auto     npc = popInstance(vm);

  if(npc!=nullptr)
    npc->setFatness(fat);
  }

void WorldScript::mdl_applyoverlaymds(Daedalus::DaedalusVM &vm) {
  auto&       overlayname = popString(vm);
  auto        npc         = popInstance(vm);
  auto        skelet      = Resources::loadSkeleton(overlayname);

  if(npc!=nullptr)
    npc->addOverlay(skelet,0);
  }

void WorldScript::mdl_applyoverlaymdstimed(Daedalus::DaedalusVM &vm) {
  int32_t     ticks       = vm.popDataValue();
  auto&       overlayname = popString(vm);
  auto        npc         = popInstance(vm);
  auto        skelet      = Resources::loadSkeleton(overlayname);

  if(npc!=nullptr && ticks>0)
    npc->addOverlay(skelet,uint64_t(ticks));
  }

void WorldScript::mdl_removeoverlaymds(Daedalus::DaedalusVM &vm) {
  auto&       overlayname = popString(vm);
  auto        npc         = popInstance(vm);
  auto        skelet      = Resources::loadSkeleton(overlayname);

  if(npc!=nullptr)
    npc->delOverlay(skelet);
  }

void WorldScript::mdl_setmodelscale(Daedalus::DaedalusVM &vm) {
  float z   = vm.popFloatValue();
  float y   = vm.popFloatValue();
  float x   = vm.popFloatValue();
  auto  npc = popInstance(vm);

  if(npc!=nullptr)
    npc->setScale(x,y,z);
  }

void WorldScript::mdl_startfaceani(Daedalus::DaedalusVM &vm) {
  float time      = vm.popFloatValue();
  float intensity = vm.popFloatValue();
  auto& ani       = popString(vm);
  auto  npc       = popInstance(vm);
  //TODO: face animation
  }

void WorldScript::wld_insertnpc(Daedalus::DaedalusVM &vm) {
  const std::string& spawnpoint = popString(vm);
  int32_t npcInstance = vm.popDataValue();

  if(spawnpoint.empty() || npcInstance<=0)
    return;

  auto at=owner.findPoint(spawnpoint);
  if(at==nullptr){
    Log::e("invalid waypoint \"",spawnpoint,"\"");
    return;
    }
  inserNpc(size_t(npcInstance),spawnpoint.c_str());
  }

void WorldScript::wld_insertitem(Daedalus::DaedalusVM &vm) {
  const std::string& spawnpoint   = popString(vm);
  int32_t            itemInstance = vm.popDataValue();

  if(spawnpoint.empty() || itemInstance<=0)
    return;

  owner.addItem(size_t(itemInstance),spawnpoint.c_str());
  }

void WorldScript::npc_settofightmode(Daedalus::DaedalusVM &vm) {
  int32_t weaponSymbol = vm.popDataValue();
  auto    npc          = popInstance(vm);
  if(npc!=nullptr && weaponSymbol>=0)
    npc->setToFightMode(uint32_t(weaponSymbol));
  }

void WorldScript::npc_settofistmode(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->drawWeaponFist();
  }

void WorldScript::npc_isinstate(Daedalus::DaedalusVM &vm) {
  uint32_t stateFn = uint32_t(vm.popVar());
  auto     npc     = popInstance(vm);

  if(npc!=nullptr){
    const bool ret=npc->isState(stateFn);
    vm.setReturn(ret ? 1: 0);
    return;
    }
  vm.setReturn(0);
  }

void WorldScript::npc_wasinstate(Daedalus::DaedalusVM &vm) {
  uint32_t stateFn = uint32_t(vm.popVar());
  auto     npc     = popInstance(vm);

  if(npc!=nullptr){
    const bool ret=npc->wasInState(stateFn);
    vm.setReturn(ret ? 1: 0);
    return;
    }
  vm.setReturn(0);
  }

void WorldScript::npc_getdisttowp(Daedalus::DaedalusVM &vm) {
  auto&    wpname   = popString(vm);
  auto     npc      = popInstance(vm);

  auto* wp  = owner.findPoint(wpname);

  if(npc!=nullptr && wp!=nullptr){
    float ret = std::sqrt(npc->qDistTo(wp));
    if(ret<float(std::numeric_limits<int32_t>::max()))
      vm.setReturn(int32_t(ret));else
      vm.setReturn(std::numeric_limits<int32_t>::max());
    } else {
    vm.setReturn(std::numeric_limits<int32_t>::max());
    }
  }

void WorldScript::npc_exchangeroutine(Daedalus::DaedalusVM &vm) {
  auto&    rname  = popString(vm);
  auto     npc    = popInstance(vm);
  if(npc!=nullptr) {
    auto& v = vmNpc(npc->handle());
    char buf[256]={};
    std::snprintf(buf,sizeof(buf),"Rtn_%s_%d",rname.c_str(),v.id);
    auto d = vm.getDATFile().getSymbolIndexByName(buf);
    if(d>0)
      npc->aiStartState(d,0,"");
    }
  }

void WorldScript::npc_isdead(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc==nullptr || isDead(*npc))
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void WorldScript::npc_knowsinfo(Daedalus::DaedalusVM &vm) {
  uint32_t infoinstance = uint32_t(vm.popDataValue());
  auto     npc          = popInstance(vm);
  if(!npc){
    vm.setReturn(0);
    return;
    }

  Daedalus::GEngineClasses::C_Npc& vnpc = vmNpc(npc->handle());
  bool knows = doesNpcKnowInfo(vnpc, infoinstance);
  vm.setReturn(knows ? 1 : 0);
  }

void WorldScript::npc_settalentskill(Daedalus::DaedalusVM &vm) {
  int  lvl     = vm.popDataValue();
  int  t       = vm.popDataValue();
  auto npc     = popInstance(vm);
  if(npc!=nullptr)
    npc->setTalentSkill(Npc::Talent(t),lvl);
  }

void WorldScript::npc_gettalentskill(Daedalus::DaedalusVM &vm) {
  uint32_t skillId = uint32_t(vm.popDataValue());
  uint32_t self    = vm.popVar();
  auto     npc     = getNpcById(self);

  int32_t  skill   = npc==nullptr ? 0 : npc->talentSkill(Npc::Talent(skillId));
  vm.setReturn(skill);
  }

void WorldScript::npc_settalentvalue(Daedalus::DaedalusVM &vm) {
  int lvl  = vm.popDataValue();
  int t    = vm.popDataValue();
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->setTalentValue(Npc::Talent(t),lvl);
  }

void WorldScript::npc_gettalentvalue(Daedalus::DaedalusVM &vm) {
  uint32_t skillId = uint32_t(vm.popDataValue());
  auto     npc     = popInstance(vm);

  int32_t  skill   = npc==nullptr ? 0 : npc->talentValue(Npc::Talent(skillId));
  vm.setReturn(skill);
  }

void WorldScript::npc_setrefusetalk(Daedalus::DaedalusVM &vm) {
  int32_t  timeSec = vm.popDataValue();
  auto     npc     = popInstance(vm);
  if(npc)
    npc->setRefuseTalk(uint64_t(std::max(timeSec,0)));
  }

void WorldScript::npc_refusetalk(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc && npc->isRefuseTalk())
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void WorldScript::npc_hasitems(Daedalus::DaedalusVM &vm) {
  uint32_t itemId = uint32_t(vm.popDataValue());
  auto     npc    = popInstance(vm);
  if(npc!=nullptr)
    vm.setReturn(int32_t(npc->hasItem(itemId))); else
    vm.setReturn(0);
  }

void WorldScript::npc_removeinvitems(Daedalus::DaedalusVM &vm) {
  uint32_t amount = uint32_t(vm.popDataValue());
  uint32_t itemId = uint32_t(vm.popDataValue());
  auto     npc    = popInstance(vm);

  if(npc!=nullptr)
    npc->delItem(itemId,amount);
  }

void WorldScript::npc_getbodystate(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);

  if(npc!=nullptr)
    vm.setReturn(int32_t(npc->bodyState())); else
    vm.setReturn(int32_t(0));
  }

void WorldScript::npc_getlookattarget(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  auto ret = npc ? nullptr : npc->lookAtTarget();
  if(ret!=nullptr) {
    auto n = vmNpc(ret->handle());
    auto x = getNpcById(n.instanceSymbol);
    vm.setReturn(int32_t(n.instanceSymbol)); //TODO
    } else {
    vm.setReturn(0);
    }
  }

void WorldScript::npc_getdisttonpc(Daedalus::DaedalusVM &vm) {
  auto a = popInstance(vm);
  auto b = popInstance(vm);

  if(a==nullptr || b==nullptr){
    vm.setReturn(std::numeric_limits<int32_t>::max());
    return;
    }

  float ret = std::sqrt(a->qDistTo(*b));
  if(ret>float(std::numeric_limits<int32_t>::max()))
    vm.setReturn(std::numeric_limits<int32_t>::max()); else
    vm.setReturn(int(ret));
  }

void WorldScript::npc_hasequippedarmor(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr && npc->currentArmour()!=nullptr)
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void WorldScript::npc_getattitude(Daedalus::DaedalusVM &vm) {
  auto a = popInstance(vm);
  auto b = popInstance(vm);

  if(a!=nullptr && b!=nullptr){
    auto att=guildAttitude(*a,*b);
    vm.setReturn(att); //TODO: personal attitudes
    } else {
    vm.setReturn(ATT_NEUTRAL);
    }
  }

void WorldScript::npc_setperctime(Daedalus::DaedalusVM &vm) {
  float sec = vm.popFloatValue();
  auto  npc = popInstance(vm);
  if(npc)
    npc->setPerceptionTime(uint64_t(sec*1000));
  }

void WorldScript::npc_percenable(Daedalus::DaedalusVM &vm) {
  int32_t fn  = vm.popDataValue();
  int32_t pr  = vm.popDataValue();
  auto    npc = popInstance(vm);
  if(npc && fn>=0)
    npc->setPerceptionEnable(Npc::PercType(pr),size_t(fn));
  }

void WorldScript::npc_percdisable(Daedalus::DaedalusVM &vm) {
  int32_t pr  = vm.popDataValue();
  auto    npc = popInstance(vm);
  if(npc)
    npc->setPerceptionDisable(Npc::PercType(pr));
  }

void WorldScript::npc_getnearestwp(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  auto wp  = npc ? owner.findWayPoint(npc->position()) : nullptr;
  if(wp)
    vm.setReturn(wp->wpName); else
    vm.setReturn("");
  }

void WorldScript::npc_clearaiqueue(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc)
    npc->aiClearQueue();
  }

void WorldScript::npc_isplayer(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc && npc->isPlayer())
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void WorldScript::npc_getstatetime(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc)
    vm.setReturn(int32_t(npc->stateTime()/1000)); else
    vm.setReturn(0);
  }

void WorldScript::npc_setstatetime(Daedalus::DaedalusVM &vm) {
  int32_t val = vm.popDataValue();
  auto    npc = popInstance(vm);
  if(npc)
    npc->setStateTime(val*1000);
  }

void WorldScript::npc_changeattribute(Daedalus::DaedalusVM &vm) {
  int32_t val  = vm.popDataValue();
  int32_t atr  = vm.popDataValue();
  auto    npc  = popInstance(vm);
  if(npc!=nullptr && atr>=0)
    npc->changeAttribute(Npc::Attribute(atr),val);
  }

void WorldScript::npc_isonfp(Daedalus::DaedalusVM &vm) {
  auto& val = popString(vm);
  auto  npc = popInstance(vm);
  if(npc!=nullptr) {
    auto w = npc->currentWayPoint();

    if(w!=nullptr && w->wpName.find(val)!=std::string::npos) {
      if(npc->qDistTo(w)<10*10){
        vm.setReturn(1);
        return;
        }
      }

    auto f = owner.findFreePoint(npc->position(),val.c_str());
    if(f!=nullptr && npc->qDistTo(f)<10*10){
      vm.setReturn(1);
      } else {
      vm.setReturn(0);
      }
    }
  }

void WorldScript::npc_getheighttonpc(Daedalus::DaedalusVM &vm) {
  auto  a   = popInstance(vm);
  auto  b   = popInstance(vm);
  float ret = 0;
  if(a!=nullptr && b!=nullptr)
    ret = std::abs(a->position()[1] - b->position()[1]);
  vm.setReturn(int32_t(ret));
  }

void WorldScript::npc_getequippedmeleweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr){
    auto a = npc->currentRangeWeapon();
    if(a!=nullptr)
      vm.setReturn(int32_t(a->clsId())); else
      vm.setReturn(0);
    }
  }

void WorldScript::npc_getequippedrangedweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr){
    auto a = npc->currentRangeWeapon();
    if(a!=nullptr)
      vm.setReturn(int32_t(a->clsId())); else
      vm.setReturn(0);
    }
  }

void WorldScript::npc_getequippedarmor(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr){
    auto a = npc->currentArmour();
    if(a!=nullptr)
      vm.setReturn(int32_t(a->clsId())); else
      vm.setReturn(0);
    }
  }

void WorldScript::npc_canseenpc(Daedalus::DaedalusVM &vm) {
  auto npc   = popInstance(vm);
  auto other = popInstance(vm);
  vm.setReturn(1); //TODO
  }

void WorldScript::npc_hasequippedmeleeweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr && npc->currentMeleWeapon()!=nullptr)
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void WorldScript::npc_hasequippedrangedweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr && npc->currentRangeWeapon()!=nullptr)
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void WorldScript::npc_getactivespell(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc==nullptr){
    vm.setReturn(-1);
    return;
    }

  const Item* w = npc->inventory().activeWeapon();
  if(w==nullptr || !w->isSpell()){
    vm.setReturn(-1);
    return;
    }

  vm.setReturn(w->spellId());
  }

void WorldScript::npc_getactivespellisscroll(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc==nullptr){
    vm.setReturn(0);
    return;
    }

  const Item* w = npc->inventory().activeWeapon();
  if(w==nullptr || !w->isSpell()){
    vm.setReturn(0);
    return;
    }

  vm.setReturn(1);
  }

void WorldScript::npc_canseenpcfreelos(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  auto oth = popInstance(vm);

  if(npc!=nullptr && oth!=nullptr){
    vm.setReturn(1); // TODO: real check view
    return;
    }
  vm.setReturn(0);
  }

void WorldScript::npc_isinfightmode(Daedalus::DaedalusVM &vm) {
  FightMode mode = FightMode(vm.popDataValue());
  auto      npc  = popInstance(vm);

  if(npc==nullptr){
    vm.setReturn(0);
    return;
    }

  auto st  = npc->weaponState();
  bool ret = false;
  if(mode==FightMode::FMODE_NONE){
    ret = (st==WeaponState::NoWeapon);
    }
  else if(mode==FightMode::FMODE_FIST){
    ret = (st==WeaponState::Fist);
    }
  else if(mode==FightMode::FMODE_MELEE){
    ret = (st==WeaponState::W1H || st==WeaponState::W2H);
    }
  else if(mode==FightMode::FMODE_FAR){
    ret = (st==WeaponState::Bow || st==WeaponState::CBow);
    }
  else if(mode==FightMode::FMODE_MAGIC){
    ret = (st==WeaponState::Mage);
    }
  vm.setReturn(ret ? 1: 0);
  }

void WorldScript::npc_settarget(Daedalus::DaedalusVM &vm) {
  auto oth = popInstance(vm);
  auto npc = popInstance(vm);
  if(npc)
    npc->setTarget(oth);
  }

void WorldScript::npc_gettarget(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr && npc->target()) {
    Daedalus::PARSymbol& s = vm.getDATFile().getSymbolByName("other");
    s.instanceDataHandle   = ZMemory::toBigHandle(npc->target()->handle());
    s.instanceDataClass    = Daedalus::IC_Npc;
    vm.setReturn(1);
    } else {
    vm.setReturn(0);
    }
  }

void WorldScript::npc_sendpassiveperc(Daedalus::DaedalusVM &vm) {
  auto other  = popInstance(vm);
  auto victum = popInstance(vm);
  auto id     = vm.popDataValue();
  auto npc    = popInstance(vm);

  if(npc && other && victum)
    owner.sendPassivePerc(*npc,*other,*victum,id);
  }

void WorldScript::npc_checkinfo(Daedalus::DaedalusVM &vm) {
  auto imp = vm.popDataValue();
  auto n   = popInstance(vm);
  if(n==nullptr){
    vm.setReturn(1);
    return;
    }

  auto& hero     = vm.getDATFile().getSymbolByName("other");
  NpcHandle hnpc = ZMemory::handleCast<NpcHandle>(hero.instanceDataHandle);
  auto& pl       = vmNpc(hnpc);
  auto& npc      = vmNpc(n->handle());

  for(auto& infoHandle : dialogsInfo) {
    Daedalus::GEngineClasses::C_Info& info = vm.getGameState().getInfo(infoHandle);
    if(info.npc!=int32_t(npc.instanceSymbol) || info.important!=imp)
      continue;
    bool npcKnowsInfo = doesNpcKnowInfo(pl,info.instanceSymbol);
    if(npcKnowsInfo)
      continue;
    bool valid=false;
    if(info.condition)
      valid = runFunction(info.condition)!=0;
    if(valid) {
      vm.setReturn(1);
      return;
      }
    }
  vm.setReturn(0);
  }

void WorldScript::npc_getportalguild(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  const int GIL_PUBLIC = 15; // _Intern/Constants.d
  vm.setReturn(GIL_PUBLIC);  // TODO: guild id for a room
  }

void WorldScript::npc_isinplayersroom(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  vm.setReturn(0); //TODO: stub
  }

void WorldScript::npc_getreadiedweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc==nullptr) {
    vm.setReturn(0);
    return;
    }

  auto ret = npc->inventory().activeWeapon();
  if(ret!=nullptr){
    vm.setReturn(int32_t(ret->clsId()));
    } else {
    vm.setReturn(0);
    }
  }

void WorldScript::npc_isdrawingspell(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc==nullptr){
    vm.setReturn(0);
    return;
    }
  auto it = npc->inventory().activeWeapon();
  if(it==nullptr || !it->isSpell()){
    vm.setReturn(0);
    return;
    }
  vm.setReturn(int32_t(it->clsId()));
  }

void WorldScript::npc_perceiveall(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  (void)npc; // nop
  }

void WorldScript::npc_stopani(Daedalus::DaedalusVM &vm) {
  auto& name = popString(vm);
  auto  npc  = popInstance(vm);
  if(npc!=nullptr)
    npc->stopAnim(name);
  }

void WorldScript::ai_processinfos(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  auto pl  = owner.player();
  if(pl!=nullptr && npc!=nullptr) {
    owner.aiProcessInfos(*pl,*npc);
    }
  }

void WorldScript::ai_output(Daedalus::DaedalusVM &vm) {
  auto& outputname = popString(vm);
  auto  target     = popInstance(vm);
  auto  self       = popInstance(vm);

  if(!self || !target)
    return;

  owner.aiForwardOutput(*self,outputname.c_str());
  self->aiOutput(*target,outputname);
  }

void WorldScript::ai_stopprocessinfos(Daedalus::DaedalusVM &vm) {
  auto self = popInstance(vm);
  if(self)
    self->aiStopProcessInfo();
  }

void WorldScript::ai_standup(Daedalus::DaedalusVM &vm) {
  auto self = popInstance(vm);
  if(self!=nullptr)
    self->aiStandup();
  }

void WorldScript::ai_standupquick(Daedalus::DaedalusVM &vm) {
  auto self = popInstance(vm);
  if(self!=nullptr)
    self->aiStandup(); //TODO
  }

void WorldScript::ai_continueroutine(Daedalus::DaedalusVM &vm) {
  auto self = popInstance(vm);
  (void)self;
  Log::d("TODO: ai_continueroutine");
  }

void WorldScript::ai_stoplookat(Daedalus::DaedalusVM &vm) {
  auto self = popInstance(vm);
  if(self!=nullptr)
    self->aiStopLookAt();
  }

void WorldScript::ai_lookatnpc(Daedalus::DaedalusVM &vm) {
  auto npc  = popInstance(vm);
  auto self = popInstance(vm);
  if(self!=nullptr)
    self->aiLookAt(npc);
  }

void WorldScript::ai_removeweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiRemoveWeapon();
  }

void WorldScript::ai_turntonpc(Daedalus::DaedalusVM &vm) {
  auto npc  = popInstance(vm);
  auto self = popInstance(vm);
  if(self!=nullptr)
    self->aiTurnToNpc(npc);
  }

void WorldScript::ai_outputsvm(Daedalus::DaedalusVM &vm) {
  auto& name   = popString  (vm);
  auto  target = popInstance(vm);
  auto  self   = popInstance(vm);
  if(self!=nullptr && target!=nullptr)
    self->aiOutputSvm(*target,name);
  }

void WorldScript::ai_outputsvm_overlay(Daedalus::DaedalusVM &vm) {
  auto& name   = popString  (vm);
  auto  target = popInstance(vm);
  auto  self   = popInstance(vm);
  if(self!=nullptr && target!=nullptr)
    self->aiOutputSvmOverlay(*target,name);
  }

void WorldScript::ai_startstate(Daedalus::DaedalusVM &vm) {
  auto& wp    = popString(vm);
  auto  state = vm.popDataValue();
  auto  func  = vm.popDataValue();
  auto  self  = popInstance(vm);
  if(self!=nullptr && func>0)
    self->aiStartState(uint32_t(func),state,wp);
  }

void WorldScript::ai_playani(Daedalus::DaedalusVM &vm) {
  auto name = popString  (vm);
  auto npc  = popInstance(vm);
  if(npc!=nullptr)
    npc->aiPlayAnim(name);
  }

void WorldScript::ai_setwalkmode(Daedalus::DaedalusVM &vm) {
  auto mode = vm.popDataValue();
  auto npc  = popInstance(vm);
  if(npc!=nullptr && mode>=0 && mode<=3){
    npc->setWalkMode(WalkBit(mode));
    }
  }

void WorldScript::ai_wait(Daedalus::DaedalusVM &vm) {
  auto ms  = vm.popFloatValue();
  auto npc = popInstance(vm);
  if(npc!=nullptr && ms>0)
    npc->aiWait(uint64_t(ms*1000));
  }

void WorldScript::ai_waitms(Daedalus::DaedalusVM &vm) {
  auto ms  = vm.popDataValue();
  auto npc = popInstance(vm);
  if(npc!=nullptr && ms>0)
    npc->aiWait(uint64_t(ms));
  }

void WorldScript::ai_aligntowp(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  //Log::d("TODO: ai_aligntowp");
  }

void WorldScript::ai_gotowp(Daedalus::DaedalusVM &vm) {
  auto&  waypoint = popString(vm);
  auto   npc      = popInstance(vm);

  auto to = owner.findPoint(waypoint);
  if(npc) {
    npc->aiGoToPoint(to);
    }
  }

void WorldScript::ai_gotofp(Daedalus::DaedalusVM &vm) {
  auto&  waypoint = popString(vm);
  auto   npc      = popInstance(vm);

  if(npc) {
    auto to = owner.findFreePoint(npc->position(),waypoint.c_str());
    if(to!=nullptr)
      npc->aiGoToPoint(to);
    }
  }

void WorldScript::ai_playanibs(Daedalus::DaedalusVM &vm) {
  Npc::BodyState bs  = Npc::BodyState(vm.popVar());
  auto&          ani = popString(vm);
  auto           npc = popInstance(vm);
  if(npc!=nullptr){
    (void)bs;
    npc->aiPlayAnim(ani);
    }
  }

void WorldScript::ai_equiparmor(Daedalus::DaedalusVM &vm) {
  auto id  = vm.popDataValue();
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiEquipArmor(id);
  }

void WorldScript::ai_equipbestmeleeweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiEquipBestMeleWeapon();
  }

void WorldScript::ai_equipbestrangedweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiEquipBestRangeWeapon();
  }

void WorldScript::ai_usemob(Daedalus::DaedalusVM &vm) {
  int32_t  state = vm.popDataValue();
  auto&    tg    = popString(vm);
  auto     npc   = popInstance(vm);
  if(npc!=nullptr)
    npc->aiUseMob(tg,state);
  }

void WorldScript::ai_teleport(Daedalus::DaedalusVM &vm) {
  auto&    tg  = popString(vm);
  auto     npc = popInstance(vm);
  auto     pt  = owner.findPoint(tg);
  if(npc!=nullptr && pt!=nullptr)
    npc->aiTeleport(*pt);
  }

void WorldScript::ai_stoppointat(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  (void)npc;
  // TODO: stub
  }

void WorldScript::ai_readymeleeweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiReadyMeleWeapon();
  }

void WorldScript::ai_readyrangedweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiReadyRangeWeapon();
  }

void WorldScript::ai_readyspell(Daedalus::DaedalusVM &vm) {
  auto mana  = vm.popDataValue();
  auto spell = vm.popDataValue();
  auto npc   = popInstance(vm);
  if(npc!=nullptr)
    npc->aiReadySpell(spell,mana);
  }

void WorldScript::ai_atack(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiAtack();
  }

void WorldScript::ai_flee(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiFlee();
  }

void WorldScript::ai_dodge(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiDodge();
  }

void WorldScript::ai_unequipweapons(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiUnEquipWeapons();
  }

void WorldScript::ai_gotonpc(Daedalus::DaedalusVM &vm) {
  auto to  = popInstance(vm);
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiGoToNpc(to);
  }

void WorldScript::mob_hasitems(Daedalus::DaedalusVM &vm) {
  uint32_t item = vm.popVar();
  auto&    tag  = popString(vm);
  vm.setReturn(int(owner.hasItems(tag,item)));
  }

void WorldScript::ta_min(Daedalus::DaedalusVM &vm) {
  auto&    waypoint = popString(vm);
  int32_t  action   = vm.popDataValue();
  int32_t  stop_m   = vm.popDataValue();
  int32_t  stop_h   = vm.popDataValue();
  int32_t  start_m  = vm.popDataValue();
  int32_t  start_h  = vm.popDataValue();
  auto     npc      = popInstance(vm);

  // TODO
  auto at=owner.findPoint(waypoint);
  if(npc!=nullptr && at!=nullptr){
    npc->setPosition (at->position.x, at->position.y, at->position.z);
    npc->setDirection(at->direction.x,at->direction.y,at->direction.z);
    }

  npc->addRoutine(gtime(start_h,start_m),gtime(stop_h,stop_m),uint32_t(action),at);
  }

void WorldScript::log_createtopic(Daedalus::DaedalusVM &vm) {
  int32_t section   = vm.popDataValue();
  auto&   topicName = popString(vm);

  quests.add(topicName,QuestLog::Section(section));
  }

void WorldScript::log_settopicstatus(Daedalus::DaedalusVM &vm) {
  int32_t status    = vm.popDataValue();
  auto&   topicName = popString(vm);

  quests.setStatus(topicName,QuestLog::Status(status));
  }

void WorldScript::log_addentry(Daedalus::DaedalusVM &vm) {
  auto&   entry     = popString(vm);
  auto&   topicName = popString(vm);

  quests.addEntry(topicName,entry);
  }

void WorldScript::equipitem(Daedalus::DaedalusVM &vm) {
  uint32_t cls  = uint32_t(vm.popDataValue());
  auto     self = popInstance(vm);

  if(self!=nullptr) {
    if(self->hasItem(cls)==0)
      self->addItem(cls,1);
    self->useItem(cls,true);
    //self->aiUseItem(int32_t(cls)); // avoid recursive vm call; FIXME: recursive vm calls
    }
  }

void WorldScript::createinvitem(Daedalus::DaedalusVM &vm) {
  uint32_t itemInstance = uint32_t(vm.popDataValue());
  auto     self         = popInstance(vm);
  if(self!=nullptr)
    self->addItem(itemInstance,1);
  }

void WorldScript::createinvitems(Daedalus::DaedalusVM &vm) {
  uint32_t amount       = uint32_t(vm.popDataValue());
  uint32_t itemInstance = uint32_t(vm.popDataValue());
  auto     self         = popInstance(vm);
  if(self!=nullptr)
    self->addItem(itemInstance,amount);
  }

void WorldScript::hlp_getinstanceid(Daedalus::DaedalusVM &vm) {
  auto self = popInstance(vm);

  if(self!=nullptr){
    auto v = vmNpc(self->handle());
    vm.setReturn(int32_t(v.instanceSymbol));
    return;
    }
  vm.setReturn(-1);
  }

void WorldScript::hlp_isvalidnpc(Daedalus::DaedalusVM &vm) {
  auto self = popInstance(vm);
  if(self!=nullptr)
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void WorldScript::hlp_isitem(Daedalus::DaedalusVM &vm) {
  uint32_t instanceSymbol = vm.popVar();
  auto     item           = popItem(vm);
  if(item!=nullptr){
    auto& v = vmItem(item->handle());
    vm.setReturn(v.instanceSymbol==instanceSymbol ? 1 : 0);
    } else
    vm.setReturn(0);
  }

void WorldScript::hlp_isvaliditem(Daedalus::DaedalusVM &vm) {
  auto item = popItem(vm);
  if(item!=nullptr)
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void WorldScript::hlp_getnpc(Daedalus::DaedalusVM &vm) {
  uint32_t instanceSymbol = vm.popVar();
  auto     handle         = vm.getDATFile().getSymbolByIndex(instanceSymbol);

  if(auto npc = getNpcById(instanceSymbol))
    vm.setReturn(int32_t(instanceSymbol)); else
    vm.setReturn(-1);
  }

void WorldScript::info_addchoice(Daedalus::DaedalusVM &vm) {
  uint32_t func         = vm.popVar();
  auto&    text         = popString(vm);
  uint32_t infoInstance = uint32_t(vm.popDataValue());

  ZMemory::BigHandle                h     = vm.getDATFile().getSymbolByIndex(infoInstance).instanceDataHandle;
  Daedalus::GameState::InfoHandle   hInfo = ZMemory::handleCast<Daedalus::GameState::InfoHandle>(h);
  Daedalus::GEngineClasses::C_Info& cInfo = vm.getGameState().getInfo(hInfo);

  cInfo.addChoice(Daedalus::GEngineClasses::SubChoice{text, func});
  }

void WorldScript::info_clearchoices(Daedalus::DaedalusVM &vm) {
  uint32_t infoInstance = uint32_t(vm.popDataValue());

  ZMemory::BigHandle                h     = vm.getDATFile().getSymbolByIndex(infoInstance).instanceDataHandle;
  Daedalus::GameState::InfoHandle   hInfo = ZMemory::handleCast<Daedalus::GameState::InfoHandle>(h);
  Daedalus::GEngineClasses::C_Info& cInfo = vm.getGameState().getInfo(hInfo);

  cInfo.subChoices.clear();
  }

void WorldScript::infomanager_hasfinished(Daedalus::DaedalusVM &vm) {
  vm.setReturn(owner.aiIsDlgFinished() ? 1 : 0);
  }

void WorldScript::introducechapter(Daedalus::DaedalusVM &vm) {
  int32_t waittime = vm.popDataValue();
  auto&   sound    = popString(vm);
  auto&   texture  = popString(vm);
  auto&   subtitle = popString(vm);
  auto&   title    = popString(vm);
  Log::i("chapter screen not implemented [",subtitle,", ",texture,"]");
  }

void WorldScript::playvideo(Daedalus::DaedalusVM &vm) {
  const std::string& filename = popString(vm);
  Log::i("video not implemented [",filename,"]");
  vm.setReturn(0);
  }

void WorldScript::printscreen(Daedalus::DaedalusVM &vm) {
  int32_t            timesec  = vm.popDataValue();
  const std::string& font     = popString(vm);
  int32_t            posy     = vm.popDataValue();
  int32_t            posx     = vm.popDataValue();
  const std::string& msg      = popString(vm);
  int32_t            dialognr = vm.popDataValue();
  (void)dialognr;
  owner.printScreen(msg.c_str(),posx,posy,timesec,Resources::fontByName(font));
  }

void WorldScript::print(Daedalus::DaedalusVM &vm) {
  const std::string& msg = popString(vm);
  owner.print(msg.c_str());
  }

void WorldScript::perc_setrange(Daedalus::DaedalusVM &) {
  Log::i("perc_setrange");
  }

void WorldScript::sort(std::vector<WorldScript::DlgChoise> &dlg) {
  std::sort(dlg.begin(),dlg.end(),[](const WorldScript::DlgChoise& l,const WorldScript::DlgChoise& r){
    return l.sort<r.sort;
    });
  }

void WorldScript::setNpcInfoKnown(const Daedalus::GEngineClasses::C_Npc& npc, const Daedalus::GEngineClasses::C_Info &info) {
  auto id = std::make_pair(npc.instanceSymbol,info.instanceSymbol);
  dlgKnownInfos.insert(id);
  }

bool WorldScript::doesNpcKnowInfo(const Daedalus::GEngineClasses::C_Npc& npc, size_t infoInstance) const {
  auto id = std::make_pair(npc.instanceSymbol,infoInstance);
  return dlgKnownInfos.find(id)!=dlgKnownInfos.end();
  }
