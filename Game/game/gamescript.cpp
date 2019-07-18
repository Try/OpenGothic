#include "gamescript.h"

#include "gothic.h"
#include "world/npc.h"
#include "world/item.h"
#include "game/serialize.h"
#include "game/spelldefinitions.h"
#include "utils/cp1251.h"

#include <fstream>
#include <Tempest/Log>
#include <Tempest/SoundEffect>

using namespace Tempest;
using namespace Daedalus::GameState;

struct GameScript::ScopeVar final {
  ScopeVar(Daedalus::DaedalusVM& vm,Daedalus::PARSymbol& sym,Npc& n)
    :ScopeVar(vm,sym,n.handle(),Daedalus::IC_Npc){
    }

  ScopeVar(Daedalus::DaedalusVM& vm,Daedalus::PARSymbol& sym,Npc* n)
    :ScopeVar(vm,sym,n ? n->handle() : nullptr, Daedalus::IC_Npc){
    }

  ScopeVar(Daedalus::DaedalusVM& vm,const char* name, Daedalus::GEngineClasses::C_Npc* h)
    :ScopeVar(vm,vm.getDATFile().getSymbolByName(name),h,Daedalus::IC_Npc){
    }

  ScopeVar(Daedalus::DaedalusVM& vm,Daedalus::PARSymbol& sym, Daedalus::GEngineClasses::Instance* h, Daedalus::EInstanceClass instanceClass)
    :vm(vm),sym(sym) {
    cls    = sym.instanceDataClass;
    handle = sym.instanceDataHandle;
    sym.instanceDataHandle = h;
    sym.instanceDataClass  = instanceClass;
    }

  ScopeVar(const ScopeVar&)=delete;
  ~ScopeVar(){
    sym.instanceDataHandle = handle;
    sym.instanceDataClass  = cls;
    }

  Daedalus::GEngineClasses::Instance* handle=nullptr;
  Daedalus::EInstanceClass            cls   =Daedalus::IC_None;
  Daedalus::DaedalusVM&               vm;
  Daedalus::PARSymbol&                sym;
  };


bool GameScript::GlobalOutput::output(Npc& npc,const std::string &text) {
  return owner.aiOutput(npc,text);
  }

bool GameScript::GlobalOutput::outputSvm(Npc &npc, const std::string &text, int voice) {
  return owner.aiOutputSvm(npc,text,voice);
  }

bool GameScript::GlobalOutput::outputOv(Npc &npc, const std::string &text, int voice) {
  return owner.aiOutputSvm(npc,text,voice);
  }

bool GameScript::GlobalOutput::isFinished() {
  return true;
  }

GameScript::GameScript(GameSession &owner)
  :vm(owner.loadScriptCode()),owner(owner){
  Daedalus::registerGothicEngineClasses(vm);
  aiDefaultPipe.reset(new GlobalOutput(*this));
  initCommon();
  }

GameScript::GameScript(GameSession &owner, Serialize &fin)
  :GameScript(owner){
  uint32_t sz=0;
  fin.read(sz);
  for(size_t i=0;i<sz;++i){
    uint32_t f=0,s=0;
    fin.read(f,s);
    dlgKnownInfos.insert(std::make_pair(f,s));
    }

  fin.read(gilAttitudes);
  }

