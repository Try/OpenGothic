#pragma once

#include <phoenix/vm.hh>
#include <phoenix/ext/daedalus_classes.hh>

class FightAi final {
  public:
    struct FA final {
      zenkit::IFightAi enemy_prehit;      // Enemy attacks me
      zenkit::IFightAi enemy_stormprehit; // Enemy makes a storm attack
      zenkit::IFightAi my_w_combo;        // I'm in the combo window
      zenkit::IFightAi my_w_runto;        // I run towards the opponent
      zenkit::IFightAi my_w_strafe;       // Just take hit
      zenkit::IFightAi my_w_focus;        // I have opponent in focus (can hit)
      zenkit::IFightAi my_w_nofocus;      // I don't have opponent in focus

      // 'G' - range
      zenkit::IFightAi my_g_combo;        // I'm in the combo window (not used in G2)
      zenkit::IFightAi my_g_runto;        // I run towards the opponent (can make a storm attack)
      zenkit::IFightAi my_g_strafe;       // not used in G2
      zenkit::IFightAi my_g_focus;        // I have opponent in focus (can hit)

      // FK Range Foes (far away)
      zenkit::IFightAi my_fk_focus;       // I have opponents in focus

      zenkit::IFightAi my_g_fk_nofocus;   // I am NOT in focus of opponents (also applies to G-distance!)

      // Range + Magic  (used at each removal)
      zenkit::IFightAi my_fk_focus_far;   // Opponents in focus
      zenkit::IFightAi my_fk_nofocus_far; // Opponents NOT in focus

      zenkit::IFightAi my_fk_focus_mag;   // Opponents in focus
      zenkit::IFightAi my_fk_nofocus_mag; // Opponents NOT in focus
      };

    FightAi();

    const FA& operator[](size_t i) const;

  private:
    auto loadAi(zenkit::DaedalusVm &vm, std::string_view name) -> zenkit::IFightAi;
    FA   loadAi(zenkit::DaedalusVm &vm, size_t id);

    std::vector<FA> fAi;
  };
