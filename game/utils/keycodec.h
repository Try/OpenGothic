#pragma once

#include <Tempest/Event>
#include <string>
#include <vector>

#include "utils/string_frm.h"
#include <optional>

class KeyCodec final {
  public:
    KeyCodec();

    /// Up to two keyboard keys can be mapped to the same action.
    /// This enum determines the order of the mapping within a single key.
    enum class Mapping {
      Primary, Secondary
      };

    static constexpr auto NumMappings = 2;

    enum Action : uint8_t {
      Idle,

      Escape,

      Inventory,
      Status,
      Log,
      Map,
      Heal,
      Potion,

      LookBack,
      FirstPerson,

      Forward,
      Back,
      Left,
      Right,
      RotateL,
      RotateR,
      Jump,

      ActionGeneric,
      ActionLeft,
      ActionRight,
      Parade,

      Walk,
      Sneak,

      Weapon,
      WeaponMele,
      WeaponBow,

      WeaponMage3,
      WeaponMage4,
      WeaponMage5,
      WeaponMage6,
      WeaponMage7,
      WeaponMage8,
      WeaponMage9,
      WeaponMage10,

      K_ENTER, // transform back
      K_F8, // Marvin Mode's F8
      K_K, // Marvin Mode's K
      K_O, // Marvin Mode's O

      Last
      };

    /// Encapsulates an in-game action and a key mapping that caused it to be fired.
    struct ActionMapping {
      Action action;
      Mapping mapping;
      };

    static auto   keysStr(std::string_view keys) -> string_frm<64>;

    Action        tr(Tempest::KeyEvent const& e) const;    
    Action        tr(Tempest::MouseEvent const& e) const;

    /// @brief Gets a mapping out of a key event.
    /// @param e The key event.
    /// @return Requested mapping of an action.
    Mapping       mapping(Tempest::KeyEvent const& e) const;

    void          set(std::string_view section, std::string_view key, int32_t code);
    void          setDefaultKeys(std::string_view preset);

    static int32_t     keyToCode(Tempest::Event::KeyType     t);
    static int32_t     keyToCode(Tempest::Event::MouseButton t);

    static std::string toCode(int32_t scan);

  private:
    struct K_Key {
      K_Key(Tempest::Event::KeyType k, int32_t code):k(k),code(code){}
      Tempest::Event::KeyType k=Tempest::Event::K_NoKey;
      int32_t                 code=0;
      };
    struct M_Key {
      M_Key(Tempest::Event::MouseButton k, int32_t code):k(k),code(code){}
      Tempest::Event::MouseButton k=Tempest::Event::ButtonNone;
      int32_t                     code=0;
      };

    struct KeyPair {
      const char* key = "";
      int32_t     k[2]={};
      bool is(int32_t i) const { return k[0]==i || k[1]==i; }

      /// @brief Determines which mapping (primary/secondary) corresponds to the given key index.
      /// @param i The key index
      /// @return Corresponding mapping or nullopt if key is not related.
      auto mapping(int32_t i) const -> std::optional<Mapping> {
        if(k[0] == i)
          return Mapping::Primary;
        if(k[1] == i)
          return Mapping::Secondary;
        return std::nullopt;
        }
      };

    auto        implTr(int32_t code) const -> ActionMapping;
    static int  fetch(std::string_view keys, size_t s, size_t e);
    static auto keyToStr(int32_t k) -> string_frm<64>;
    static auto keyToStr(Tempest::Event::KeyType k) -> string_frm<64>;
    static auto keyToStr(Tempest::Event::MouseButton k) -> string_frm<64>;

    void        setDefaultKeysG1();
    void        setDefaultKeysG1Alt();
    void        setupSettings();
    KeyPair     setup(const char* kp);
    static auto parse(std::string_view kp) -> KeyPair;

    KeyPair     keyEnd;
    KeyPair     keyHeal;
    KeyPair     keyPotion;
    KeyPair     keyLockTarget;
    KeyPair     keyParade;
    KeyPair     keyActionRight;
    KeyPair     keyActionLeft;
    KeyPair     keyUp;
    KeyPair     keyDown;
    KeyPair     keyLeft;
    KeyPair     keyRight;
    KeyPair     keyStrafeLeft;
    KeyPair     keyStrafeRight;
    KeyPair     keyAction;
    KeyPair     keySlow;
    KeyPair     keySMove;
    KeyPair     keyWeapon;
    KeyPair     keySneak;
    KeyPair     keyLook;
    KeyPair     keyLookFP;
    KeyPair     keyInventory;
    KeyPair     keyShowStatus;
    KeyPair     keyShowLog;
    KeyPair     keyShowMap;

    std::vector<KeyPair*> allKeys;

    static std::initializer_list<K_Key> keys;
    static std::initializer_list<M_Key> mkeys;
  };