void GameScript::initCommon() {
  vm.registerUnsatisfiedLink([this](Daedalus::DaedalusVM& vm){return notImplementedRoutine(vm);});

  vm.registerExternalFunction("concatstrings", &GameScript::concatstrings);
  vm.registerExternalFunction("inttostring",   &GameScript::inttostring  );
  vm.registerExternalFunction("floattostring", &GameScript::floattostring);
  vm.registerExternalFunction("inttofloat",    &GameScript::inttofloat   );
  vm.registerExternalFunction("floattoint",    &GameScript::floattoint   );

  vm.registerExternalFunction("hlp_random",          [this](Daedalus::DaedalusVM& vm){ hlp_random(vm);         });
  vm.registerExternalFunction("hlp_strcmp",          &GameScript::hlp_strcmp                                   );
  vm.registerExternalFunction("hlp_isvalidnpc",      [this](Daedalus::DaedalusVM& vm){ hlp_isvalidnpc(vm);     });
  vm.registerExternalFunction("hlp_isvaliditem",     [this](Daedalus::DaedalusVM& vm){ hlp_isvaliditem(vm);    });
  vm.registerExternalFunction("hlp_isitem",          [this](Daedalus::DaedalusVM& vm){ hlp_isitem(vm);         });
  vm.registerExternalFunction("hlp_getnpc",          [this](Daedalus::DaedalusVM& vm){ hlp_getnpc(vm);         });
  vm.registerExternalFunction("hlp_getinstanceid",   [this](Daedalus::DaedalusVM& vm){ hlp_getinstanceid(vm);  });

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
  vm.registerExternalFunction("wld_isnextfpavailable",
                                                     [this](Daedalus::DaedalusVM& vm){ wld_isnextfpavailable(vm);    });
  vm.registerExternalFunction("wld_ismobavailable",  [this](Daedalus::DaedalusVM& vm){ wld_ismobavailable(vm);       });
  vm.registerExternalFunction("wld_setmobroutine",   [this](Daedalus::DaedalusVM& vm){ wld_setmobroutine(vm);        });
  vm.registerExternalFunction("wld_assignroomtoguild",
                                                     [this](Daedalus::DaedalusVM& vm){ wld_assignroomtoguild(vm);    });
  vm.registerExternalFunction("wld_detectnpc",       [this](Daedalus::DaedalusVM& vm){ wld_detectnpc(vm);            });
  vm.registerExternalFunction("wld_detectitem",      [this](Daedalus::DaedalusVM& vm){ wld_detectitem(vm);           });

  vm.registerExternalFunction("mdl_setvisual",       [this](Daedalus::DaedalusVM& vm){ mdl_setvisual(vm);        });
  vm.registerExternalFunction("mdl_setvisualbody",   [this](Daedalus::DaedalusVM& vm){ mdl_setvisualbody(vm);    });
  vm.registerExternalFunction("mdl_setmodelfatness", [this](Daedalus::DaedalusVM& vm){ mdl_setmodelfatness(vm);  });
  vm.registerExternalFunction("mdl_applyoverlaymds", [this](Daedalus::DaedalusVM& vm){ mdl_applyoverlaymds(vm);  });
  vm.registerExternalFunction("mdl_applyoverlaymdstimed",
                                                     [this](Daedalus::DaedalusVM& vm){ mdl_applyoverlaymdstimed(vm); });
  vm.registerExternalFunction("mdl_removeoverlaymds",[this](Daedalus::DaedalusVM& vm){ mdl_removeoverlaymds(vm); });
  vm.registerExternalFunction("mdl_setmodelscale",   [this](Daedalus::DaedalusVM& vm){ mdl_setmodelscale(vm);    });
  vm.registerExternalFunction("mdl_startfaceani",    [this](Daedalus::DaedalusVM& vm){ mdl_startfaceani(vm);     });
  vm.registerExternalFunction("mdl_applyrandomani",  [this](Daedalus::DaedalusVM& vm){ mdl_applyrandomani(vm);   });
  vm.registerExternalFunction("mdl_applyrandomanifreq",
                                                     [this](Daedalus::DaedalusVM& vm){ mdl_applyrandomanifreq(vm);});

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
  vm.registerExternalFunction("npc_getactivespellcat",
                                                     [this](Daedalus::DaedalusVM& vm){ npc_getactivespellcat(vm); });
  vm.registerExternalFunction("npc_setactivespellinfo",
                                                     [this](Daedalus::DaedalusVM& vm){ npc_setactivespellinfo(vm); });

  vm.registerExternalFunction("npc_canseenpcfreelos",[this](Daedalus::DaedalusVM& vm){ npc_canseenpcfreelos(vm); });
  vm.registerExternalFunction("npc_isinfightmode",   [this](Daedalus::DaedalusVM& vm){ npc_isinfightmode(vm);    });
  vm.registerExternalFunction("npc_settarget",       [this](Daedalus::DaedalusVM& vm){ npc_settarget(vm);        });
  vm.registerExternalFunction("npc_gettarget",       [this](Daedalus::DaedalusVM& vm){ npc_gettarget(vm);        });
  vm.registerExternalFunction("npc_getnexttarget",   [this](Daedalus::DaedalusVM& vm){ npc_getnexttarget(vm);    });
  vm.registerExternalFunction("npc_sendpassiveperc", [this](Daedalus::DaedalusVM& vm){ npc_sendpassiveperc(vm);  });
  vm.registerExternalFunction("npc_checkinfo",       [this](Daedalus::DaedalusVM& vm){ npc_checkinfo(vm);        });
  vm.registerExternalFunction("npc_getportalguild",  [this](Daedalus::DaedalusVM& vm){ npc_getportalguild(vm);   });
  vm.registerExternalFunction("npc_isinplayersroom", [this](Daedalus::DaedalusVM& vm){ npc_isinplayersroom(vm);  });
  vm.registerExternalFunction("npc_getreadiedweapon",[this](Daedalus::DaedalusVM& vm){ npc_getreadiedweapon(vm); });
  vm.registerExternalFunction("npc_isdrawingspell",  [this](Daedalus::DaedalusVM& vm){ npc_isdrawingspell(vm);   });
  vm.registerExternalFunction("npc_perceiveall",     [this](Daedalus::DaedalusVM& vm){ npc_perceiveall(vm);      });
  vm.registerExternalFunction("npc_stopani",         [this](Daedalus::DaedalusVM& vm){ npc_stopani(vm);          });
  vm.registerExternalFunction("npc_settrueguild",    [this](Daedalus::DaedalusVM& vm){ npc_settrueguild(vm);     });
  vm.registerExternalFunction("npc_gettrueguild",    [this](Daedalus::DaedalusVM& vm){ npc_gettrueguild(vm);     });
  vm.registerExternalFunction("npc_clearinventory",  [this](Daedalus::DaedalusVM& vm){ npc_clearinventory(vm);   });
  vm.registerExternalFunction("npc_getattitude",     [this](Daedalus::DaedalusVM& vm){ npc_getattitude(vm);      });
  vm.registerExternalFunction("npc_getpermattitude", [this](Daedalus::DaedalusVM& vm){ npc_getpermattitude(vm);  });
  vm.registerExternalFunction("npc_setattitude",     [this](Daedalus::DaedalusVM& vm){ npc_setattitude(vm);      });
  vm.registerExternalFunction("npc_settempattitude", [this](Daedalus::DaedalusVM& vm){ npc_settempattitude(vm);  });
  vm.registerExternalFunction("npc_hasbodyflag",     [this](Daedalus::DaedalusVM& vm){ npc_hasbodyflag(vm);      });
  vm.registerExternalFunction("npc_getlasthitspellid",
                                                     [this](Daedalus::DaedalusVM& vm){ npc_getlasthitspellid(vm);});
  vm.registerExternalFunction("npc_getlasthitspellcat",
                                                     [this](Daedalus::DaedalusVM& vm){ npc_getlasthitspellcat(vm);});

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
  vm.registerExternalFunction("ai_equipbestarmor",   [this](Daedalus::DaedalusVM& vm){ ai_equipbestarmor(vm);    });
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
  vm.registerExternalFunction("ai_unequiparmor",     [this](Daedalus::DaedalusVM& vm){ ai_unequiparmor(vm);      });
  vm.registerExternalFunction("ai_gotonpc",          [this](Daedalus::DaedalusVM& vm){ ai_gotonpc(vm);           });
  vm.registerExternalFunction("ai_gotonextfp",       [this](Daedalus::DaedalusVM& vm){ ai_gotonextfp(vm);        });
  vm.registerExternalFunction("ai_aligntofp",        [this](Daedalus::DaedalusVM& vm){ ai_aligntofp(vm);         });
  vm.registerExternalFunction("ai_useitem",          [this](Daedalus::DaedalusVM& vm){ ai_useitem(vm);           });
  vm.registerExternalFunction("ai_useitemtostate",   [this](Daedalus::DaedalusVM& vm){ ai_useitemtostate(vm);    });
  vm.registerExternalFunction("ai_setnpcstostate",   [this](Daedalus::DaedalusVM& vm){ ai_setnpcstostate(vm);    });

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

  vm.registerExternalFunction("snd_play",            [this](Daedalus::DaedalusVM& vm){ snd_play(vm);             });

  vm.registerExternalFunction("introducechapter",    [this](Daedalus::DaedalusVM& vm){ introducechapter(vm);     });
  vm.registerExternalFunction("playvideo",           [this](Daedalus::DaedalusVM& vm){ playvideo(vm);            });
  vm.registerExternalFunction("printscreen",         [this](Daedalus::DaedalusVM& vm){ printscreen(vm);          });
  vm.registerExternalFunction("printdialog",         [this](Daedalus::DaedalusVM& vm){ printdialog(vm);          });
  vm.registerExternalFunction("print",               [this](Daedalus::DaedalusVM& vm){ print(vm);                });
  vm.registerExternalFunction("perc_setrange",       [this](Daedalus::DaedalusVM& vm){ perc_setrange(vm);        });

  vm.registerExternalFunction("printdebug",          [this](Daedalus::DaedalusVM& vm){ printdebug(vm);           });
  vm.registerExternalFunction("printdebugch",        [this](Daedalus::DaedalusVM& vm){ printdebugch(vm);         });
  vm.registerExternalFunction("printdebuginst",      [this](Daedalus::DaedalusVM& vm){ printdebuginst(vm);       });
  vm.registerExternalFunction("printdebuginstch",    [this](Daedalus::DaedalusVM& vm){ printdebuginstch(vm);     });

  vm.registerExternalFunction("game_initgerman",     [this](Daedalus::DaedalusVM& vm){ game_initgerman(vm);      });
  vm.registerExternalFunction("game_initenglish",    [this](Daedalus::DaedalusVM& vm){ game_initenglish(vm);     });

  spellFxInstanceNames = vm.getDATFile().getSymbolIndexByName("spellFxInstanceNames");
  spellFxAniLetters    = vm.getDATFile().getSymbolIndexByName("spellFxAniLetters");

  spells               = std::make_unique<SpellDefinitions>(vm);
  svm                  = std::make_unique<SvmDefinitions>(vm);

  cFocusNorm           = getFocus("Focus_Normal");
  cFocusMele           = getFocus("Focus_Melee");
  cFocusRange          = getFocus("Focus_Ranged");
  cFocusMage           = getFocus("Focus_Magic");

  ZS_Dead              = getAiState(getSymbolIndex("ZS_Dead")).funcIni;
  ZS_Unconscious       = getAiState(getSymbolIndex("ZS_Unconscious")).funcIni;
  ZS_Talk              = getAiState(getSymbolIndex("ZS_Talk")).funcIni;

  auto& dat = vm.getDATFile();

  if(owner.isGothic2()){
    auto& currency = dat.getSymbolByName("TRADE_CURRENCY_INSTANCE");
    itMi_Gold      = dat.getSymbolIndexByName(currency.getString(0).c_str());
    if(itMi_Gold!=size_t(-1)){ // FIXME
      Daedalus::GEngineClasses::C_Item item={};
      vm.initializeInstance(item, itMi_Gold, Daedalus::IC_Item);
      goldTxt = cp1251::toUtf8(item.name);
      }
    auto& tradeMul = dat.getSymbolByName("TRADE_VALUE_MULTIPLIER");
    tradeValMult   = tradeMul.getFloat();

    auto& vtime     = dat.getSymbolByName("VIEW_TIME_PER_CHAR");
    viewTimePerChar = vtime.getFloat(0);
    if(viewTimePerChar<=0.f)
      viewTimePerChar=0.55f;
    } else {
    tradeValMult   = 1.f;
    viewTimePerChar=0.55f;
    }

  auto gilMax = dat.getSymbolByName("GIL_MAX");
  gilCount=size_t(gilMax.getInt(0));

  auto tableSz = dat.getSymbolByName("TAB_ANZAHL");
  auto guilds  = dat.getSymbolByName("GIL_ATTITUDES");
  gilAttitudes.resize(gilCount*gilCount,ATT_HOSTILE);

  size_t tbSz=size_t(std::sqrt(tableSz.getInt()));
  for(size_t i=0;i<tbSz;++i)
    for(size_t r=0;r<tbSz;++r) {
      gilAttitudes[i*gilCount+r]=guilds.getInt(i*tbSz+r);
      gilAttitudes[r*gilCount+i]=guilds.getInt(r*tbSz+i);
      }

  auto id = dat.getSymbolIndexByName("Gil_Values");
  if(id!=size_t(-1)){
    vm.initializeInstance(cGuildVal, id, Daedalus::IC_GilValues);
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

  if(hasSymbolName("startup_global"))
    runFunction("startup_global");
  }

void GameScript::initDialogs(Gothic& gothic) {
  loadDialogOU(gothic);
  if(!dialogs)
    dialogs.reset(new ZenLoad::zCCSLib(""));

  size_t count=0;
  vm.getDATFile().iterateSymbolsOfClass("C_Info", [&count](size_t,Daedalus::PARSymbol&){
    ++count;
    });
  dialogsInfo.resize(count);

  count=0;
  vm.getDATFile().iterateSymbolsOfClass("C_Info", [this,&count](size_t i,Daedalus::PARSymbol&){
    Daedalus::GEngineClasses::C_Info& h = dialogsInfo[count];
    vm.initializeInstance(h, i, Daedalus::IC_Info);
    ++count;
    });
  }

void GameScript::loadDialogOU(Gothic &gothic) {
  const char16_t* names[]={u"OU.DAT",u"OU.BIN"};
  const char16_t* dir  []={u"_work/data/Scripts/content/CUTSCENE/",u"_work/DATA/scripts/content/CUTSCENE/"};

  for(auto n:names){
    for(auto d:dir){
      std::u16string full = gothic.path()+u"/"+d+n;
      bool exist=false;
      {
      try {
        RFile f(full);
        exist=f.size()>0;
        }
      catch(...){
        exist=false;
        }
      }
      if(exist){
        std::string acii(full.begin(),full.end()); //FIXME: #14 unicode support
        dialogs.reset(new ZenLoad::zCCSLib(acii));
        return;
        }
      }
    }
  }

void GameScript::initializeInstance(Daedalus::GEngineClasses::C_Npc &n, size_t instance) {
  vm.initializeInstance(n,instance,Daedalus::IC_Npc);

  if(n.daily_routine!=0) {
    ScopeVar self(vm,vm.globalSelf(),&n,Daedalus::IC_Npc);
    vm.runFunctionBySymIndex(n.daily_routine,false);
    }
  }

void GameScript::initializeInstance(Daedalus::GEngineClasses::C_Item &it,size_t instance) {
  vm.initializeInstance(it,instance,Daedalus::IC_Item);
  }

void GameScript::save(Serialize &fout) {
  fout.write(uint32_t(dlgKnownInfos.size()));
  for(auto& i:dlgKnownInfos)
    fout.write(uint32_t(i.first),uint32_t(i.second));

  fout.write(uint32_t(gilAttitudes.size()));
  for(auto& i:gilAttitudes)
    fout.write(i);
  }

void GameScript::saveVar(Serialize &fout) {
  auto&  dat = vm.getDATFile().getSymTable().symbols;
  fout.write(uint32_t(dat.size()));
  for(auto& i:dat){
    saveSym(fout,i);
    }
  }

void GameScript::loadVar(Serialize &fin) {
  std::string name;
  uint32_t sz=0;
  fin.read(sz);
  for(size_t i=0;i<sz;++i){
    uint32_t t=Daedalus::EParType::EParType_Void;
    fin.read(t);
    switch(t) {
      case Daedalus::EParType::EParType_Int:{
        fin.read(name);
        auto& s = getSymbol(name.c_str());
        fin.read(s.intData);
        break;
        }
      case Daedalus::EParType::EParType_Float:{
        fin.read(name);
        auto& s = getSymbol(name.c_str());
        fin.read(s.floatData);
        break;
        }
      case Daedalus::EParType::EParType_String:{
        fin.read(name);
        auto& s = getSymbol(name.c_str());
        fin.read(s.strData);
        break;
        }
      case Daedalus::EParType::EParType_Instance:{
        uint8_t dataClass=0;
        fin.read(dataClass);
        if(dataClass>0){
          uint32_t id=0;
          fin.read(name,id);
          auto& s = getSymbol(name.c_str());
          if(dataClass==1) {
            auto npc = world().npcById(id);
            s.instanceDataClass  = Daedalus::IC_Npc;
            s.instanceDataHandle = npc ? npc->handle() : nullptr;
            }
          else if(dataClass==2) {
            auto itm = world().itmById(id);
            s.instanceDataClass  = Daedalus::IC_Item;
            s.instanceDataHandle = itm ? itm->handle() : nullptr;
            }
          }
        break;
        }
      }
    }
  }

void GameScript::resetVarPointers() {
  auto&  dat = vm.getDATFile().getSymTable().symbols;
  for(size_t i=0;i<dat.size();++i){
    auto& s = vm.getDATFile().getSymbolByIndex(i);
    if(s.properties.elemProps.type!=Daedalus::EParType::EParType_Instance && s.instanceDataHandle)
      Log::i("");
    if(s.name=="SLEEPABIT_S1.ROCK")
      Log::i("");
    if(s.instanceDataClass==Daedalus::IC_Npc || s.instanceDataClass==Daedalus::IC_Item){
      s.instanceDataHandle = nullptr;
      }
    }
  }

void GameScript::saveSym(Serialize &fout,const Daedalus::PARSymbol &i) {
  switch(i.properties.elemProps.type) {
    case Daedalus::EParType::EParType_Int:
      if(i.intData.size()>0){
        fout.write(i.properties.elemProps.type);
        fout.write(i.name,i.intData);
        return;
        }
      break;
    case Daedalus::EParType::EParType_Float:
      if(i.floatData.size()>0){
        fout.write(i.properties.elemProps.type);
        fout.write(i.name,i.floatData);
        return;
        }
      break;
    case Daedalus::EParType::EParType_String:
      if(i.strData.size()>0){
        fout.write(i.properties.elemProps.type);
        fout.write(i.name,i.strData);
        return;
        }
      break;
    case Daedalus::EParType::EParType_Instance:
      fout.write(i.properties.elemProps.type);
      if(i.instanceDataClass==Daedalus::IC_None){
        fout.write(uint8_t(0));
        }
      else if(i.instanceDataClass==Daedalus::IC_Npc){
        fout.write(uint8_t(1),i.name,world().npcId(i.instanceDataHandle));
        }
      else if(i.instanceDataClass==Daedalus::IC_Item){
        fout.write(uint8_t(2),i.name,world().itmId(i.instanceDataHandle));
        }
      else if(i.instanceDataClass==Daedalus::IC_Focus || i.instanceDataClass==Daedalus::IC_GilValues ||
              i.instanceDataClass==Daedalus::IC_Info) {
        fout.write(uint8_t(0));
        }
      else {
        fout.write(uint8_t(0));
        }
      return;
    }
  fout.write(uint32_t(Daedalus::EParType::EParType_Void));
  }

const World &GameScript::world() const {
  return *owner.world();
  }

World &GameScript::world() {
  return *owner.world();
  }

Daedalus::GEngineClasses::C_Focus GameScript::getFocus(const char *name) {
  Daedalus::GEngineClasses::C_Focus ret={};
  auto id = vm.getDATFile().getSymbolIndexByName(name);
  if(id==size_t(-1))
    return ret;
  vm.initializeInstance(ret, id, Daedalus::IC_Focus);
  return ret;
  }

void GameScript::storeItem(Item *itm) {
  Daedalus::PARSymbol& s = vm.globalItem();
  if(itm!=nullptr) {
    s.instanceDataHandle = itm->handle();
    s.instanceDataClass  = Daedalus::IC_Item;
    } else {
    s.instanceDataHandle = nullptr;
    s.instanceDataClass  = Daedalus::IC_Item;
    }
  }

Daedalus::PARSymbol &GameScript::getSymbol(const char *s) {
  return vm.getDATFile().getSymbolByName(s);
  }

Daedalus::PARSymbol &GameScript::getSymbol(const size_t s) {
  return vm.getDATFile().getSymbolByIndex(s);
  }

size_t GameScript::getSymbolIndex(const char* s) {
  return vm.getDATFile().getSymbolIndexByName(s);
  }

size_t GameScript::getSymbolIndex(const std::string &s) {
  return vm.getDATFile().getSymbolIndexByName(s.c_str());
  }

const AiState &GameScript::getAiState(size_t id) {
  auto it = aiStates.find(id);
  if(it!=aiStates.end())
    return it->second;
  auto ins = aiStates.emplace(id,AiState(*this,id));
  return ins.first->second;
  }

const Daedalus::GEngineClasses::C_Spell &GameScript::getSpell(int32_t splId) {
  auto& spellInst = vm.getDATFile().getSymbolByIndex(spellFxInstanceNames);
  auto& tag       = spellInst.getString(size_t(splId));
  return spells->find(tag);
  }

std::vector<GameScript::DlgChoise> GameScript::dialogChoises(Daedalus::GEngineClasses::C_Npc* player,
                                                               Daedalus::GEngineClasses::C_Npc* hnpc,
                                                               const std::vector<uint32_t>& except,
                                                               bool includeImp) {
  auto& npc = *hnpc;
  auto& pl  = *player;

  ScopeVar self (vm, vm.globalSelf(),  hnpc,   Daedalus::IC_Npc);
  ScopeVar other(vm, vm.globalOther(), player, Daedalus::IC_Npc);

  std::vector<Daedalus::GEngineClasses::C_Info*> hDialog;
  for(auto& info : dialogsInfo) {
    if(info.npc==int32_t(npc.instanceSymbol)) {
      hDialog.push_back(&info);
      }
    }

  std::vector<DlgChoise> choise;

  for(int important=includeImp ? 1 : 0;important>=0;--important){
    for(auto& i:hDialog) {
      const Daedalus::GEngineClasses::C_Info& info = *i;
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
        ch.title    = cp1251::toUtf8(info.description);
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

std::vector<GameScript::DlgChoise> GameScript::updateDialog(const GameScript::DlgChoise &dlg, Npc& /*pl*/,Npc&) {
  if(dlg.handle==nullptr)
    return {};
  const Daedalus::GEngineClasses::C_Info& info = *dlg.handle;
  std::vector<GameScript::DlgChoise>     ret;

  for(size_t i=0;i<info.subChoices.size();++i){
    auto& sub = info.subChoices[i];
    //bool npcKnowsInfo = doesNpcKnowInfo(pl.instanceSymbol(),info.instanceSymbol);
    //if(npcKnowsInfo)
    //  continue;

    bool valid=false;
    if(info.condition)
      valid = runFunction(info.condition)!=0;

    GameScript::DlgChoise ch;
    ch.title    = cp1251::toUtf8(sub.text);
    ch.scriptFn = sub.functionSym;
    ch.handle   = dlg.handle;
    ch.isTrade  = false;
    ch.sort     = int(i);
    ret.push_back(ch);
    }

  sort(ret);
  return ret;
  }

void GameScript::exec(const GameScript::DlgChoise &dlg,
                       Daedalus::GEngineClasses::C_Npc* player,
                       Daedalus::GEngineClasses::C_Npc* hnpc) {
  ScopeVar self (vm, vm.globalSelf(),  hnpc,   Daedalus::IC_Npc);
  ScopeVar other(vm, vm.globalOther(), player, Daedalus::IC_Npc);

  Daedalus::GEngineClasses::C_Info& info = *dlg.handle;

  auto pl = *player;
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

int GameScript::printCannotUseError(Npc& npc, int32_t atr, int32_t nValue) {
  auto id = vm.getDATFile().getSymbolIndexByName("G_CanNotUse");
  if(id==size_t(-1))
    return 0;
  vm.pushInt(npc.isPlayer() ? 1 : 0);
  vm.pushInt(atr);
  vm.pushInt(nValue);
  ScopeVar self(vm, vm.globalSelf(), npc.handle(), Daedalus::IC_Npc);
  return runFunction(id,false);
  }

int GameScript::printCannotCastError(Npc &npc, int32_t plM, int32_t itM) {
  auto id = vm.getDATFile().getSymbolIndexByName("G_CanNotCast");
  if(id==size_t(-1))
    return 0;
  vm.pushInt(npc.isPlayer() ? 1 : 0);
  vm.pushInt(itM);
  vm.pushInt(plM);
  ScopeVar self(vm, vm.globalSelf(), npc.handle(), Daedalus::IC_Npc);
  return runFunction(id,false);
  }

int GameScript::printCannotBuyError(Npc &npc) {
  auto id = vm.getDATFile().getSymbolIndexByName("player_trade_not_enough_gold");
  if(id==size_t(-1))
    return 0;
  ScopeVar self(vm, vm.globalSelf(), npc.handle(), Daedalus::IC_Npc);
  return runFunction(id,true);
  }

int GameScript::invokeState(Daedalus::GEngineClasses::C_Npc* hnpc, Daedalus::GEngineClasses::C_Npc* oth, const char *name) {
  auto& dat = vm.getDATFile();
  auto  id  = dat.getSymbolIndexByName(name);
  if(id==size_t(-1))
    return 0;

  ScopeVar self (vm, vm.globalSelf(),  hnpc, Daedalus::IC_Npc);
  ScopeVar other(vm, vm.globalOther(), oth,  Daedalus::IC_Npc);
  return runFunction(id);
  }

int GameScript::invokeState(Npc* npc, Npc* oth, Npc* vic, size_t fn) {
  if(fn==size_t(-1))
    return 0;
  if(oth==nullptr){
    oth=npc; //FIXME: PC_Levelinspektor?
    }
  if(vic==nullptr)
    vic=owner.player();

  if(fn==ZS_Talk){
    if(!oth->isPlayer())
      Log::e("unxepected perc acton");
    }

  ScopeVar self  (vm, vm.globalSelf(),   npc);
  ScopeVar other (vm, vm.globalOther(),  oth);
  ScopeVar victum(vm, vm.globalVictim(), vic);
  const int ret = runFunction(fn);
  if(vm.globalOther().instanceDataClass==Daedalus::IC_Npc){
    auto oth2 = reinterpret_cast<Daedalus::GEngineClasses::C_Npc*>(vm.globalOther().instanceDataHandle);
    if(oth2!=oth->handle()) {
      Npc* other = getNpc(oth2);
      npc->setOther(other);
      }
    }
  return ret;
  }

int GameScript::invokeItem(Npc *npc,size_t fn) {
  if(fn==size_t(-1))
    return 1;
  ScopeVar self(vm, vm.globalSelf(), npc);
  return runFunction(fn);
  }

int GameScript::invokeMana(Npc &npc, Npc* target, Item &) {
  auto& dat = vm.getDATFile();
  auto fn   = dat.getSymbolIndexByName("Spell_ProcessMana");
  if(fn==size_t(-1))
    return Npc::SpellCode::SPL_SENDSTOP;

  ScopeVar self (vm, vm.globalSelf(),  npc);
  ScopeVar other(vm, vm.globalOther(), target);

  vm.pushInt(npc.attribute(Npc::ATR_MANA));
  return runFunction(fn,false);
  }

int GameScript::invokeSpell(Npc &npc, Npc* target, Item &it) {
  auto& spellInst = vm.getDATFile().getSymbolByIndex(spellFxInstanceNames);
  auto& tag       = spellInst.getString(size_t(it.spellId()));
  auto  str       = "Spell_Cast_" + tag;

  auto& dat = vm.getDATFile();
  auto  fn  = dat.getSymbolIndexByName(str.c_str());
  if(fn==size_t(-1))
    return 0;

  ScopeVar self (vm, vm.globalSelf(),  npc);
  ScopeVar other(vm, vm.globalOther(), target);
  return runFunction(fn);
  }

int GameScript::spellCastAnim(Npc&, Item &it) {
  if(spellFxAniLetters==size_t(-1))
    return Npc::Anim::MagFib;
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

bool GameScript::aiUseMob(Npc &pl, const std::string &name) {
  return world().aiUseMob(pl,name);
  }

bool GameScript::aiOutput(Npc &npc, const std::string &outputname) {
  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"%s.WAV",outputname.c_str());
  npc.setAiOutputBarrier(messageTime(outputname));
  npc.emitDlgSound(buf);
  return true;
  }

bool GameScript::aiOutputSvm(Npc &npc, const std::string &outputname, int32_t voice) {
  auto& sv = svm->find(outputname.c_str(),voice);
  if(sv.size()>0)
    return aiOutput(npc,sv);
  return true;
  }

bool GameScript::isDead(const Npc &pl) {
  return pl.isState(ZS_Dead);
  }

bool GameScript::isUnconscious(const Npc &pl) {
  return pl.isState(ZS_Unconscious);
  }

bool GameScript::isTalk(const Npc &pl) {
  return pl.isState(ZS_Talk);
  }

const std::string &GameScript::messageFromSvm(const std::string &id, int voice) const {
  return svm->find(id.c_str(),voice);
  }

const std::string &GameScript::messageByName(const std::string& id) const {
  if(!dialogs->messageExists(id)){
    static std::string empty;
    return empty;
    }
  return cp1251::toUtf8(dialogs->getMessageByName(id).text);
  }

uint32_t GameScript::messageTime(const std::string &id) const {
  auto& txt = messageByName(id);
  return uint32_t(txt.size()*viewTimePerChar);
  }

int GameScript::printNothingToGet() {
  auto id = vm.getDATFile().getSymbolIndexByName("player_plunder_is_empty");
  if(id==size_t(-1))
    return 0;
  ScopeVar self(vm, vm.globalSelf(), owner.player());
  return runFunction(id,false);
  }

void GameScript::useInteractive(Daedalus::GEngineClasses::C_Npc* hnpc,const std::string& func) {
  auto& dat = vm.getDATFile();
  if(!dat.hasSymbolName(func.c_str()))
    return;

  ScopeVar self(vm,vm.globalSelf(),hnpc,Daedalus::IC_Npc);
  try {
    runFunction(func.c_str());
    }
  catch (...) {
    Log::i("unable to use interactive [",func,"]");
    }
  }

Attitude GameScript::guildAttitude(const Npc &p0, const Npc &p1) const {
  auto selfG = std::min(gilCount-1,p0.guild());
  auto npcG  = std::min(gilCount-1,p1.guild());
  auto ret   = gilAttitudes[selfG*gilCount+npcG];
  return Attitude(ret);
  }

Attitude GameScript::personAttitude(const Npc &p0, const Npc &p1) const {
  if(!p0.isPlayer() && !p1.isPlayer())
    return guildAttitude(p0,p1);

  Attitude att=ATT_NULL;
  const Npc& npc = p0.isPlayer() ? p1 : p0;
  att = npc.attitude();
  if(att!=ATT_NULL)
    return att;
  if(npc.isFriend())
    return ATT_FRIENDLY;
  att = guildAttitude(p0,p1);
  return att;
  }

bool GameScript::hasSymbolName(const char* fn) {
  return vm.getDATFile().hasSymbolName(fn);
  }

int32_t GameScript::runFunction(const char *fname) {
  auto id = vm.getDATFile().getSymbolIndexByName(fname);
  if(id==size_t(-1))
    throw std::runtime_error("script bad call");
  return runFunction(id,true);
  }

int32_t GameScript::runFunction(const size_t fid,bool clearStk) {
  if(invokeRecursive) {
    //Log::d("WorldScript: invokeRecursive");
    clearStk=false;
    //return 0;
    }
  invokeRecursive++;

  auto&       dat  = vm.getDATFile();
  auto&       sym  = dat.getSymbolByIndex(fid);
  const char* call = sym.name.c_str();(void)call; //for debuging

  int32_t ret = vm.runFunctionBySymIndex(fid,clearStk);
  invokeRecursive--;
  return ret;
  }

uint64_t GameScript::tickCount() const {
  return owner.tickCount();
  }

uint32_t GameScript::rand(uint32_t max) {
  return randGen()%max;
  }

template<class Ret,class ... Args>
std::function<Ret(Args...)> GameScript::notImplementedFn(){
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

template<void(GameScript::*)(Daedalus::DaedalusVM &vm)>
void GameScript::notImplementedFn(const char* name) {
  static bool first=true;
  if(first){
    Log::e("not implemented call [",name,"]");
    first=false;
    }
  }

void GameScript::notImplementedRoutine(Daedalus::DaedalusVM &vm) {
  static std::set<std::string> s;
  auto& fn = vm.currentCall();

  if(s.find(fn)==s.end()){
    s.insert(fn);
    Log::e("not implemented call [",fn,"]");
    }
  }

Npc* GameScript::getNpc(Daedalus::GEngineClasses::C_Npc *handle) {
  if(handle==nullptr)
    return nullptr;
  assert(handle->userPtr); // engine bug, if null
  return reinterpret_cast<Npc*>(handle->userPtr);
  }

Item *GameScript::getItem(Daedalus::GEngineClasses::C_Item* handle) {
  if(handle==nullptr)
    return nullptr;
  auto& itData = *handle;
  assert(itData.userPtr); // engine bug, if null
  return reinterpret_cast<Item*>(itData.userPtr);
  }

Item *GameScript::getItemById(size_t id) {
  auto& handle = vm.getDATFile().getSymbolByIndex(id);
  if(handle.instanceDataClass!=Daedalus::EInstanceClass::IC_Item)
    return nullptr;
  auto hnpc = reinterpret_cast<Daedalus::GEngineClasses::C_Item*>(handle.instanceDataHandle);
  return getItem(hnpc);
  }

Npc* GameScript::getNpcById(size_t id) {
  auto& handle = vm.getDATFile().getSymbolByIndex(id);
  if(handle.instanceDataClass!=Daedalus::EInstanceClass::IC_Npc)
    return nullptr;

  auto hnpc = reinterpret_cast<Daedalus::GEngineClasses::C_Npc*>(handle.instanceDataHandle);
  if(hnpc==nullptr) {
    auto obj = world().findNpcByInstance(id);
    handle.instanceDataHandle = obj ? obj->handle() : nullptr;
    hnpc = reinterpret_cast<Daedalus::GEngineClasses::C_Npc*>(handle.instanceDataHandle);
    }
  return getNpc(hnpc);
  }

Daedalus::GEngineClasses::C_Info* GameScript::getInfo(size_t id) {
  auto& sym = vm.getDATFile().getSymbolByIndex(id);
  if(sym.instanceDataClass!=Daedalus::EInstanceClass::IC_Info)
    return nullptr;
  void* h = sym.instanceDataHandle;
  if(h==nullptr)
    Log::e("invalid C_Info object: \"",sym.name,"\"");
  return reinterpret_cast<Daedalus::GEngineClasses::C_Info*>(h);
  }

void GameScript::removeItem(Item &it) {
  world().removeItem(it);
  }

void GameScript::setInstanceNPC(const char *name, Npc &npc) {
  assert(vm.getDATFile().hasSymbolName(name));
  vm.setInstance(name,npc.handle(),Daedalus::EInstanceClass::IC_Npc);
  }

AiOuputPipe *GameScript::openAiOuput() {
  return aiDefaultPipe.get();
  }

AiOuputPipe *GameScript::openDlgOuput(Npc &player, Npc &npc) {
  if(player.isPlayer())
    return owner.openDlgOuput(player,npc);
  return owner.openDlgOuput(npc,player);
  }

bool GameScript::isRamboMode() const {
  return owner.isRamboMode();
  }

const FightAi::FA &GameScript::getFightAi(size_t i) const {
  return owner.getFightAi(i);
  }

const std::string& GameScript::popString(Daedalus::DaedalusVM &vm) {
  return vm.popString();
  }

Npc *GameScript::popInstance(Daedalus::DaedalusVM &vm) {
  uint32_t arr_self = 0;
  uint32_t idx      = vm.popVar(arr_self);
  return getNpcById(idx);
  }

Item *GameScript::popItem(Daedalus::DaedalusVM &vm) {
  uint32_t arr_self = 0;
  uint32_t idx      = vm.popVar(arr_self);
  return getItemById(idx);
  }



void GameScript::concatstrings(Daedalus::DaedalusVM &vm) {
  const std::string& s2 = popString(vm);
  const std::string& s1 = popString(vm);

  vm.setReturn(s1 + s2);
  }

void GameScript::inttostring(Daedalus::DaedalusVM &vm){
  int32_t x = vm.popInt();
  vm.setReturn(std::to_string(x));
  }

void GameScript::floattostring(Daedalus::DaedalusVM &vm) {
  auto x = vm.popFloat();
  vm.setReturn(std::to_string(x));
  }

void GameScript::floattoint(Daedalus::DaedalusVM &vm) {
  auto x = vm.popFloat();
  vm.setReturn(int32_t(x));
  }

void GameScript::inttofloat(Daedalus::DaedalusVM &vm) {
  auto x = vm.popInt();
  vm.setReturn(float(x));
  }

void GameScript::game_initgerman(Daedalus::DaedalusVM&) {
  }

void GameScript::game_initenglish(Daedalus::DaedalusVM &) {
  }

void GameScript::hlp_random(Daedalus::DaedalusVM &vm) {
  uint32_t mod = uint32_t(std::max(1,vm.popInt()));
  vm.setReturn(int32_t(randGen() % mod));
  }

void GameScript::hlp_strcmp(Daedalus::DaedalusVM &vm) {
  auto& s1 = popString(vm);
  auto& s2 = popString(vm);
  vm.setReturn(s1 == s2 ? 1 : 0);
  }

void GameScript::wld_settime(Daedalus::DaedalusVM &vm) {
  auto minute = vm.popInt();
  auto hour   = vm.popInt();
  world().setDayTime(hour,minute);
  }

void GameScript::wld_getday(Daedalus::DaedalusVM &vm) {
  auto d = owner.time().day();
  vm.setReturn(int32_t(d));
  }

void GameScript::wld_playeffect(Daedalus::DaedalusVM &vm) {
  int32_t            a        = vm.popInt();
  int32_t            b        = vm.popInt();
  int32_t            c        = vm.popInt();
  int32_t            d        = vm.popInt();
  auto               npc1     = popInstance(vm);
  auto               npc0     = popInstance(vm);
  const std::string& visual   = popString(vm);

  Log::i("effect not implemented [",visual,"]");
  }

void GameScript::wld_stopeffect(Daedalus::DaedalusVM &vm) {
  const std::string& visual   = popString(vm);
  Log::i("effect not implemented [",visual,"]");
  }

void GameScript::wld_getplayerportalguild(Daedalus::DaedalusVM &vm) {
  vm.setReturn(int(Guild::GIL_NONE));  // TODO: guild id for a room
  }

void GameScript::wld_setguildattitude(Daedalus::DaedalusVM &vm) {
  size_t  gil2 = size_t(vm.popInt());
  int32_t att  = vm.popInt();
  size_t  gil1 = size_t(vm.popInt());
  if(gilCount==0 || gil1>=gilCount || gil2>=gilCount)
    return;
  gilAttitudes[gil1*gilCount+gil2]=att;
  gilAttitudes[gil2*gilCount+gil1]=att;
  }

void GameScript::wld_getguildattitude(Daedalus::DaedalusVM &vm) {
  int32_t g0 = vm.popInt();
  int32_t g1 = vm.popInt();
  if(g0<0 || g1<0 || gilCount==0) {
    vm.setReturn(ATT_HOSTILE); // error
    return;
    }

  auto selfG = std::min(gilCount-1,size_t(g0));
  auto npcG  = std::min(gilCount-1,size_t(g1));
  auto ret   = gilAttitudes[selfG*gilCount+npcG];
  vm.setReturn(ret);
  }

void GameScript::wld_istime(Daedalus::DaedalusVM &vm) {
  int32_t min1  = vm.popInt();
  int32_t hour1 = vm.popInt();
  int32_t min0  = vm.popInt();
  int32_t hour0 = vm.popInt();

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

void GameScript::wld_isfpavailable(Daedalus::DaedalusVM &vm) {
  auto& name = popString(vm);
  auto  self = popInstance(vm);

  if(self==nullptr){
    vm.setReturn(0);
    return;
    }

  auto wp = world().findFreePoint(*self,name.c_str());
  vm.setReturn(wp ? 1 : 0);
  }

void GameScript::wld_isnextfpavailable(Daedalus::DaedalusVM &vm) {
  auto& name = popString(vm);
  auto  self = popInstance(vm);

  if(self==nullptr){
    vm.setReturn(0);
    return;
    }
  auto fp = world().findNextFreePoint(*self,name.c_str());
  vm.setReturn(fp ? 1 : 0);
  }

void GameScript::wld_ismobavailable(Daedalus::DaedalusVM &vm) {
  auto& name = popString(vm);
  auto  self = popInstance(vm);
  if(self==nullptr){
    vm.setReturn(0);
    return;
    }

  auto wp = world().aviableMob(*self,name);
  vm.setReturn(wp ? 1 : 0);
  }

void GameScript::wld_setmobroutine(Daedalus::DaedalusVM &vm) {
  int   st   = vm.popInt();
  auto& name = popString(vm);
  int   mm   = vm.popInt();
  int   hh   = vm.popInt();
  notImplementedFn<&GameScript::wld_setmobroutine>("wld_setmobroutine");
  //Log::i("TODO: wld_setmobroutine(",hh,",",mm,",",name,",",st,")");
  }

void GameScript::wld_assignroomtoguild(Daedalus::DaedalusVM &vm) {
  int   g    = vm.popInt();
  auto& name = popString(vm);
  notImplementedFn<&GameScript::wld_assignroomtoguild>("wld_assignroomtoguild");
  //Log::i("TODO: wld_assignroomtoguild(",name,",",g,")");
  }

void GameScript::wld_detectnpc(Daedalus::DaedalusVM &vm) {
  int   guild  = vm.popInt();
  int   state  = vm.popInt();
  int   inst   = vm.popInt();
  auto  npc    = popInstance(vm);
  if(npc==nullptr) {
    vm.setReturn(0);
    return;
    }
  Npc*  ret =nullptr;
  float dist=std::numeric_limits<float>::max();

  world().detectNpc(npc->position(), [inst,state,guild,&ret,&dist,npc](Npc& n){
    if((inst ==-1 || int32_t(n.instanceSymbol())==inst) &&
       (state==-1 || n.isState(uint32_t(state))) &&
       (guild==-1 || int32_t(n.guild())==guild) &&
       (&n!=npc) && !n.isDead()) {
      float d = n.qDistTo(*npc);
      if(d<dist){
        ret = &n;
        dist = d;
        }
      }
    });
  if(ret) {
    vm.globalOther().instanceDataHandle = ret->handle();
    vm.globalOther().instanceDataClass  = Daedalus::IC_Npc;
    }
  vm.setReturn(ret ? 1 : 0);
  }

void GameScript::wld_detectitem(Daedalus::DaedalusVM &vm) {
  int   flags = vm.popInt();
  auto  npc   = popInstance(vm);

  (void)flags;
  (void)npc;

  notImplementedFn<&GameScript::wld_detectitem>("wld_detectitem");
  vm.setReturn(0);
  }

void GameScript::mdl_setvisual(Daedalus::DaedalusVM &vm) {
  const std::string& visual = popString(vm);
  auto               npc    = popInstance(vm);
  if(npc==nullptr)
    return;
  npc->setVisual(visual.c_str());
  }

void GameScript::mdl_setvisualbody(Daedalus::DaedalusVM &vm) {
  int32_t     armor        = vm.popInt();
  int32_t     teethTexNr   = vm.popInt();
  int32_t     headTexNr    = vm.popInt();
  auto&       head         = popString(vm);
  int32_t     bodyTexColor = vm.popInt();
  int32_t     bodyTexNr    = vm.popInt();
  auto&       body         = popString(vm);
  auto        npc          = popInstance(vm);

  if(npc==nullptr)
    return;
  npc->setVisualBody(headTexNr,teethTexNr,bodyTexNr,bodyTexColor,body,head);
  if(armor>=0) {
    if(npc->hasItem(uint32_t(armor))==0)
      npc->addItem(uint32_t(armor),1);
    npc->useItem(uint32_t(armor),true);
    }
  }

void GameScript::mdl_setmodelfatness(Daedalus::DaedalusVM &vm) {
  float    fat = vm.popFloat();
  auto     npc = popInstance(vm);

  if(npc!=nullptr)
    npc->setFatness(fat);
  }

void GameScript::mdl_applyoverlaymds(Daedalus::DaedalusVM &vm) {
  auto&       overlayname = popString(vm);
  auto        npc         = popInstance(vm);
  auto        skelet      = Resources::loadSkeleton(overlayname);

  if(npc!=nullptr)
    npc->addOverlay(skelet,0);
  }

void GameScript::mdl_applyoverlaymdstimed(Daedalus::DaedalusVM &vm) {
  int32_t     ticks       = vm.popInt();
  auto&       overlayname = popString(vm);
  auto        npc         = popInstance(vm);
  auto        skelet      = Resources::loadSkeleton(overlayname);

  if(npc!=nullptr && ticks>0)
    npc->addOverlay(skelet,uint64_t(ticks));
  }

void GameScript::mdl_removeoverlaymds(Daedalus::DaedalusVM &vm) {
  auto&       overlayname = popString(vm);
  auto        npc         = popInstance(vm);
  auto        skelet      = Resources::loadSkeleton(overlayname);

  if(npc!=nullptr)
    npc->delOverlay(skelet);
  }

void GameScript::mdl_setmodelscale(Daedalus::DaedalusVM &vm) {
  float z   = vm.popFloat();
  float y   = vm.popFloat();
  float x   = vm.popFloat();
  auto  npc = popInstance(vm);

  if(npc!=nullptr)
    npc->setScale(x,y,z);
  }

void GameScript::mdl_startfaceani(Daedalus::DaedalusVM &vm) {
  float time      = vm.popFloat();
  float intensity = vm.popFloat();
  auto& ani       = popString(vm);
  auto  npc       = popInstance(vm);
  notImplementedFn<&GameScript::mdl_startfaceani>("mdl_startfaceani");
  }

void GameScript::mdl_applyrandomani(Daedalus::DaedalusVM &vm) {
  auto& s0  = popString(vm);
  auto& s1  = popString(vm);
  auto  npc = popInstance(vm);
  notImplementedFn<&GameScript::mdl_applyrandomani>("mdl_applyrandomani");
  }

void GameScript::mdl_applyrandomanifreq(Daedalus::DaedalusVM &vm) {
  auto  f0  = vm.popFloat();
  auto& s1  = popString(vm);
  auto  npc = popInstance(vm);
  notImplementedFn<&GameScript::mdl_applyrandomanifreq>("mdl_applyrandomanifreq");
  }

void GameScript::wld_insertnpc(Daedalus::DaedalusVM &vm) {
  const std::string& spawnpoint = popString(vm);
  int32_t npcInstance = vm.popInt();

  if(spawnpoint.empty() || npcInstance<=0)
    return;

  auto at=world().findPoint(spawnpoint);
  if(at==nullptr){
    Log::e("invalid waypoint \"",spawnpoint,"\"");
    return;
    }
  world().addNpc(size_t(npcInstance),at->name.c_str());
  }

void GameScript::wld_insertitem(Daedalus::DaedalusVM &vm) {
  const std::string& spawnpoint   = popString(vm);
  int32_t            itemInstance = vm.popInt();

  if(spawnpoint.empty() || itemInstance<=0)
    return;

  world().addItem(size_t(itemInstance),spawnpoint.c_str());
  }

void GameScript::npc_settofightmode(Daedalus::DaedalusVM &vm) {
  int32_t weaponSymbol = vm.popInt();
  auto    npc          = popInstance(vm);
  if(npc!=nullptr && weaponSymbol>=0)
    npc->setToFightMode(uint32_t(weaponSymbol));
  }

void GameScript::npc_settofistmode(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->drawWeaponFist();
  }

void GameScript::npc_isinstate(Daedalus::DaedalusVM &vm) {
  uint32_t stateFn = uint32_t(vm.popVar());
  auto     npc     = popInstance(vm);

  if(npc!=nullptr){
    const bool ret=npc->isState(stateFn);
    vm.setReturn(ret ? 1: 0);
    return;
    }
  vm.setReturn(0);
  }

void GameScript::npc_wasinstate(Daedalus::DaedalusVM &vm) {
  uint32_t stateFn = uint32_t(vm.popVar());
  auto     npc     = popInstance(vm);

  if(npc!=nullptr){
    const bool ret=npc->wasInState(stateFn);
    vm.setReturn(ret ? 1: 0);
    return;
    }
  vm.setReturn(0);
  }

void GameScript::npc_getdisttowp(Daedalus::DaedalusVM &vm) {
  auto&    wpname = popString(vm);
  auto     npc    = popInstance(vm);

  auto*    wp     = world().findPoint(wpname);

  if(npc!=nullptr && wp!=nullptr){
    float ret = std::sqrt(npc->qDistTo(wp));
    if(ret<float(std::numeric_limits<int32_t>::max()))
      vm.setReturn(int32_t(ret));else
      vm.setReturn(std::numeric_limits<int32_t>::max());
    } else {
    vm.setReturn(std::numeric_limits<int32_t>::max());
    }
  }

void GameScript::npc_exchangeroutine(Daedalus::DaedalusVM &vm) {
  auto&    rname  = popString(vm);
  auto     npc    = popInstance(vm);
  if(npc!=nullptr) {
    auto& v = *npc->handle();
    char buf[256]={};
    std::snprintf(buf,sizeof(buf),"Rtn_%s_%d",rname.c_str(),v.id);
    auto d = vm.getDATFile().getSymbolIndexByName(buf);
    if(d>0)
      npc->excRoutine(d);
    }
  }

void GameScript::npc_isdead(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc==nullptr || isDead(*npc))
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void GameScript::npc_knowsinfo(Daedalus::DaedalusVM &vm) {
  uint32_t infoinstance = uint32_t(vm.popInt());
  auto     npc          = popInstance(vm);
  if(!npc){
    vm.setReturn(0);
    return;
    }

  Daedalus::GEngineClasses::C_Npc& vnpc = *npc->handle();
  bool knows = doesNpcKnowInfo(vnpc, infoinstance);
  vm.setReturn(knows ? 1 : 0);
  }

void GameScript::npc_settalentskill(Daedalus::DaedalusVM &vm) {
  int  lvl     = vm.popInt();
  int  t       = vm.popInt();
  auto npc     = popInstance(vm);
  if(npc!=nullptr)
    npc->setTalentSkill(Npc::Talent(t),lvl);
  }

void GameScript::npc_gettalentskill(Daedalus::DaedalusVM &vm) {
  uint32_t skillId = uint32_t(vm.popInt());
  uint32_t self    = vm.popVar();
  auto     npc     = getNpcById(self);

  int32_t  skill   = npc==nullptr ? 0 : npc->talentSkill(Npc::Talent(skillId));
  vm.setReturn(skill);
  }

void GameScript::npc_settalentvalue(Daedalus::DaedalusVM &vm) {
  int lvl  = vm.popInt();
  int t    = vm.popInt();
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->setTalentValue(Npc::Talent(t),lvl);
  }

void GameScript::npc_gettalentvalue(Daedalus::DaedalusVM &vm) {
  uint32_t skillId = uint32_t(vm.popInt());
  auto     npc     = popInstance(vm);

  int32_t  skill   = npc==nullptr ? 0 : npc->talentValue(Npc::Talent(skillId));
  vm.setReturn(skill);
  }

void GameScript::npc_setrefusetalk(Daedalus::DaedalusVM &vm) {
  int32_t  timeSec = vm.popInt();
  auto     npc     = popInstance(vm);
  if(npc)
    npc->setRefuseTalk(uint64_t(std::max(timeSec,0)));
  }

void GameScript::npc_refusetalk(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc && npc->isRefuseTalk())
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void GameScript::npc_hasitems(Daedalus::DaedalusVM &vm) {
  uint32_t itemId = uint32_t(vm.popInt());
  auto     npc    = popInstance(vm);
  if(npc!=nullptr)
    vm.setReturn(int32_t(npc->hasItem(itemId))); else
    vm.setReturn(0);
  }

void GameScript::npc_removeinvitems(Daedalus::DaedalusVM &vm) {
  int32_t  amount = vm.popInt();
  uint32_t itemId = uint32_t(vm.popInt());
  auto     npc    = popInstance(vm);

  if(npc!=nullptr && amount>0)
    npc->delItem(itemId,uint32_t(amount));
  }

void GameScript::npc_getbodystate(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);

  if(npc!=nullptr)
    vm.setReturn(int32_t(npc->bodyState())); else
    vm.setReturn(int32_t(0));
  }

void GameScript::npc_getlookattarget(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  auto ret = npc ? nullptr : npc->lookAtTarget();
  if(ret!=nullptr) {
    auto n = *(ret->handle());
    auto x = getNpcById(n.instanceSymbol);
    vm.setReturn(int32_t(n.instanceSymbol)); //TODO
    } else {
    vm.setReturn(0);
    }
  }

void GameScript::npc_getdisttonpc(Daedalus::DaedalusVM &vm) {
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

void GameScript::npc_hasequippedarmor(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr && npc->currentArmour()!=nullptr)
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void GameScript::npc_setperctime(Daedalus::DaedalusVM &vm) {
  float sec = vm.popFloat();
  auto  npc = popInstance(vm);
  if(npc)
    npc->setPerceptionTime(uint64_t(sec*1000));
  }

void GameScript::npc_percenable(Daedalus::DaedalusVM &vm) {
  int32_t fn  = vm.popInt();
  int32_t pr  = vm.popInt();
  auto    npc = popInstance(vm);
  if(npc && fn>=0)
    npc->setPerceptionEnable(Npc::PercType(pr),size_t(fn));
  }

void GameScript::npc_percdisable(Daedalus::DaedalusVM &vm) {
  int32_t pr  = vm.popInt();
  auto    npc = popInstance(vm);
  if(npc)
    npc->setPerceptionDisable(Npc::PercType(pr));
  }

void GameScript::npc_getnearestwp(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  auto wp  = npc ? world().findWayPoint(npc->position()) : nullptr;
  if(wp)
    vm.setReturn(wp->name); else
    vm.setReturn("");
  }

void GameScript::npc_clearaiqueue(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc)
    npc->clearAiQueue();
  }

void GameScript::npc_isplayer(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc && npc->isPlayer())
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void GameScript::npc_getstatetime(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc)
    vm.setReturn(int32_t(npc->stateTime()/1000)); else
    vm.setReturn(0);
  }

void GameScript::npc_setstatetime(Daedalus::DaedalusVM &vm) {
  int32_t val = vm.popInt();
  auto    npc = popInstance(vm);
  if(npc)
    npc->setStateTime(val*1000);
  }

void GameScript::npc_changeattribute(Daedalus::DaedalusVM &vm) {
  int32_t val  = vm.popInt();
  int32_t atr  = vm.popInt();
  auto    npc  = popInstance(vm);
  if(npc!=nullptr && atr>=0)
    npc->changeAttribute(Npc::Attribute(atr),val);
  }

void GameScript::npc_isonfp(Daedalus::DaedalusVM &vm) {
  auto& val = popString(vm);
  auto  npc = popInstance(vm);
  if(npc==nullptr) {
    vm.setReturn(0);
    return;
    }

  auto w = npc->currentWayPoint();
  if(w==nullptr || !MoveAlgo::isClose(npc->position(),*w) || !w->checkName(val)){
    vm.setReturn(0);
    return;
    }
  if(w->isFreePoint()){
    vm.setReturn(1);
    return;
    }
  if(val=="STAND"){
    vm.setReturn(1);
    return;
    }
  vm.setReturn(0);
  }

void GameScript::npc_getheighttonpc(Daedalus::DaedalusVM &vm) {
  auto  a   = popInstance(vm);
  auto  b   = popInstance(vm);
  float ret = 0;
  if(a!=nullptr && b!=nullptr)
    ret = std::abs(a->position()[1] - b->position()[1]);
  vm.setReturn(int32_t(ret));
  }

void GameScript::npc_getequippedmeleweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr){
    auto a = npc->currentRangeWeapon();
    if(a!=nullptr) //FIXME: return value is C_Item
      vm.setReturn(int32_t(a->clsId())); else
      vm.setReturn(0);
    }
  }

void GameScript::npc_getequippedrangedweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr){
    auto a = npc->currentRangeWeapon();
    if(a!=nullptr) //FIXME: return value is C_Item
      vm.setReturn(int32_t(a->clsId())); else
      vm.setReturn(0);
    }
  }

void GameScript::npc_getequippedarmor(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr){
    auto a = npc->currentArmour();
    if(a!=nullptr) //FIXME: return value is C_Item
      vm.setReturn(int32_t(a->clsId())); else
      vm.setReturn(0);
    }
  }

