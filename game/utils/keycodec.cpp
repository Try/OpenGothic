#include "keycodec.h"

#include "gothic.h"
#include <cstring>

std::initializer_list<KeyCodec::K_Key> KeyCodec::keys = {
  {Tempest::Event::K_A,        0x1e00},
  {Tempest::Event::K_B,        0x3000},
  {Tempest::Event::K_C,        0x2e00},
  {Tempest::Event::K_D,        0x2000},
  {Tempest::Event::K_E,        0x1200},
  {Tempest::Event::K_F,        0x2100},
  {Tempest::Event::K_G,        0x2200},
  {Tempest::Event::K_H,        0x2300},
  {Tempest::Event::K_I,        0x1700},
  {Tempest::Event::K_J,        0x2400},
  {Tempest::Event::K_K,        0x2500},
  {Tempest::Event::K_L,        0x2600},
  {Tempest::Event::K_M,        0x3200},
  {Tempest::Event::K_N,        0x3100},
  {Tempest::Event::K_O,        0x1800},
  {Tempest::Event::K_P,        0x1900},
  {Tempest::Event::K_Q,        0x1000},
  {Tempest::Event::K_R,        0x1300},
  {Tempest::Event::K_S,        0x1f00},
  {Tempest::Event::K_T,        0x1400},
  {Tempest::Event::K_U,        0x1600},
  {Tempest::Event::K_V,        0x2f00},
  {Tempest::Event::K_W,        0x1100},
  {Tempest::Event::K_X,        0x2d00},
  {Tempest::Event::K_Y,        0x1500},
  {Tempest::Event::K_Z,        0x2c00},

  {Tempest::Event::K_0,        0x8100},
  {Tempest::Event::K_1,        0x7800},
  {Tempest::Event::K_2,        0x7900},
  {Tempest::Event::K_3,        0x7a00},
  {Tempest::Event::K_4,        0x7b00},
  {Tempest::Event::K_5,        0x7c00},
  {Tempest::Event::K_6,        0x7d00},
  {Tempest::Event::K_7,        0x7e00},
  {Tempest::Event::K_8,        0x7f00},
  {Tempest::Event::K_9,        0x8000},

  {Tempest::Event::K_Up,       0xc800},
  {Tempest::Event::K_Down,     0xd000},
  {Tempest::Event::K_Left,     0xcb00},
  {Tempest::Event::K_Right,    0xcd00},

  {Tempest::Event::K_Back,     0x0e00},
  {Tempest::Event::K_Tab,      0x0f00},
  {Tempest::Event::K_Delete,   0xd300},
  {Tempest::Event::K_Space,    0x3900},
  {Tempest::Event::K_CapsLock, 0x3A00},

  // Left
  {Tempest::Event::K_LControl, 0x1d00},
  {Tempest::Event::K_LShift,   0x2a00},
  {Tempest::Event::K_LAlt,     0x3800},
  // Right
  {Tempest::Event::K_RControl, 0x9d00},
  {Tempest::Event::K_RShift,   0x3600},
  {Tempest::Event::K_RAlt,     0xb800},
  };

std::initializer_list<KeyCodec::M_Key> KeyCodec::mkeys = {
  {Tempest::Event::ButtonLeft,    0x0c02},
  {Tempest::Event::ButtonMid,     0x0e02},
  {Tempest::Event::ButtonRight,   0x0d02},
  {Tempest::Event::ButtonForward, 0x1002},
  {Tempest::Event::ButtonBack,    0x0f02},
  };

KeyCodec::KeyCodec() {
  allKeys = {
    &keyEnd,
    &keyHeal,
    &keyPotion,
    &keyLockTarget,
    &keyParade,
    &keyActionRight,
    &keyActionLeft,
    &keyUp,
    &keyDown,
    &keyLeft,
    &keyRight,
    &keyStrafeLeft,
    &keyStrafeRight,
    &keyAction,
    &keySlow,
    &keySMove,
    &keyWeapon,
    &keySneak,
    &keyLook,
    &keyLookFP,
    &keyInventory,
    &keyShowStatus,
    &keyShowLog,
    &keyShowMap
    };
  Gothic::inst().onSettingsChanged.bind(this,&KeyCodec::setupSettings);
  setupSettings();
  }

