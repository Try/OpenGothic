#include "fightaidefinitions.h"

#include "gothic.h"

FightAi::FightAi() {
  auto vm = Gothic::inst().createPhoenixVm("Fight.dat");

  auto* max = vm->find_symbol_by_name("MAX_FIGHTAI");
  int count = max != nullptr ? max->get_int() : 0;
  if(count<0)
    count=0;

  fAi.resize(size_t(count));
  for(size_t i=1;i<fAi.size();++i)
    fAi[i] = loadAi(*vm,i);
  }

const FightAi::FA& FightAi::operator[](size_t i) const {
  if(i<fAi.size())
    return fAi[i];
  static FA tmp;
  return tmp;
  }

phoenix::c_fight_ai FightAi::loadAi(phoenix::vm& vm, const char* name) {
  auto id = vm.find_symbol_by_name(name);
  if(id==nullptr)
    return {};

  try {
    auto fai = vm.init_instance<phoenix::c_fight_ai>(id);
    return *fai;
    } catch (const phoenix::script_error&) {
    // There was an error during initialization. Ignore it.
    }

  return {};
  }

FightAi::FA FightAi::loadAi(phoenix::vm &vm, size_t id) {
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