void GameScript::npc_canseenpc(Daedalus::DaedalusVM &vm) {
  auto other = popInstance(vm);
  auto npc   = popInstance(vm);
  bool ret   = false;
  if(npc!=nullptr && other!=nullptr){
    ret = npc->canSeeNpc(*other,false);
    }
  vm.setReturn(ret ? 1 : 0);
  }

void GameScript::npc_hasequippedmeleeweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr && npc->currentMeleWeapon()!=nullptr)
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void GameScript::npc_hasequippedrangedweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr && npc->currentRangeWeapon()!=nullptr)
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void GameScript::npc_getactivespell(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc==nullptr){
    vm.setReturn(-1);
    return;
    }

  const Item* w = npc->inventory().activeWeapon();
  if(w==nullptr || !w->isSpellOrRune()){
    vm.setReturn(-1);
    return;
    }

  vm.setReturn(w->spellId());
  }

void GameScript::npc_getactivespellisscroll(Daedalus::DaedalusVM &vm) {
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

void GameScript::npc_canseenpcfreelos(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  auto oth = popInstance(vm);

  if(npc!=nullptr && oth!=nullptr){
    bool v = npc->canSeeNpc(*oth,true);
    vm.setReturn(v ? 1 : 0);
    return;
    }
  vm.setReturn(0);
  }

void GameScript::npc_isinfightmode(Daedalus::DaedalusVM &vm) {
  FightMode mode = FightMode(vm.popInt());
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

void GameScript::npc_settarget(Daedalus::DaedalusVM &vm) {
  auto oth = popInstance(vm);
  auto npc = popInstance(vm);
  if(npc)
    npc->setTarget(oth);
  }

/**
 * @brief WorldScript::npc_gettarget
 * Fill 'other' with the current npc target.
 * set by Npc_SetTarget () or Npc_GetNextTarget ().
 * - return: current target saved -> TRUE
 * no target saved -> FALSE
 */
void GameScript::npc_gettarget(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  Daedalus::PARSymbol& s = vm.globalOther();

  if(npc!=nullptr && npc->target()) {
    s.instanceDataHandle = npc->target()->handle();
    s.instanceDataClass  = Daedalus::IC_Npc;
    vm.setReturn(1);
    } else {
    s.instanceDataHandle = nullptr;
    s.instanceDataClass  = Daedalus::IC_Npc;
    vm.setReturn(0);
    }
  }

void GameScript::npc_getnexttarget(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  Npc* ret = nullptr;

  if(npc!=nullptr){
    float dist = npc->handle()->senses_range;
    dist*=dist;

    world().detectNpc(npc->position(),[&,npc](Npc& oth){
      if(!oth.isDown() && oth.isEnemy(*npc)){
        float qd = oth.qDistTo(*npc);
        if(qd<dist){
          dist=qd;
          ret = &oth;
          }
        }
      return false;
      });
    if(ret!=nullptr)
      npc->setTarget(ret);
    }

  Daedalus::PARSymbol& s = vm.globalOther();
  if(ret!=nullptr) {
    s.instanceDataHandle = ret->handle();
    s.instanceDataClass  = Daedalus::IC_Npc;
    vm.setReturn(1);
    } else {
    s.instanceDataHandle = nullptr;
    s.instanceDataClass  = Daedalus::IC_Npc;
    vm.setReturn(0);
    }
  }

void GameScript::npc_sendpassiveperc(Daedalus::DaedalusVM &vm) {
  auto victum = popInstance(vm);
  auto other  = popInstance(vm);
  auto id     = vm.popInt();
  auto npc    = popInstance(vm);

  if(npc && other && victum)
    world().sendPassivePerc(*npc,*other,*victum,id);
  }

void GameScript::npc_checkinfo(Daedalus::DaedalusVM &vm) {
  auto imp = vm.popInt();
  auto n   = popInstance(vm);
  if(n==nullptr){
    vm.setReturn(0);
    return;
    }

  auto& hero = vm.globalOther();
  if(hero.instanceDataClass!=Daedalus::EInstanceClass::IC_Npc){
    vm.setReturn(0);
    return;
    }
  auto* hpl  = reinterpret_cast<Daedalus::GEngineClasses::C_Npc*>(hero.instanceDataHandle);
  auto& pl   = *(hpl);
  auto& npc  = *(n->handle());

  for(auto& info:dialogsInfo) {
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

void GameScript::npc_getportalguild(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  vm.setReturn(int(Guild::GIL_NONE));
  notImplementedFn<&GameScript::npc_getportalguild>("npc_getportalguild");
  }

void GameScript::npc_isinplayersroom(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  vm.setReturn(0); //TODO: stub
  notImplementedFn<&GameScript::npc_isinplayersroom>("npc_isinplayersroom");
  }

void GameScript::npc_getreadiedweapon(Daedalus::DaedalusVM &vm) {
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

void GameScript::npc_isdrawingspell(Daedalus::DaedalusVM &vm) {
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

void GameScript::npc_perceiveall(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  (void)npc; // nop
  }

void GameScript::npc_stopani(Daedalus::DaedalusVM &vm) {
  auto& name = popString(vm);
  auto  npc  = popInstance(vm);
  if(npc!=nullptr)
    npc->stopAnim(name);
  }

void GameScript::npc_settrueguild(Daedalus::DaedalusVM &vm) {
  auto  gil = vm.popInt();
  auto  npc = popInstance(vm);
  if(npc!=nullptr)
    npc->setTrueGuild(gil);
  }

void GameScript::npc_gettrueguild(Daedalus::DaedalusVM &vm) {
  auto  npc = popInstance(vm);
  if(npc!=nullptr)
    vm.setReturn(npc->trueGuild()); else
    vm.setReturn(int32_t(GIL_NONE));
  }

void GameScript::npc_clearinventory(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->clearInventory();
  }

void GameScript::npc_getattitude(Daedalus::DaedalusVM &vm) {
  auto a = popInstance(vm);
  auto b = popInstance(vm);

  if(a!=nullptr && b!=nullptr){
    auto att=personAttitude(*a,*b);
    vm.setReturn(att); //TODO: temp attitudes
    } else {
    vm.setReturn(ATT_NEUTRAL);
    }
  }

void GameScript::npc_getpermattitude(Daedalus::DaedalusVM &vm) {
  auto a = popInstance(vm);
  auto b = popInstance(vm);

  if(a!=nullptr && b!=nullptr){
    auto att=personAttitude(*a,*b);
    vm.setReturn(att);
    } else {
    vm.setReturn(ATT_NEUTRAL);
    }
  }

void GameScript::npc_setattitude(Daedalus::DaedalusVM &vm) {
  int32_t att = vm.popInt();
  auto    npc = popInstance(vm);
  if(npc!=nullptr)
    npc->setAttitude(Attitude(att));
  }

void GameScript::npc_settempattitude(Daedalus::DaedalusVM &vm) {
  int32_t att = vm.popInt();
  auto    npc = popInstance(vm);
  if(npc!=nullptr)
    npc->setTempAttitude(Attitude(att));
  }

void GameScript::npc_hasbodyflag(Daedalus::DaedalusVM &vm) {
  int32_t bodyflag = vm.popInt();
  auto    npc      = popInstance(vm);
  if(npc==nullptr){
    vm.setReturn(0);
    return;
    }
  int32_t st = npc->bodyState()&Npc::BS_MAX_FLAGS;
  bodyflag&=Npc::BS_MAX_FLAGS;
  vm.setReturn(bool(bodyflag&st) ? 1 : 0);
  }

void GameScript::npc_getlasthitspellid(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc==nullptr){
    vm.setReturn(0);
    return;
    }
  vm.setReturn(npc->lastHitSpellId());
  }

void GameScript::npc_getlasthitspellcat(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc==nullptr){
    vm.setReturn(SPELL_GOOD);
    return;
    }
  const int id    = npc->lastHitSpellId();
  auto&     spell = getSpell(id);
  vm.setReturn(spell.spellType);
  }

void GameScript::npc_getactivespellcat(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc==nullptr){
    vm.setReturn(SPELL_GOOD);
    return;
    }

  const Item* w = npc->inventory().activeWeapon();
  if(w==nullptr || !w->isSpellOrRune()){
    vm.setReturn(SPELL_GOOD);
    return;
    }

  const int id    = w->spellId();
  auto&     spell = getSpell(id);
  vm.setReturn(spell.spellType);
  }

void GameScript::npc_setactivespellinfo(Daedalus::DaedalusVM &vm) {
  int32_t v   = vm.popInt();
  auto    npc = popInstance(vm);

  notImplementedFn<&GameScript::npc_setactivespellinfo>("npc_setactivespellinfo");
  vm.setReturn(0);
  }

void GameScript::ai_processinfos(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  auto pl  = owner.player();
  if(pl!=nullptr && npc!=nullptr) {
    aiOutOrderId=0;
    npc->aiProcessInfo(*pl);
    }
  }

void GameScript::ai_output(Daedalus::DaedalusVM &vm) {
  auto& outputname = popString(vm);
  auto  target     = popInstance(vm);
  auto  self       = popInstance(vm);

  if(!self || !target)
    return;

  self->aiOutput(*target,outputname,aiOutOrderId);
  ++aiOutOrderId;
  }

void GameScript::ai_stopprocessinfos(Daedalus::DaedalusVM &vm) {
  auto self = popInstance(vm);
  if(self)
    self->aiStopProcessInfo();
  }

void GameScript::ai_standup(Daedalus::DaedalusVM &vm) {
  auto self = popInstance(vm);
  if(self!=nullptr)
    self->aiStandup();
  }

void GameScript::ai_standupquick(Daedalus::DaedalusVM &vm) {
  auto self = popInstance(vm);
  if(self!=nullptr)
    self->aiStandupQuick();
  }

void GameScript::ai_continueroutine(Daedalus::DaedalusVM &vm) {
  auto self = popInstance(vm);
  if(self!=nullptr)
    self->aiContinueRoutine();
  }

void GameScript::ai_stoplookat(Daedalus::DaedalusVM &vm) {
  auto self = popInstance(vm);
  if(self!=nullptr)
    self->aiStopLookAt();
  }

void GameScript::ai_lookatnpc(Daedalus::DaedalusVM &vm) {
  auto npc  = popInstance(vm);
  auto self = popInstance(vm);
  if(self!=nullptr)
    self->aiLookAt(npc);
  }

void GameScript::ai_removeweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiRemoveWeapon();
  }

void GameScript::ai_turntonpc(Daedalus::DaedalusVM &vm) {
  auto npc  = popInstance(vm);
  auto self = popInstance(vm);
  if(self!=nullptr)
    self->aiTurnToNpc(npc);
  }

void GameScript::ai_outputsvm(Daedalus::DaedalusVM &vm) {
  auto& name   = popString  (vm);
  auto  target = popInstance(vm);
  auto  self   = popInstance(vm);
  if(self!=nullptr && target!=nullptr) {
    self->aiOutputSvm(*target,name,aiOutOrderId);
    ++aiOutOrderId;
    }
  }

void GameScript::ai_outputsvm_overlay(Daedalus::DaedalusVM &vm) {
  auto& name   = popString  (vm);
  auto  target = popInstance(vm);
  auto  self   = popInstance(vm);
  if(self!=nullptr && target!=nullptr) {
    self->aiOutputSvmOverlay(*target,name,aiOutOrderId);
    ++aiOutOrderId;
    }
  }

void GameScript::ai_startstate(Daedalus::DaedalusVM &vm) {
  auto& wp    = popString(vm);
  auto  state = vm.popInt();
  auto  func  = vm.popInt();
  auto  self  = popInstance(vm);
  Daedalus::PARSymbol& s = vm.globalOther();
  if(self!=nullptr && func>0){
    Npc* oth = nullptr;
    if(s.instanceDataClass==Daedalus::IC_Npc){
      auto npc = reinterpret_cast<Daedalus::GEngineClasses::C_Npc*>(s.instanceDataHandle);
      if(npc)
        oth = reinterpret_cast<Npc*>(npc->userPtr);
      }
    self->aiStartState(uint32_t(func),state,oth,wp);
    }
  }

void GameScript::ai_playani(Daedalus::DaedalusVM &vm) {
  auto name = popString  (vm);
  auto npc  = popInstance(vm);
  if(npc!=nullptr)
    npc->aiPlayAnim(name);
  }

void GameScript::ai_setwalkmode(Daedalus::DaedalusVM &vm) {
  int32_t weaponBit = 0x80;
  int32_t modeBits  = vm.popInt();
  auto    npc       = popInstance(vm);

  int32_t mode = modeBits & (~weaponBit);
  if(npc!=nullptr && mode>=0 && mode<=3){ //TODO: weapon flags
    npc->aiSetWalkMode(WalkBit(mode));
    }
  }

void GameScript::ai_wait(Daedalus::DaedalusVM &vm) {
  auto ms  = vm.popFloat();
  auto npc = popInstance(vm);
  if(npc!=nullptr && ms>0)
    npc->aiWait(uint64_t(ms*1000));
  }

void GameScript::ai_waitms(Daedalus::DaedalusVM &vm) {
  auto ms  = vm.popInt();
  auto npc = popInstance(vm);
  if(npc!=nullptr && ms>0)
    npc->aiWait(uint64_t(ms));
  }

void GameScript::ai_aligntowp(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc)
    npc->aiAlignToWp();
  }

void GameScript::ai_gotowp(Daedalus::DaedalusVM &vm) {
  auto&  waypoint = popString(vm);
  auto   npc      = popInstance(vm);

  auto to = world().findPoint(waypoint);
  if(npc && to)
    npc->aiGoToPoint(*to);
  }

void GameScript::ai_gotofp(Daedalus::DaedalusVM &vm) {
  auto&  waypoint = popString(vm);
  auto   npc      = popInstance(vm);

  if(npc) {
    auto to = world().findFreePoint(*npc,waypoint.c_str());
    if(to!=nullptr)
      npc->aiGoToPoint(*to);
    }
  }

void GameScript::ai_playanibs(Daedalus::DaedalusVM &vm) {
  Npc::BodyState bs  = Npc::BodyState(vm.popVar());
  auto&          ani = popString(vm);
  auto           npc = popInstance(vm);
  if(npc!=nullptr){
    (void)bs;
    npc->aiPlayAnim(ani);
    }
  }

void GameScript::ai_equiparmor(Daedalus::DaedalusVM &vm) {
  auto id  = vm.popInt();
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiEquipArmor(id);
  }

void GameScript::ai_equipbestarmor(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiEquipBestArmor();
  }

void GameScript::ai_equipbestmeleeweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiEquipBestMeleWeapon();
  }

