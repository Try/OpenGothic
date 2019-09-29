#include "fightaidefinitions.h"

#include "gothic.h"

FightAi::FightAi(Gothic& gothic) {
  auto vm = gothic.createVm(u"_work/Data/Scripts/_compiled/Fight.dat");

  auto& max = vm->getDATFile().getSymbolByName("MAX_FIGHTAI");
  int count = max.getInt();
  if(count<0)
    count=0;

  fAi.resize(size_t(count));
  for(size_t i=1;i<fAi.size();++i)
    fAi[i] = loadAi(*vm,i);

  vm->clearReferences(Daedalus::IC_FightAi);
  }

const FightAi::FA &FightAi::get(size_t i) {
  if(i<fAi.size())
    return fAi[i];
  static FA tmp;
  return tmp;
  }

Daedalus::GEngineClasses::C_FightAI FightAi::loadAi(Daedalus::DaedalusVM& vm,const char *name) {
  Daedalus::GEngineClasses::C_FightAI ret={};
  auto id = vm.getDATFile().getSymbolIndexByName(name);
  if(id==size_t(-1))
    return ret;
  vm.initializeInstance(ret, id, Daedalus::IC_FightAi);
  return ret;
  }

FightAi::FA FightAi::loadAi(Daedalus::DaedalusVM &vm, size_t id) {
  FA ret={};
  char buf[64]={};

  std::snprintf(buf,sizeof(buf),"FA_ENEMY_PREHIT_%d",int(id));
  ret.enemy_prehit      = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_ENEMY_STORMPREHIT_%d",int(id));
  ret.enemy_stormprehit = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_W_COMBO_%d",int(id));
  ret.my_w_combo = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_W_RUNTO_%d",int(id));
  ret.my_w_runto = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_W_STRAFE_%d",int(id));
  ret.my_w_strafe = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_W_FOCUS_%d",int(id));
  ret.my_w_focus = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_W_NOFOCUS_%d",int(id));
  ret.my_w_nofocus = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_G_COMBO_%d",int(id));
  ret.my_g_combo = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_G_RUNTO_%d",int(id));
  ret.my_g_runto = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_G_STRAFE_%d",int(id));
  ret.my_g_strafe = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_G_FOCUS_%d",int(id));
  ret.my_g_focus = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_FK_FOCUS_%d",int(id));
  ret.my_fk_focus = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_G_FK_NOFOCUS_%d",int(id));
  ret.my_g_fk_nofocus = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_FK_FOCUS_FAR_%d",int(id));
  ret.my_fk_focus_far = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_FK_NOFOCUS_FAR_%d",int(id));
  ret.my_fk_nofocus_far = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_FK_FOCUS_MAG_%d",int(id));
  ret.my_fk_focus_mag = loadAi(vm,buf);

  std::snprintf(buf,sizeof(buf),"FA_MY_FK_NOFOCUS_MAG_%d",int(id));
  ret.my_fk_nofocus_mag = loadAi(vm,buf);

  return ret;
  }
