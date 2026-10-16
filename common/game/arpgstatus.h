#pragma once

#include <cstdint>

// Diathic: negative statuses of the hero, people and creatures (Npc::arpgApplyStatus).
// The first ones: Zachwianie (after a parry), Podpalenie (Gniew Innosa), Krwawienie (Głębokie rany); more will come.
namespace ArpgStatus {
  enum Id : uint8_t {
    Stagger,  // Zachwianie: the swing stops, no attack, the next hit taken x4 and ends it, blue stars over the head
    Burn,     // Podpalenie: fire every second (Gniew Innosa)
    Bleed,    // Krwawienie: damage every second (Głębokie rany)
    Count
    };

  struct Info {
    const char* name;  // UTF-8 (ArpgMenu::gtxt for Gothic's fonts)
    bool        dot;   // damage every second (value = damage per second)
    };

  inline const Info& info(Id id) {
    static const Info infos[Count] = {
      {"Zachwianie",  false},
      {"Podpalenie",  true },
      {"Krwawienie",  true },
      };
    return infos[id<Count ? id : 0];
    }

  constexpr uint64_t StaggerMs     = 1000;  // how long a parried enemy is staggered (ends at the first hit taken)
  constexpr float    StaggerDmgMul = 4.f;   // damage of that hit
  constexpr uint64_t ParryWindowMs = 250;   // the block has to start at most this long before the enemy's hit
  }