void GameScript::ai_equipbestrangedweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiEquipBestRangeWeapon();
  }

void GameScript::ai_usemob(Daedalus::DaedalusVM &vm) {
  int32_t  state = vm.popInt();
  auto&    tg    = popString(vm);
  auto     npc   = popInstance(vm);
  if(npc!=nullptr)
    npc->aiUseMob(tg,state);
  }

void GameScript::ai_teleport(Daedalus::DaedalusVM &vm) {
  auto&    tg  = popString(vm);
  auto     npc = popInstance(vm);
  auto     pt  = world().findPoint(tg);
  if(npc!=nullptr && pt!=nullptr)
    npc->aiTeleport(*pt);
  }

void GameScript::ai_stoppointat(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  (void)npc;
  // TODO: stub
  }

void GameScript::ai_readymeleeweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiReadyMeleWeapon();
  }

void GameScript::ai_readyrangedweapon(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiReadyRangeWeapon();
  }

void GameScript::ai_readyspell(Daedalus::DaedalusVM &vm) {
  auto mana  = vm.popInt();
  auto spell = vm.popInt();
  auto npc   = popInstance(vm);
  if(npc!=nullptr)
    npc->aiReadySpell(spell,mana);
  }

void GameScript::ai_atack(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiAtack();
  }

