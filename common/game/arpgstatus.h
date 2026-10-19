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

  // Diathic: the hero's block holds 3 hits; lowered, it gets 1 back a second; at 0 every hit goes 1 below
  // (to -3) and each charge below 0 lets 25% of the damage through. A parry does not use the block (and the other way)
  constexpr int32_t  BlockCharges  = 3;
  constexpr int32_t  BlockDebt     = 3;     // down to -3
  constexpr float    BlockLeak     = 0.25f; // damage through the block for each charge below 0
  constexpr uint64_t BlockRegenMs  = 1000;  // one charge back, the block lowered
  constexpr uint64_t BastionMs     = 3000;  // Bastion: the block all round after a parry

  // Diathic: the weapon trees (balance)
  constexpr float    RageDmg       = 0.3f;  // Szał najemnika: +damage below 30% of health ...
  constexpr float    RageLeech     = 0.03f; // ... and leech
  constexpr float    StalkerMul    = 3.f;   // Łowca: the first shot at an enemy who has not noticed the hero
  constexpr uint64_t ButcherMs     = 3000;  // Rzeźnik: attack speed after a kill

  // Diathic: the skills of the weapon trees (Q / E / F); s, cm, multipliers of the hit of the weapon in hand
  constexpr float    BashCd        = 6.f;   // Uderzenie (F): a dash in the block pose, the enemy in front staggered
  constexpr float    BashDist      = 150.f;
  constexpr float    BashSpeed     = 600.f;
  constexpr float    BashMul       = 0.5f;
  constexpr uint64_t BashStaggerMs = 1000;
  constexpr float    BashRunDist   = 600.f; // Szarża: the bash with a run-up, knocks down everyone on the way
  constexpr float    BashRunSpeed  = 1100.f;
  constexpr float    ThrustCd      = 5.f;   // Pchnięcie (1H, Q): a lunge with a forward swing
  constexpr float    ThrustDist    = 300.f;
  constexpr float    ThrustSpeed   = 1000.f;
  constexpr float    ThrustMul     = 1.5f;
  constexpr float    ThrustBleed   = 0.1f;
  constexpr float    DanceCd       = 8.f;   // Taniec ostrzy (1H, E): cuts one after another, each at the cursor
  constexpr float    DanceSpeed    = 0.6f;  // + attack speed
  constexpr float    DanceCutMax   = 0.8f;  // s: the next cut at the latest
  constexpr float    LeapCd        = 8.f;   // Skok (2H, Q): to the cursor with an overhead swing, knocks down around
  constexpr float    LeapDist      = 500.f;
  constexpr float    LeapSpeed     = 1100.f;
  constexpr float    LeapMul       = 1.5f;
  constexpr float    LeapRadius    = 250.f;
  constexpr float    MillCd        = 8.f;   // Młyn (2H, E, held): turning round with the weapon out
  constexpr float    MillTime      = 2.f;
  constexpr float    MillTick      = 0.4f;
  constexpr float    MillMul       = 0.6f;
  constexpr float    MillRadius    = 300.f;
  constexpr float    MillTurn      = 720.f; // degrees a second
  constexpr float    MillMove      = 150.f; // cm/s towards the cursor
  constexpr float    RainCd        = 10.f;  // Deszcz strzał (bow, Q): arrows from the sky on a place
  constexpr float    RainTime      = 2.f;
  constexpr float    RainRadius    = 400.f;
  constexpr float    RainEvery     = 0.12f;
  constexpr float    RainHeight    = 700.f;
  constexpr float    RainMul       = 1.f;
  constexpr float    DisengageCd   = 6.f;   // Odskok (bow, E): a jump back and a shot
  constexpr float    DisengageDist = 300.f;
  constexpr float    DisengageSpeed= 900.f;
  constexpr float    DisengageMul  = 1.f;
  constexpr float    RiposteTime   = 0.7f;  // Riposta: s of the counter swing (the block waits)
  }