KeyCodec::Action KeyCodec::tr(Tempest::KeyEvent& e) const {
  int32_t code = keyToCode(e.key);
  if(e.key==Tempest::KeyEvent::K_ESCAPE)
    return Escape;
  auto act = implTr(code);
  if(act!=KeyCodec::Idle)
    return act;
  if(Tempest::Event::K_0<e.key && e.key<=Tempest::Event::K_9)
    return Action(Weapon+int(e.key-Tempest::Event::K_0));
  if(Tempest::Event::K_0==e.key)
    return Action(Weapon+10);
  if(e.key==Tempest::Event::K_F8)
    return K_F8;
  if(e.key == Tempest::Event::K_K)
    return K_K;
  if(e.key == Tempest::Event::K_O)
    return K_O;
  return Idle;
  }

KeyCodec::Action KeyCodec::tr(Tempest::MouseEvent& e) const {
  int32_t code = keyToCode(e.button);
  auto act = implTr(code);
  if(act!=KeyCodec::Idle)
    return act;
  return Idle;
  }

void KeyCodec::set(std::string_view sec, std::string_view opt, int32_t code) {
  for(auto i:allKeys) {
    const bool chg = i->is(code);
    if(i->k[0]==code)
      i->k[0] = 0;
    if(i->k[1]==code)
      i->k[1] = 0;
    if(chg) {
      std::string s;
      for(auto r:i->k)
        if(r!=0)
          s += toCode(r);
      Gothic::settingsSetS("KEYS",i->key,s);
      }
    }

  auto val = std::string(Gothic::settingsGetS(sec, opt));
  if(val.size()>4)
    val = val.substr(val.size()-4,4);
  val += toCode(code);
  Gothic::settingsSetS(sec, opt, val);
  }

void KeyCodec::setDefaultKeys(std::string_view preset) {
  if(!Gothic::settingsHasSection(preset)) {
    if(Gothic::inst().version().game==1) {
      if(preset=="KEYSDEFAULT0")
        setDefaultKeysG1(); else
        setDefaultKeysG1Alt();
      }
    return;
    }

  for(auto i:allKeys) {
    auto s = Gothic::settingsGetS(preset,i->key);
    Gothic::settingsSetS("KEYS",i->key,s);
    *i = setup(i->key);
    }
  }

void KeyCodec::setDefaultKeysG1() {
  for(auto i:allKeys)
    Gothic::settingsSetS("KEYS",i->key,"");

  Gothic::settingsSetS("KEYS", "keyShowMap",     "3200");
  Gothic::settingsSetS("KEYS", "keyEnd",         "0100");
  Gothic::settingsSetS("KEYS", "keyUp",          "c800c700");
  Gothic::settingsSetS("KEYS", "keyDown",        "d000cf00");
  Gothic::settingsSetS("KEYS", "keyLeft",        "cb00d200");
  Gothic::settingsSetS("KEYS", "keyRight",       "cd00c900");
  Gothic::settingsSetS("KEYS", "keyStrafeLeft",  "d300");
  Gothic::settingsSetS("KEYS", "keyStrafeRight", "d100");
  Gothic::settingsSetS("KEYS", "keyAction",      "1d00");
  Gothic::settingsSetS("KEYS", "keySlow",        "2a00");
  Gothic::settingsSetS("KEYS", "keySMove",       "3800b800");
  Gothic::settingsSetS("KEYS", "keyWeapon",      "3900");
  Gothic::settingsSetS("KEYS", "keySneak",       "1e003700");
  Gothic::settingsSetS("KEYS", "keyLook",        "52001300");
  Gothic::settingsSetS("KEYS", "keyLookFP",      "53002100");
  Gothic::settingsSetS("KEYS", "keyInventory",   "0f000e00");
  Gothic::settingsSetS("KEYS", "keyShowStatus",  "1f003000");
  Gothic::settingsSetS("KEYS", "keyShowLog",     "26003100");

  for(auto i:allKeys) {
    *i = setup(i->key);
    }
  }