void GameScript::ai_flee(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiFlee();
  }

void GameScript::ai_dodge(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiDodge();
  }

void GameScript::ai_unequipweapons(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiUnEquipWeapons();
  }

void GameScript::ai_unequiparmor(Daedalus::DaedalusVM &vm) {
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiUnEquipArmor();
  }

void GameScript::ai_gotonpc(Daedalus::DaedalusVM &vm) {
  auto to  = popInstance(vm);
  auto npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiGoToNpc(to);
  }

void GameScript::ai_gotonextfp(Daedalus::DaedalusVM &vm) {
  auto& to  = popString(vm);
  auto  npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiGoToNextFp(to);
  }

void GameScript::ai_aligntofp(Daedalus::DaedalusVM &vm) {
  auto  npc = popInstance(vm);
  if(npc!=nullptr)
    npc->aiAlignToFp();
  }

void GameScript::ai_useitem(Daedalus::DaedalusVM &vm) {
  int32_t  item  = vm.popInt();
  auto     npc   = popInstance(vm);
  if(npc)
    npc->aiUseItem(item);
  }

void GameScript::ai_useitemtostate(Daedalus::DaedalusVM &vm) {
  int32_t  state = vm.popInt();
  int32_t  item  = vm.popInt();
  auto     npc   = popInstance(vm);
  notImplementedFn<&GameScript::ai_useitemtostate>("ai_useitemtostate");
  }

