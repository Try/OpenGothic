#pragma once

#include <Tempest/Event>
#include <string>
#include <vector>

class KeyCodec final {
  public:
    KeyCodec();

    enum Action : uint8_t {
      Idle,

      Escape,

      Inventory,
      Status,
      Log,
      Map,

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

      Last
      };

    static void   keysStr(const std::string& keys, char buf[], size_t bufSz);
    Action        tr(Tempest::KeyEvent&   e) const;
    Action        tr(Tempest::MouseEvent& e) const;
    void          set(const char* section, const char* key, int32_t code);
    void          setDefaultKeys(const char* preset);

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
      };

    Action      implTr(int32_t code) const;
    static int  fetch(const std::string& keys,size_t s,size_t e);
    static bool keyToStr(int32_t k, char* buf, size_t bufSz);
    static void keyToStr(Tempest::Event::KeyType k, char* buf, size_t bufSz);
    static void keyToStr(Tempest::Event::MouseButton k, char* buf, size_t bufSz);

    void        setupSettings();
    KeyPair     setup(const char* kp);
    static auto parse(const std::string& kp) -> KeyPair;

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