void KeyCodec::setDefaultKeysG1Alt() {
  for(auto i:allKeys)
    Gothic::settingsSetS("KEYS",i->key,"");

  Gothic::settingsSetS("KEYS", "keyShowMap",     "3200");
  Gothic::settingsSetS("KEYS", "keyEnd",         "0100");
  Gothic::settingsSetS("KEYS", "keyUp",          "c8001100");
  Gothic::settingsSetS("KEYS", "keyDown",        "d0001f00");
  Gothic::settingsSetS("KEYS", "keyLeft",        "cb001000");
  Gothic::settingsSetS("KEYS", "keyRight",       "cd001200");
  Gothic::settingsSetS("KEYS", "keyStrafeLeft",  "d3001e00");
  Gothic::settingsSetS("KEYS", "keyStrafeRight", "d1002000");
  Gothic::settingsSetS("KEYS", "keyAction",      "1d00");
  Gothic::settingsSetS("KEYS", "keySlow",        "2a00");
  Gothic::settingsSetS("KEYS", "keySMove",       "3800b800");
  Gothic::settingsSetS("KEYS", "keyWeapon",      "3900");
  Gothic::settingsSetS("KEYS", "keySneak",       "2d003700");
  Gothic::settingsSetS("KEYS", "keyLook",        "52001300");
  Gothic::settingsSetS("KEYS", "keyLookFP",      "53002100");
  Gothic::settingsSetS("KEYS", "keyInventory",   "0f000e00");
  Gothic::settingsSetS("KEYS", "keyShowStatus",  "3000");
  Gothic::settingsSetS("KEYS", "keyShowLog",     "3100");

  for(auto i:allKeys) {
    *i = setup(i->key);
    }
  }

std::string KeyCodec::toCode(int32_t code) {
  char ret[5]={};
  for(int i=0;i<4;++i) {
    ret[3-i] = "0123456789abcdef"[code%16];
    code/=16;
    }
  return ret;
  }

KeyCodec::Action KeyCodec::implTr(int32_t code) const {
  if(keyEnd.is(code))
    return Idle;
  if(keyUp.is(code))
    return Forward;
  if(keyDown.is(code))
    return Back;
  if(keyLeft.is(code))
    return RotateL;
  if(keyRight.is(code))
    return RotateR;
  if(keyStrafeLeft.is(code))
    return Left;
  if(keyStrafeRight.is(code))
    return Right;

  if(keyAction.is(code))
    return ActionGeneric;
  if(keyActionLeft.is(code))
    return ActionLeft;
  if(keyActionRight.is(code))
    return ActionRight;
  if(keyParade.is(code))
    return Parade;

  if(keySlow.is(code))
    return Walk;
  if(keySMove.is(code))
    return Jump;

  if(keyWeapon.is(code))
    return Weapon;
  if(keySneak.is(code))
    return Sneak;

  if(keyLook.is(code))
    return LookBack;
  if(keyLookFP.is(code))
    return FirstPerson;

  if(keyInventory.is(code))
    return Inventory;
  if(keyShowStatus.is(code))
    return Status;
  if(keyShowLog.is(code))
    return Log;
  if(keyShowMap.is(code))
    return Map;

  return Idle;
  }

string_frm<64> KeyCodec::keysStr(std::string_view keys) {
  int32_t k0 = fetch(keys,0,4);
  int32_t k1 = fetch(keys,4,8);

  if(k0==0 && k1==0)
    return "";

  auto k0Str = keyToStr(k0);
  auto k1Str = keyToStr(k1);
  if(k0Str.empty() || std::string_view(k0Str)==k1Str){
    return k1Str;
    }
  if(k1Str.empty()){
    return k0Str;
    }

  return string_frm<64>(k0Str,", ",k1Str);
  }

string_frm<64> KeyCodec::keyToStr(int32_t k) {
  using namespace Tempest;

  for(auto& i:keys)
    if(k==i.code) {
      return keyToStr(i.k);
      }
  for(auto& i:mkeys)
    if(k==i.code) {
      return keyToStr(i.k);
      }
  return "";
  }