void GameScript::ai_setnpcstostate(Daedalus::DaedalusVM &vm) {
  int32_t  radius = vm.popInt();
  int32_t  state  = vm.popInt();
  auto     npc    = popInstance(vm);
  if(npc && state>0)
    npc->aiSetNpcsToState(size_t(state),radius);
  }

void GameScript::mob_hasitems(Daedalus::DaedalusVM &vm) {
  uint32_t item = vm.popVar();
  auto&    tag  = popString(vm);
  vm.setReturn(int(world().hasItems(tag,item)));
  }

void GameScript::ta_min(Daedalus::DaedalusVM &vm) {
  auto&    waypoint = popString(vm);
  int32_t  action   = vm.popInt();
  int32_t  stop_m   = vm.popInt();
  int32_t  stop_h   = vm.popInt();
  int32_t  start_m  = vm.popInt();
  int32_t  start_h  = vm.popInt();
  auto     npc      = popInstance(vm);
  auto     at       = world().findPoint(waypoint);

  if(npc!=nullptr)
    npc->addRoutine(gtime(start_h,start_m),gtime(stop_h,stop_m),uint32_t(action),at);
  }

void GameScript::log_createtopic(Daedalus::DaedalusVM &vm) {
  int32_t section   = vm.popInt();
  auto&   topicName = popString(vm);

  if(section==QuestLog::Mission || section==QuestLog::Note)
    quests.add(topicName,QuestLog::Section(section));
  }

