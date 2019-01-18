#pragma once

#include <cstdint>
#include <string>

class Npc final {
  public:
    enum Talent : uint8_t {
      NPC_TALENT_UNKNOWN            = 0,
      NPC_TALENT_1H                 = 1,
      NPC_TALENT_2H                 = 2,
      NPC_TALENT_BOW                = 3,
      NPC_TALENT_CROSSBOW           = 4,
      NPC_TALENT_PICKLOCK           = 5,
      NPC_TALENT_MAGE               = 7,
      NPC_TALENT_SNEAK              = 8,
      NPC_TALENT_REGENERATE         = 9,
      NPC_TALENT_FIREMASTER         = 10,
      NPC_TALENT_ACROBAT            = 11,
      NPC_TALENT_PICKPOCKET         = 12,
      NPC_TALENT_SMITH              = 13,
      NPC_TALENT_RUNES              = 14,
      NPC_TALENT_ALCHEMY            = 15,
      NPC_TALENT_TAKEANIMALTROPHY   = 16,
      NPC_TALENT_FOREIGNLANGUAGE    = 17,
      NPC_TALENT_WISPDETECTOR       = 18,
      NPC_TALENT_C                  = 19,
      NPC_TALENT_D                  = 20,
      NPC_TALENT_E                  = 21,

      NPC_TALENT_MAX                = 22
      };

    Npc();
    Npc(const Npc&)=delete;
    ~Npc();

    void setName(const std::string& name);
    void setTalentSkill(Talent t,int32_t lvl);

    void equipItem();

  private:
    std::string name;
    int32_t     talents[NPC_TALENT_MAX]={};
  };
