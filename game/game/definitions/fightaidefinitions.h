#pragma once

#include <phoenix/vm.hh>
#include <phoenix/ext/daedalus_classes.hh>

class FightAi final {
  public:
    struct FA final {
      phoenix::c_fight_ai enemy_prehit;      // Enemy attacks me
      phoenix::c_fight_ai enemy_stormprehit; // Enemy makes a storm attack
      phoenix::c_fight_ai my_w_combo;        // I'm in the combo window
      phoenix::c_fight_ai my_w_runto;        // I run towards the opponent
      phoenix::c_fight_ai my_w_strafe;       // Just take hit
      phoenix::c_fight_ai my_w_focus;        // I have opponent in focus (can hit)
      phoenix::c_fight_ai my_w_nofocus;      // I don't have opponent in focus

      // 'G' - range
      phoenix::c_fight_ai my_g_combo;        // I'm in the combo window (not used in G2)
      phoenix::c_fight_ai my_g_runto;        // I run towards the opponent (can make a storm attack)
      phoenix::c_fight_ai my_g_strafe;       // not used in G2
      phoenix::c_fight_ai my_g_focus;        // I have opponent in focus (can hit)

      // FK Range Foes (far away)
      phoenix::c_fight_ai my_fk_focus;       // I have opponents in focus

      phoenix::c_fight_ai my_g_fk_nofocus;   // I am NOT in focus of opponents (also applies to G-distance!)

      // Range + Magic  (used at each removal)
      phoenix::c_fight_ai my_fk_focus_far;   // Opponents in focus
      phoenix::c_fight_ai my_fk_nofocus_far; // Opponents NOT in focus

      phoenix::c_fight_ai my_fk_focus_mag;   // Opponents in focus
      phoenix::c_fight_ai my_fk_nofocus_mag; // Opponents NOT in focus
      };

    FightAi();

    const FA& operator[](size_t i) const;

  private:
    auto loadAi(phoenix::vm &vm, const char* name) -> phoenix::c_fight_ai;
    FA   loadAi(phoenix::vm &vm, size_t id);

    std::vector<FA> fAi;
  };