void GameScript::log_settopicstatus(Daedalus::DaedalusVM &vm) {
  int32_t status    = vm.popInt();
  auto&   topicName = popString(vm);

  if(status==int32_t(QuestLog::Status::Running) ||
     status==int32_t(QuestLog::Status::Success) ||
     status==int32_t(QuestLog::Status::Failed ) ||
     status==int32_t(QuestLog::Status::Obsolete))
    quests.setStatus(topicName,QuestLog::Status(status));
  }

void GameScript::log_addentry(Daedalus::DaedalusVM &vm) {
  auto&   entry     = popString(vm);
  auto&   topicName = popString(vm);

  quests.addEntry(topicName,entry);
  }

void GameScript::equipitem(Daedalus::DaedalusVM &vm) {
  uint32_t cls  = uint32_t(vm.popInt());
  auto     self = popInstance(vm);

  if(self!=nullptr) {
    if(self->hasItem(cls)==0)
      self->addItem(cls,1);
    self->useItem(cls,true);
    }
  }

void GameScript::createinvitem(Daedalus::DaedalusVM &vm) {
  uint32_t itemInstance = uint32_t(vm.popInt());
  auto     self         = popInstance(vm);
  if(self!=nullptr) {
    Item* itm = self->addItem(itemInstance,1);
    storeItem(itm);
    }
  }

