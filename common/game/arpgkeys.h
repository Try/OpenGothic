#pragma once

#include <Tempest/Event>

#include <cstdint>
#include <string>

// Diathic: the keys of the ARPG mode, one table for the whole game and for the "Sterowanie" page of the main menu.
// Saved in Gothic.ini, section [DIATHIC] (key<Name>=<Tempest key code>); our keys win over Gothic's in the ARPG view.
namespace ArpgKeys {
  enum Action : uint8_t {
    Dodge,          // fikołek
    Weapon,         // dobycie / schowanie broni
    SwapWeapon,     // broń biała <-> łuk / kusza
    SkillA,         // pierwsza umiejętność broni w ręku (drzewko broni)
    SkillB,         // druga umiejętność broni w ręku
    SkillBlock,     // umiejętność drzewka Blok
    Walk,           // chód / bieg
    Sneak,          // skradanie
    PotionHealth,
    PotionMana,
    Equipment,
    Character,
    Skills,
    Map,
    Minimap,        // minimapa -> mapa na grze -> nic
    Labels,         // przytrzymany: etykiety
    Log,            // dziennik (Gothic's MENU_LOG)
    CamLeft,        // obrót kamery (trzymany)
    CamRight,
    Count
    };

  struct Info {
    const char*                 name;   // UTF-8, for the menu
    const char*                 ini;    // name in Gothic.ini
    Tempest::Event::KeyType     def;
    };

  const Info&             info(Action a);
  Tempest::Event::KeyType key(Action a);
  // set a key; an action that had this key gets the old key of 'a' (no key is used twice); saved at once
  void                    setKey(Action a, Tempest::Event::KeyType k);
  void                    resetDefaults();
  bool                    is(Action a, Tempest::Event::KeyType k);
  int                     actionOf(Tempest::Event::KeyType k); // -1: none of ours
  // keys that cannot be given to an action: Esc, 3-0 (spell slots), unknown keys
  bool                    canBind(Tempest::Event::KeyType k);
  std::string             keyName(Tempest::Event::KeyType k);  // UTF-8, e.g. "Spacja", "Lewy Shift", "V"
  }
