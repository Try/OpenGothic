#include "fightaidefinitions.h"

#include "gothic.h"
#include "utils/string_frm.h"

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

zenkit::IFightAi FightAi::loadAi(zenkit::DaedalusVm& vm, std::string_view name) {
  auto id = vm.find_symbol_by_name(name);
  if(id==nullptr)
    return {};
  auto* move = vm.find_symbol_by_name("C_FIGHTAI.MOVE");
  if(move==nullptr)
    return {};

  try {
    auto fai = vm.init_opaque_instance(id);
    zenkit::IFightAi ret = {};
    // Note: any excessive move will be discarded
    for(size_t i=0; i<zenkit::IFightAi::move_count && i<move->count(); ++i) {
      ret.move[i] = zenkit::FightAiMove(move->get_int(uint16_t(i), fai.get()));
      }
    return ret;
    }
  catch(const zenkit::DaedalusScriptError&) {
    // There was an error during initialization. Ignore it.
    }

  return {};
  }

FightAi::FA FightAi::loadAi(zenkit::DaedalusVm &vm, size_t id) {
  FA ret={};

  string_frm name("FA_ENEMY_PREHIT_",int(id));
  ret.enemy_prehit = loadAi(vm,name);

  name = string_frm("FA_ENEMY_STORMPREHIT_",int(id));
  ret.enemy_stormprehit = loadAi(vm,name);

  name = string_frm("FA_MY_W_COMBO_",int(id));
  ret.my_w_combo = loadAi(vm,name);

  name = string_frm("FA_MY_W_RUNTO_",int(id));
  ret.my_w_runto = loadAi(vm,name);

  name = string_frm("FA_MY_W_STRAFE_",int(id));
  ret.my_w_strafe = loadAi(vm,name);

  name = string_frm("FA_MY_W_FOCUS_",int(id));
  ret.my_w_focus = loadAi(vm,name);

  name = string_frm("FA_MY_W_NOFOCUS_",int(id));
  ret.my_w_nofocus = loadAi(vm,name);

  name = string_frm("FA_MY_G_COMBO_",int(id));
  ret.my_g_combo = loadAi(vm,name);

  name = string_frm("FA_MY_G_RUNTO_",int(id));
  ret.my_g_runto = loadAi(vm,name);

  name = string_frm("FA_MY_G_STRAFE_",int(id));
  ret.my_g_strafe = loadAi(vm,name);

  name = string_frm("FA_MY_G_FOCUS_",int(id));
  ret.my_g_focus = loadAi(vm,name);

  name = string_frm("FA_MY_FK_FOCUS_",int(id));
  ret.my_fk_focus = loadAi(vm,name);

  name = string_frm("FA_MY_G_FK_NOFOCUS_",int(id));
  ret.my_g_fk_nofocus = loadAi(vm,name);

  name = string_frm("FA_MY_FK_FOCUS_FAR_",int(id));
  ret.my_fk_focus_far = loadAi(vm,name);

  name = string_frm("FA_MY_FK_NOFOCUS_FAR_",int(id));
  ret.my_fk_nofocus_far = loadAi(vm,name);

  name = string_frm("FA_MY_FK_FOCUS_MAG_",int(id));
  ret.my_fk_focus_mag = loadAi(vm,name);

  name = string_frm("FA_MY_FK_NOFOCUS_MAG_",int(id));
  ret.my_fk_nofocus_mag = loadAi(vm,name);

  return ret;
  }