void GameScript::createinvitems(Daedalus::DaedalusVM &vm) {
  int32_t  amount       = int32_t(vm.popInt());
  uint32_t itemInstance = uint32_t(vm.popInt());
  auto     self         = popInstance(vm);
  if(self!=nullptr && amount>0) {
    Item* itm = self->addItem(itemInstance,uint32_t(amount));
    storeItem(itm);
    }
  }

void GameScript::hlp_getinstanceid(Daedalus::DaedalusVM &vm) {
  uint32_t arr_self = 0;
  uint32_t idx      = vm.popVar(arr_self);

  auto& handle = vm.getDATFile().getSymbolByIndex(idx);(void)handle;
  auto self = getNpcById(idx);
  if(self!=nullptr){
    auto v = *(self->handle());
    vm.setReturn(int32_t(v.instanceSymbol));
    return;
    }

  auto item = getItemById(idx);
  if(item!=nullptr){
    auto v = *(item->handle());
    vm.setReturn(int32_t(v.instanceSymbol));
    return;
    }

  // Log::d("hlp_getinstanceid: name \"",handle.name,"\" not found");
  vm.setReturn(-1);
  }

void GameScript::hlp_isvalidnpc(Daedalus::DaedalusVM &vm) {
  auto self = popInstance(vm);
  if(self!=nullptr)
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void GameScript::hlp_isitem(Daedalus::DaedalusVM &vm) {
  uint32_t instanceSymbol = vm.popVar();
  auto     item           = popItem(vm);
  if(item!=nullptr){
    auto& v = *(item->handle());
    vm.setReturn(v.instanceSymbol==instanceSymbol ? 1 : 0);
    } else
    vm.setReturn(0);
  }

void GameScript::hlp_isvaliditem(Daedalus::DaedalusVM &vm) {
  auto item = popItem(vm);
  if(item!=nullptr)
    vm.setReturn(1); else
    vm.setReturn(0);
  }

void GameScript::hlp_getnpc(Daedalus::DaedalusVM &vm) {
  uint32_t instanceSymbol = vm.popVar();
  auto&    handle         = vm.getDATFile().getSymbolByIndex(instanceSymbol);(void)handle;

  if(auto npc = getNpcById(instanceSymbol))
    vm.setReturn(int32_t(instanceSymbol)); else
    vm.setReturn(-1);
  }

void GameScript::info_addchoice(Daedalus::DaedalusVM &vm) {
  uint32_t func         = vm.popVar();
  auto&    text         = popString(vm);
  uint32_t infoInstance = uint32_t(vm.popInt());

  auto info = getInfo(infoInstance);
  if(info==nullptr)
    return;
  info->addChoice(Daedalus::GEngineClasses::SubChoice{text, func});
  }

void GameScript::info_clearchoices(Daedalus::DaedalusVM &vm) {
  uint32_t infoInstance = uint32_t(vm.popInt());

  auto info = getInfo(infoInstance);
  if(info==nullptr)
    return;
  info->subChoices.clear();
  }

void GameScript::infomanager_hasfinished(Daedalus::DaedalusVM &vm) {
  vm.setReturn(owner.aiIsDlgFinished() ? 1 : 0);
  }

void GameScript::snd_play(Daedalus::DaedalusVM &vm) {
  auto file = popString(vm);
  for(auto& c:file)
    c = char(std::toupper(c));
  owner.emitGlobalSound(file);
  }

void GameScript::introducechapter(Daedalus::DaedalusVM &vm) {
  ChapterScreen::Show s;
  s.time     = vm.popInt();
  s.sound    = popString(vm);
  s.img      = popString(vm);
  s.subtitle = popString(vm);
  s.title    = popString(vm);
  owner.introChapter(s);
  }

void GameScript::playvideo(Daedalus::DaedalusVM &vm) {
  const std::string& filename = popString(vm);
  Log::i("video not implemented [",filename,"]");
  vm.setReturn(0);
  }

void GameScript::printscreen(Daedalus::DaedalusVM &vm) {
  int32_t            timesec = vm.popInt();
  const std::string& font    = popString(vm);
  int32_t            posy    = vm.popInt();
  int32_t            posx    = vm.popInt();
  const std::string& msg     = popString(vm);
  owner.printScreen(cp1251::toUtf8(msg).c_str(),posx,posy,timesec,Resources::fontByName(font));
  vm.setReturn(0);
  }

void GameScript::printdialog(Daedalus::DaedalusVM &vm) {
  int32_t            timesec  = vm.popInt();
  const std::string& font     = popString(vm);
  int32_t            posy     = vm.popInt();
  int32_t            posx     = vm.popInt();
  const std::string& msg      = popString(vm);
  int32_t            dialognr = vm.popInt();
  (void)dialognr;
  owner.printScreen(cp1251::toUtf8(msg).c_str(),posx,posy,timesec,Resources::fontByName(font));
  vm.setReturn(0);
  }

void GameScript::print(Daedalus::DaedalusVM &vm) {
  const std::string& msg = popString(vm);
  owner.print(cp1251::toUtf8(msg).c_str());
  }

void GameScript::perc_setrange(Daedalus::DaedalusVM &) {
  Log::i("perc_setrange");
  }

void GameScript::printdebug(Daedalus::DaedalusVM &vm) {
  const std::string& msg = popString(vm);
  Log::d("[zspy]: ",msg);
  }

void GameScript::printdebugch(Daedalus::DaedalusVM &vm) {
  const std::string& msg = popString(vm);
  int                ch  = vm.popInt();
  //Log::d("[zspy,",ch,"]: ",msg);
  }

void GameScript::printdebuginst(Daedalus::DaedalusVM &vm) {
  const std::string& msg = popString(vm);
  //Log::d("[zspy]: ",msg);
  }

void GameScript::printdebuginstch(Daedalus::DaedalusVM &vm) {
  const std::string& msg = popString(vm);
  int                ch  = vm.popInt();
  //Log::d("[zspy,",ch,"]: ",msg);
  }

void GameScript::sort(std::vector<GameScript::DlgChoise> &dlg) {
  std::sort(dlg.begin(),dlg.end(),[](const GameScript::DlgChoise& l,const GameScript::DlgChoise& r){
    return std::tie(l.sort,l.scriptFn)<std::tie(r.sort,r.scriptFn); // small hack with scriptfn to reproduce behavior of original game
    });
  }

void GameScript::setNpcInfoKnown(const Daedalus::GEngineClasses::C_Npc& npc, const Daedalus::GEngineClasses::C_Info &info) {
  auto id = std::make_pair(npc.instanceSymbol,info.instanceSymbol);
  dlgKnownInfos.insert(id);
  }

bool GameScript::doesNpcKnowInfo(const Daedalus::GEngineClasses::C_Npc& npc, size_t infoInstance) const {
  auto id = std::make_pair(npc.instanceSymbol,infoInstance);
  return dlgKnownInfos.find(id)!=dlgKnownInfos.end();
  }


