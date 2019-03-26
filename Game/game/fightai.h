#pragma once

#include <daedalus/DaedalusVM.h>

class Gothic;

class FightAi final {
  public:
    struct FA final {
      Daedalus::GEngineClasses::C_FightAI enemy_prehit;      // Enemy attacks me
      Daedalus::GEngineClasses::C_FightAI enemy_stormprehit; // Enemy makes a storm attack
      Daedalus::GEngineClasses::C_FightAI my_w_combo;        // I'm in the combo window
      Daedalus::GEngineClasses::C_FightAI my_w_runto;        // I run towards the opponent
      Daedalus::GEngineClasses::C_FightAI my_w_strafe;       // Just take hit
      Daedalus::GEngineClasses::C_FightAI my_w_focus;        // I have opponent in focus (can hit)
      Daedalus::GEngineClasses::C_FightAI my_w_nofocus;      // I don't have opponent in focus

      // 'G' - range
      Daedalus::GEngineClasses::C_FightAI my_g_combo;        // I'm in the combo window (not used in G2)
      Daedalus::GEngineClasses::C_FightAI my_g_runto;        // I run towards the opponent (can make a storm attack)
      Daedalus::GEngineClasses::C_FightAI my_g_strafe;       // not used in G2
      Daedalus::GEngineClasses::C_FightAI my_g_focus;        // I have opponent in focus (can hit)

      // FK Range Foes (far away)
      Daedalus::GEngineClasses::C_FightAI my_fk_focus;       // I have opponents in focus

      Daedalus::GEngineClasses::C_FightAI my_g_fk_nofocus;   // I am NOT in focus of opponents (also applies to G-distance!)

      // Range + Magic  (used at each removal)
      Daedalus::GEngineClasses::C_FightAI my_fk_focus_far;   // Opponents in focus
      Daedalus::GEngineClasses::C_FightAI my_fk_nofocus_far; // Opponents NOT in focus

      Daedalus::GEngineClasses::C_FightAI my_fk_focus_mag;   // Opponents in focus
      Daedalus::GEngineClasses::C_FightAI my_fk_nofocus_mag; // Opponents NOT in focus
      };

    FightAi(Gothic &gothic);

    const FA& get(size_t i);

  private:
    auto loadAi(Daedalus::DaedalusVM &vm, const char* name) -> Daedalus::GEngineClasses::C_FightAI;
    FA   loadAi(Daedalus::DaedalusVM &vm, size_t id);

    std::vector<FA> fAi;
  };