string_frm<64> KeyCodec::keyToStr(Tempest::Event::KeyType k) {
  if(Tempest::Event::K_0<=k && k<=Tempest::Event::K_9) {
    auto c = char('0' + (k-Tempest::Event::K_0));
    return string_frm<64>(c);
    }
  if(Tempest::Event::K_A<=k && k<=Tempest::Event::K_Z) {
    auto c = char('A' + (k-Tempest::Event::K_A));
    return string_frm<64>(c);
    }

  if(k==Tempest::Event::K_Up) {
    return "CURSOR UP";
    }
  if(k==Tempest::Event::K_Down) {
    return "CURSOR DOWN";
    }
  if(k==Tempest::Event::K_Left) {
    return "CURSOR LEFT";
    }
  if(k==Tempest::Event::K_Right) {
    return "CURSOR RIGHT";
    }
  if(k==Tempest::Event::K_Tab) {
    return "TAB";
    }
  if(k==Tempest::Event::K_Back) {
    return "BACKSPACE";
    }
  if(k==Tempest::Event::K_Space) {
    return "SPACE";
    }
  if(k==Tempest::Event::K_LControl) {
    return "LEFT CONTROL";
    }
  if(k==Tempest::Event::K_RControl) {
    return "RIGHT CONTROL";
    }
  if(k==Tempest::Event::K_Delete) {
    return "DELETE";
    }
  if(k==Tempest::Event::K_LShift) {
    return "LEFT SHIFT";
    }
  if(k==Tempest::Event::K_RShift) {
    return "RIGHT SHIFT";
    }
  if(k==Tempest::Event::K_CapsLock) {
    return "CAPS LOCK";
    }
  if(k==Tempest::Event::K_LAlt) {
    return "LEFT ALT";
    }
  if(k==Tempest::Event::K_RAlt) {
    return "RIGHT ALT";
    }

  return string_frm<64>('?');
  }

string_frm<64> KeyCodec::keyToStr(Tempest::Event::MouseButton k) {
  if(k==Tempest::Event::ButtonLeft) {
    return "MOUSE LEFT";
    }
  if(k==Tempest::Event::ButtonMid) {
    return "MOUSE MID";
    }
  if(k==Tempest::Event::ButtonRight) {
    return "MOUSE RIGHT";
    }
  if(k==Tempest::Event::ButtonForward) {
    return "MOUSE X2";
    }
  if(k==Tempest::Event::ButtonBack) {
    return "MOUSE X1";
    }

  return string_frm<64>('?');
  }

int32_t KeyCodec::keyToCode(Tempest::Event::KeyType t) {
  for(auto& i:keys)
    if(i.k==t)
      return i.code;
  return 0;
  }

int32_t KeyCodec::keyToCode(Tempest::Event::MouseButton t) {
  for(auto& i:mkeys)
    if(i.k==t)
      return i.code;
  return 0;
  }

void KeyCodec::setupSettings() {
  keyEnd         = setup("keyEnd");
  keyHeal        = setup("keyHeal");
  keyPotion      = setup("keyPotion");
  keyLockTarget  = setup("keyLockTarget");
  keyParade      = setup("keyParade");
  keyActionRight = setup("keyActionRight");
  keyActionLeft  = setup("keyActionLeft");
  keyUp          = setup("keyUp");
  keyDown        = setup("keyDown");
  keyLeft        = setup("keyLeft");
  keyRight       = setup("keyRight");
  keyStrafeLeft  = setup("keyStrafeLeft");
  keyStrafeRight = setup("keyStrafeRight");
  keyAction      = setup("keyAction");
  keySlow        = setup("keySlow");
  keySMove       = setup("keySMove");
  keyWeapon      = setup("keyWeapon");
  keySneak       = setup("keySneak");
  keyLook        = setup("keyLook");
  keyLookFP      = setup("keyLookFP");
  keyInventory   = setup("keyInventory");
  keyShowStatus  = setup("keyShowStatus");
  keyShowLog     = setup("keyShowLog");
  keyShowMap     = setup("keyShowMap");
  }

KeyCodec::KeyPair KeyCodec::setup(const char* kp) {
  auto k = parse(Gothic::settingsGetS("KEYS",kp));
  k.key = kp;
  return k;
  }

KeyCodec::KeyPair KeyCodec::parse(std::string_view kp) {
  KeyPair p;
  p.k[0] = fetch(kp,0,4);
  p.k[1] = fetch(kp,4,8);
  return p;
  }

int KeyCodec::fetch(std::string_view keys, size_t s, size_t e) {
  if(s<keys.size() && e<=keys.size() && s<e) {
    int val=0;
    for(size_t i=s;i<e;++i) {
      char k = keys[i];
      if('0'<=k && k<='9')
        val = val*16+(k-'0'); else
      if('a'<=k && k<='f')
        val = val*16+(k-'a'+10); else
      if('A'<=k && k<='F')
        val = val*16+(k-'A'+10); else
        return 0; //error
      }
    return val;
    }
  return 0;
  }
