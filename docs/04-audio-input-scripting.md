# OpenGothic Engine - Audio, Input & Scripting Systems

## Table of Contents
1. [Audio System](#audio-system)
2. [Input System](#input-system)
3. [Scripting System](#scripting-system)

---

# Audio System

## Overview

OpenGothic implements a sophisticated audio system with:
- **3D Spatial Audio** with distance attenuation and occlusion
- **Complete DirectMusic Reimplementation** for adaptive music
- **Ambient Sound System** with time-of-day scheduling

## 1.1 Core Audio Engine

### Tempest Audio Integration

**Library**: Tempest Engine's audio module

**Key Classes**:
- `Tempest::SoundDevice` - Audio device management
- `Tempest::SoundEffect` - Sound effect resources
- `Tempest::Sound` - Playing sound instance

**File**: `game/sound/soundfx.h/cpp`

```cpp
class SoundFx {
  std::vector<Tempest::SoundEffect> inst;  // Sound variants
  float    volume = 0.5f;
  bool     loop = false;

  Tempest::Sound getAudio(SoundDevice& dev, std::string_view name);
};
```

**Features**:
- Variant support: `sound_A0`, `sound_A1`, ... (random selection)
- Volume control per sound
- Loop support
- Script definition parsing via `zenkit::ISoundEffect`

## 1.2 3D Spatial Audio

**File**: `game/world/objects/sound.h`, `game/world/worldsound.h/cpp`

### Sound Object

```cpp
class Sound {
  enum Type {
    T_Regular,  // 2D audio
    T_3D        // 3D positioned audio
  };

  Vec3  pos;           // World position
  float maxDist = 3500; // Effective range (35m)
  float occlusion = 0;  // Geometry occlusion factor

  void setPosition(Vec3 p);
  void setMaxDistance(float dist);  // Max 7000 units (70m)
  void setOcclusion(float occ);     // 0 = clear, 1 = fully occluded
};
```

### 3D Audio Manager

**File**: `game/world/worldsound.cpp:105-186`

```cpp
class WorldSound {
  std::vector<std::unique_ptr<Sound>> effect3d;
  Vec3 plPos;  // Player/listener position

  void tick(uint64_t dt) {
    for(auto& snd : effect3d) {
      float dist = (snd->pos - plPos).length();

      if(dist > snd->maxDist) {
        snd->setVolume(0);  // Out of range
      } else {
        float atten = 1.0f - (dist / snd->maxDist);
        float vol = atten * (1.0f - snd->occlusion);
        snd->setVolume(vol);
      }
    }
  }
};
```

**Occlusion Calculation** (via physics raycast):
```cpp
float occlusion = physics->soundOcclusion(soundPos, listenerPos);
// occlusion = wallThickness / 1.5m
sound->setOcclusion(std::min(occlusion, 1.0f));
```

### Ambient Sounds

**Time-Scheduled Ambient System**:
```cpp
struct WSound {
  std::string name;
  Vec3        pos;
  float       range = 3500;
  bool        isAmbient3d = true;

  uint64_t    delay = 0;         // Scheduled start time
  uint64_t    restartTimeout = 0; // Loop delay
  uint64_t    varRandom = 0;      // Random variance

  Sound*      handle = nullptr;
};

std::vector<WSound> ambient;
```

**Update Logic** (`game/world/worldsound.cpp:188-237`):
```cpp
void updateAmbient(uint64_t time) {
  for(auto& amb : ambient) {
    if(time < amb.delay)
      continue;  // Not yet

    if(!amb.handle || !amb.handle->isPlaying()) {
      // Restart ambient
      amb.handle = playSound3d(amb.name, amb.pos, amb.range);

      // Schedule next restart with randomness
      uint64_t restart = amb.restartTimeout;
      uint64_t variance = rand() % amb.varRandom;
      amb.delay = time + restart + variance;
    }
  }
}
```

## 1.3 DirectMusic System

OpenGothic includes a **complete reimplementation** of Microsoft's DirectMusic format in C++!

**Location**: `game/dmusic/` (15 files, ~5000 lines)

### Architecture

```
DirectMusic Engine
├─ Segment      - Musical segments (.sgt files)
├─ Style        - Musical styles (.sty files)
├─ Pattern      - Note patterns with timing
├─ PatternList  - Collection of patterns
├─ Track        - Individual music tracks
├─ Band         - Instrument configurations
├─ Mixer        - Real-time synthesis and mixing
├─ SoundFont    - DLS sample playback
├─ Hydra        - DLS articulation
└─ Riff         - IFF/RIFF parser
```

### Core Components

**1. Segment** (`game/dmusic/segment.h`):
```cpp
class Segment {
  std::vector<Track>         tracks;
  std::vector<PatternList>   patterns;
  std::shared_ptr<Style>     style;
  uint32_t                   length;  // In music time
  uint32_t                   repeats;

  void play(Mixer& mixer, uint64_t time);
};
```

**2. Pattern** (`game/dmusic/pattern.h`):
```cpp
struct Pattern {
  struct Note {
    uint8_t  midiNote;
    uint8_t  velocity;
    uint32_t offset;    // Music time offset
    uint32_t duration;
    // ...
  };

  std::vector<Note> notes;
  uint32_t timeSignature;
  uint32_t length;
};
```

**3. Mixer** (`game/dmusic/mixer.h/cpp`):
- Real-time audio synthesis
- Multi-track mixing
- DLS instrument sample playback
- Tempo and time signature handling

### Adaptive Music Tags

**File**: `game/dmusic/directmusic.cpp:214-256`

```cpp
enum class MusicTag {
  Day,  // Daytime music
  Ngt,  // Night music
  Std,  // Standard/exploration
  Fgt,  // Fight/combat
  Thr   // Threat (enemy nearby)
};

Segment* selectSegment(MusicTag tag, const std::string& zone) {
  // Find segment matching tag and zone
  // Supports transitions between states
}
```

### Transition System

**Embellishment Types**:
```cpp
enum DMUS_EMBELLISHT_TYPES {
  DMUS_EMBELLISHT_NORMAL,  // Standard transition
  DMUS_EMBELLISHT_FILL,    // Musical fill
  DMUS_EMBELLISHT_BREAK,   // Break
  DMUS_EMBELLISHT_INTRO,   // Intro section
  DMUS_EMBELLISHT_END,     // Ending section
};
```

**Seamless Transitions**:
- Calculates musical boundaries (beats, measures)
- Inserts fills or breaks as needed
- Maintains tempo across transitions

### Music Provider Architecture

**File**: `game/gamemusic.cpp`

**Two Implementations**:
1. **OpenGothicMusicProvider**: Custom DirectMusic engine
2. **GothicKitMusicProvider**: Alternative using GothicKit library

```cpp
class GameMusic {
  std::unique_ptr<MusicProvider> impl;

  void setMusic(MusicTag tag, const std::string& zone);
  void stopMusic();
};
```

---

# Input System

## 2.1 Architecture

**Key Classes**:
- `KeyCodec` - Action mapping and key binding
- `PlayerControl` - Player input processing
- `TouchInput` - Mobile/touch support

## 2.2 Action Mapping

**File**: `game/utils/keycodec.h`

### Dual Binding System

```cpp
class KeyCodec {
  enum class Mapping {
    Primary,    // Main key (e.g., W)
    Secondary   // Alt key (e.g., Up Arrow)
  };

  enum Action : uint8_t {
    // Movement (8)
    Forward, Back, Left, Right,
    RotateL, RotateR, Jump, Walk,

    // Combat (11)
    ActionGeneric, ActionLeft, ActionRight, Parade,
    Weapon, WeaponMele, WeaponBow,
    WeaponMage3, WeaponMage4, ..., WeaponMage10,  // 8 spell slots

    // UI (6)
    Inventory, Status, Log, Map, Heal, Potion,

    // Misc (6)
    Sneak, LookBack, FirstPerson, ...

    Last = 73  // Total actions
  };

  struct KeyPair {
    Tempest::Event::KeyType primary;
    Tempest::Event::KeyType secondary;
  };

  KeyPair keys[Action::Last];  // Configurable bindings
};
```

### Key Binding Configuration

**Preset Support**:
```cpp
void KeyCodec::loadPreset(Preset p) {
  switch(p) {
    case Gothic1:
      keys[Forward]  = {K_Up,    K_W};
      keys[Back]     = {K_Down,  K_S};
      keys[Left]     = {K_Left,  K_A};
      keys[Right]    = {K_Right, K_D};
      // ...
      break;

    case Gothic2:
      keys[Forward]  = {K_W, K_Up};
      // Different defaults
      break;
  }
}
```

**INI Persistence** (`game/utils/keycodec.cpp:161-214`):
```cpp
void KeyCodec::save(Tempest::IniFile& ini) {
  for(int i = 0; i < Action::Last; ++i) {
    ini.set("KEYS", actionName(i) + "_P", keys[i].primary);
    ini.set("KEYS", actionName(i) + "_S", keys[i].secondary);
  }
}

void KeyCodec::load(const Tempest::IniFile& ini) {
  // Read from Gothic.ini
}
```

## 2.3 Player Control

**File**: `game/game/playercontrol.h/cpp`

### Movement State Tracking

```cpp
class PlayerControl {
  struct AxisStatus {
    std::array<bool, 2> main;     // Forward/Right
    std::array<bool, 2> reverse;  // Backward/Left

    float value() const {
      return (anyMain() ? 1.f : 0.f) + (anyReverse() ? -1.f : 0.f);
    }
  };

  struct MovementStatus {
    AxisStatus forwardBackward;
    AxisStatus strafeRightLeft;
    AxisStatus turnRightLeft;
  } moveState;

  // Action states
  bool ctrl[KeyCodec::Action::Last] = {};      // General actions
  bool wctrl[WeaponAction::Last] = {};         // Weapon switching
  bool actrl[7] = {};                          // Combat actions

  // Camera
  float rotMouseX = 0, rotMouseY = 0;          // Mouse delta
  float runAngle = 0;                          // Character rotation
};
```

### Input Processing Pipeline

**Event Handlers** (`game/game/playercontrol.cpp:94-147`):
```cpp
void PlayerControl::onKeyPressed(KeyCodec::Action a, Tempest::KeyEvent::Mapping m) {
  ctrl[a] = true;
  handleMovementAction(a, m, true);
}

void PlayerControl::onKeyReleased(KeyCodec::Action a, Tempest::KeyEvent::Mapping m) {
  ctrl[a] = false;
  handleMovementAction(a, m, false);
}

void PlayerControl::handleMovementAction(KeyCodec::Action a, Mapping m, bool pressed) {
  // Update axis state
  switch(a) {
    case Forward:
      moveState.forwardBackward.main[m] = pressed;
      break;
    case Back:
      moveState.forwardBackward.reverse[m] = pressed;
      break;
    case Left:
      moveState.strafeRightLeft.reverse[m] = pressed;
      break;
    case Right:
      moveState.strafeRightLeft.main[m] = pressed;
      break;
    // ...
  }
}
```

### Tick Update

**File**: `game/game/playercontrol.cpp:294-428`

```cpp
void PlayerControl::tickMove(uint64_t dt) {
  Npc* pl = player();
  if(!pl) return;

  // 1. Process weapon switching
  processWeaponSwitch();

  // 2. Process combat actions
  processCombatActions();

  // 3. Movement input
  float forward = moveState.forwardBackward.value();
  float strafe  = moveState.strafeRightLeft.value();
  float turn    = moveState.turnRightLeft.value();

  // 4. Calculate run angle (smooth rotation)
  if(forward != 0 || strafe != 0) {
    float targetAngle = std::atan2(strafe, forward);
    runAngle = lerpAngle(runAngle, targetAngle, dt * 0.01f);
  }

  // 5. Apply movement
  pl->setAnimRotate(runAngle);
  pl->setDirection(forward, strafe);

  // 6. Jump
  if(ctrl[Jump] && !jumpPressed) {
    pl->jump();
    jumpPressed = true;
  }

  // 7. Interaction
  if(ctrl[ActionGeneric]) {
    if(auto* target = pl->focus())
      interact(target);
  }
}
```

### Context-Sensitive Controls

**Gothic 1 vs Gothic 2 Mode** (`game/game/playercontrol.cpp:213-248`):
```cpp
if(g2Ctrl) {
  // Gothic 2: Left/Right = Attack
  if(actrl[ActionLeft])
    pl->攻击Left();
  if(actrl[ActionRight])
    pl->attackRight();
} else {
  // Gothic 1: ActionGeneric = Attack
  if(ctrl[ActionGeneric])
    pl->attack();
}
```

**Marvin Debug Mode** (`game/marvin.h:30-67`):
- **F8**: Toggle flying camera
- **K**: Kill all nearby NPCs
- **O**: Toggle invincibility
- **T**: Teleport to waypoint
- **U**: Unlock all spells

---

# Scripting System

## 3.1 Daedalus Language

**Daedalus** is Gothic's custom scripting language.

**Features**:
- C-like syntax
- Strong typing (int, float, string, func, instance)
- Classes and instances
- Function pointers
- No pointers or dynamic allocation

**Example Script**:
```daedalus
// NPC definition
instance PC_Hero(C_NPC) {
  name     = "Hero";
  guild    = GIL_NONE;
  level    = 1;
  voice    = 1;
  id       = 0;
  // ...
};

// AI state function
func ZS_Talk() {
  perception[PERC_ASSESSTALK] = ZS_Talk_Loop;
  perception[PERC_ASSESSSTOPPROCESSINFOS] = ZS_Talk_End;

  ai_standup(self);
  ai_turntonpc(self, other);
};
```

## 3.2 VM Integration

**Library**: ZenKit's `zenkit::DaedalusVm`

**File**: `game/game/gamescript.h/cpp`

### VM Creation

```cpp
class GameScript {
  zenkit::DaedalusVm vm;

  GameScript(GameSession& owner) {
    // 1. Load script bytecode
    auto script = loadScript("GOTHIC.DAT");

    // 2. Create VM with flags
    auto flags = zenkit::DaedalusVmExecutionFlag::ALLOW_NULL_INSTANCE_ACCESS;

    // 3. Ikarus mod support (removes const protection)
    if(Ikarus::isRequired(script))
      flags |= zenkit::DaedalusVmExecutionFlag::IGNORE_CONST_SPECIFIER;

    vm = zenkit::DaedalusVm(std::move(script), flags);

    // 4. Register external functions (200+)
    initCommon();
  }
};
```

### Global Variables

**Context Variables**:
```cpp
vm.global_self();    // Current NPC executing
vm.global_other();   // Target NPC
vm.global_victim();  // Combat victim
vm.global_hero();    // Player character
vm.global_item();    // Current item
```

## 3.3 External Functions

### Binding Pattern

**File**: `game/game/gamescript.cpp:252-471`

```cpp
template<class F>
void GameScript::bindExternal(const std::string& name, F function) {
  vm.register_external(name, std::function<...>(
    [this, function](auto... v) {
      return (this->*function)(v...);
    }
  ));
}

void GameScript::initCommon() {
  // Helper functions (20+)
  bindExternal("hlp_random", &GameScript::hlp_random);
  bindExternal("hlp_isvalidnpc", &GameScript::hlp_isvalidnpc);
  // ...

  // World functions (30+)
  bindExternal("wld_insertitem", &GameScript::wld_insertitem);
  bindExternal("wld_spawnnpcrange", &GameScript::wld_spawnnpcrange);
  // ...

  // NPC functions (100+)
  bindExternal("npc_changeattribute", &GameScript::npc_changeattribute);
  bindExternal("npc_getattribute", &GameScript::npc_getattribute);
  // ...

  // AI functions (50+)
  bindExternal("ai_output", &GameScript::ai_output);
  bindExternal("ai_gotowp", &GameScript::ai_gotowp);
  // ...

  // Model/Visual functions (20+)
  bindExternal("mdl_setvisual", &GameScript::mdl_setvisual);
  bindExternal("mdl_startfaceani", &GameScript::mdl_startfaceani);
  // ...

  // Sound functions (10+)
  bindExternal("snd_play", &GameScript::snd_play);
  bindExternal("snd_play3d", &GameScript::snd_play3d);
  // ...

  // Total: 200+ external functions
}
```

### Function Categories

| Category | Count | Examples |
|----------|-------|----------|
| Helper (`hlp_*`) | 15 | `hlp_random`, `hlp_strcmp` |
| World (`wld_*`) | 30 | `wld_insertitem`, `wld_settime` |
| NPC (`npc_*`) | 100+ | `npc_getattribute`, `npc_settarget` |
| AI (`ai_*`) | 50 | `ai_output`, `ai_attack` |
| Model (`mdl_*`) | 20 | `mdl_setvisual`, `mdl_applyoverlay` |
| Info (`info_*`) | 10 | `info_addchoice`, `info_clearchoices` |
| Log (`log_*`) | 8 | `log_createtopic`, `log_settopicdone` |
| Task (`ta_*`) | 5 | `ta_min`, `ta_beginoverlay` |
| Mob (`mob_*`) | 5 | `mob_hasitems` |
| Sound (`snd_*`) | 5 | `snd_play`, `snd_play3d` |

### Script Execution

**Invoke State** (`game/game/gamescript.cpp:795-815`):
```cpp
int GameScript::invokeState(Npc* npc, Npc* other, Npc* victim, ScriptFn fn) {
  // Set context
  vm.global_self()->set_instance(npc->handle());
  vm.global_other()->set_instance(other ? other->handle() : nullptr);
  vm.global_victim()->set_instance(victim ? victim->handle() : nullptr);

  try {
    // Execute
    return vm.call_function<int>(fn.ptr);
  } catch(const std::exception& e) {
    Log::e("Script error: ", e.what());
    return 0;
  }
}
```

**Invoke Item** (`game/game/gamescript.cpp:817-830`):
```cpp
void GameScript::invokeItem(Npc* npc, Item* item, ScriptFn fn) {
  vm.global_self()->set_instance(npc->handle());
  vm.global_item()->set_instance(item->handle());

  vm.call_function<void>(fn.ptr);
}
```

## 3.4 Script Lifecycle

### 1. Initialization

```cpp
Gothic::setupGlobalScripts()
  ├─ Load GOTHIC.DAT / OU.BIN
  ├─ Create DaedalusVm
  ├─ Register external functions
  ├─ Load definitions:
  │   ├─ FightAI
  │   ├─ Camera
  │   ├─ Sound
  │   ├─ Particles
  │   ├─ Visual FX
  │   └─ Music
  └─ Initialize global instances
```

### 2. Runtime Execution

**Per-Frame**:
```cpp
GameScript::tick(dt)
  ├─ Process dialogue queue
  ├─ Process delayed events
  └─ Trigger pending cutscenes
```

**NPC AI**:
```cpp
Npc::tickRoutine(dt)
  ├─ Get current AI state
  ├─ invokeState(npc, other, victim, stateFn)
  └─ Process state result
```

**Item Usage**:
```cpp
Npc::useItem(item)
  ├─ Get item use script
  ├─ invokeItem(npc, item, useFn)
  └─ Apply effects
```

### 3. Serialization

**Save** (`game/game/gamescript.cpp:908-945`):
```cpp
void GameScript::saveVar(Serialize& fout) {
  // Save all global script variables
  for(auto& sym : vm.symbols()) {
    if(sym.is_global() && !sym.is_const()) {
      fout.write(sym.name(), sym.get_value());
    }
  }
}
```

## 3.5 Mod Compatibility

### Plugin System

**File**: `game/game/compatibility/`

**Supported Plugins**:
1. **Ikarus** (`ikarus.h/cpp`): Memory manipulation framework
   - Allows scripts to call engine functions by address
   - Removes const protection for script variables
   - Used by many Gothic 2 mods

2. **LeGo** (`lego.h/cpp`): Library of common script functions
   - FrameFunctions, Talents, Dialogues, etc.
   - Built on top of Ikarus

3. **Phoenix** (`phoenix.h/cpp`): Additional API extensions

**Detection**:
```cpp
bool Ikarus::isRequired(const DaedalusScript& script) {
  // Check for Ikarus symbol presence
  return script.find_symbol_by_name("MEM_INITALL") != nullptr;
}
```

---

## Key Takeaways

### Audio System
- ✅ **Complete 3D Audio**: Distance attenuation, occlusion
- ✅ **DirectMusic Reimplementation**: Massive undertaking (~5000 lines)
- ✅ **Adaptive Music**: Tag-based with seamless transitions
- ✅ **Ambient System**: Time-scheduled, randomized

### Input System
- ✅ **Flexible Bindings**: Dual key mapping per action
- ✅ **Multiple Presets**: Gothic 1/2 compatibility
- ✅ **Smooth Controls**: Axis-based movement with interpolation
- ✅ **Context-Sensitive**: Different actions based on state

### Scripting System
- ✅ **Full Daedalus Support**: 200+ external functions
- ✅ **Robust Integration**: Deep engine hooks for NPCs, AI, items
- ✅ **Mod Support**: Ikarus, LeGo, Phoenix plugins
- ✅ **Error Handling**: Graceful script error recovery

---

**Next**: See `05-build-system.md` for CMake configuration and dependencies.
