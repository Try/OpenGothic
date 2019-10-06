### Support for VOBS & Triggers

+ [x] [zCVob](#zCVob)
    + [x] [zCVobLevelCompo](#zCVobLevelCompo)
    + [x] [oCItem](#oCItem)
    + [x] [oCMOB](#oCMOB)
        + [x] [oCMobInter](#oCMobInter)
            + [x] [oCMobBed](#oCMobBed)
            + [ ] [oCMobFire](#oCMobFire)
            + [ ] [oCMobLadder](#oCMobLadder)
            + [x] [oCMobSwitch](#oCMobSwitch)
            + [ ] [oCMobWheel](#oCMobWheel)
            + [x] [oCMobContainer](#oCMobContainer)
            + [x] [oCMobDoor](#oCMobDoor)
    + [ ] [zCPFXControler](#zCPFXControler)
    + [ ] [zCVobAnimate](#zCVobAnimate)
    + [ ] [zCVobLensFlare](#zCVobLensFlare)
    + [ ] [zCVobLight](#zCVobLight)
    + [ ] [zCVobSpot](#zCVobSpot)
    + [x] [zCVobStartpoint](#zCVobStartpoint)
    + [x] [zCVobSound](#zCVobSound)
        + [x] [zCVobSoundDaytime](#zCVobSoundDaytime)
    + [ ] [oCZoneMusic](#oCZoneMusic)
        + [ ] [oCZoneMusicDefault](#oCZoneMusicDefault)
    + [ ] [zCZoneZFog](#zCZoneZFog)
        + [ ] [zCZoneZFogDefault](#zCZoneZFogDefault)
    + [ ] [zCZoneVobFarPlane](#zCZoneVobFarPlane)
        + [ ] [zCZoneVobFarPlaneDefault](#zCZoneVobFarPlaneDefault)
    + [x] [zCCodeMaster](#zCCodeMaster)
    + [x] [zCTrigger](#zCTrigger)
        + [x] [zCMover](#zCMover)
        + [x] [oCTriggerScript](#oCTriggerScript)
        + [x] [zCTriggerList](#zCTriggerList)
        + [x] [oCTriggerChangeLevel](#oCTriggerChangeLevel)
        + [ ] [zCTriggerWorldStart](#zCTriggerWorldStart)
    + [ ] [zCMessageFilter](#zCMessageFilter)
    + [ ] [zCTouchDamage](#zCTouchDamage)
        + [ ] [oCTouchDamage](#oCTouchDamage)
    + [ ] [zCTriggerUntouch](#zCTriggerUntouch)
    + [ ] [zCCSCamera](#zCCSCamera)
    + [ ] [zCEarthquake](#zCEarthquake)

#### zCVob
```c++
struct zCVob {
    string vobName;
    string visual;
    bool   showVisual;
    enum   visualCamAlign;
    enum   visualAniMode;
    float  visualAniModeStrength;
    float  vobFarClipZScale;
    bool   cdStatic;
    bool   cdDyn;
    bool   staticVob;
    bool   dynamicShadow;
    float  zBias;
    bool   isAmbient;
    };
```
#### zCVobLevelCompo
```c++
struct zCVobLevelCompo : zCVob {};
```
#### oCItem
```c++
struct oCItem : zCVob {
    instance itemInstance;
    };
```
#### oCMOB
```c++
struct oCMOB : zCVob {
    string focusName;
    int    hitpoints;
    int    damage;
    bool   moveable;
    bool   takeable;
    bool   focusOverride;
    enum   soundMaterial;
    string visualDestroyed;
    string owner;
    string ownerGuild;
    bool   isDestroyed;
    };
```

#### oCMobInter
```c++
struct oCMobInter : oCMOB {
    int    stateNum;
    string triggerTarget;
    string useWithItem;
    string conditionFunc;
    string onStateFunc;
    };
```

#### oCMobBed
```c++
struct oCMobBed : oCMobInter {};
```

#### oCMobFire
```c++
// Not implemented
struct oCMobFire : oCMobInter {
    string fireSlot;
    string fireVobtreeName;
    };
```

#### oCMobLadder
```c++
// Not implemented
struct oCMobLadder : oCMobInter {};
```

#### oCMobSwitch
```c++
struct oCMobSwitch : oCMobInter {};
```

#### oCMobWheel
```c++
// Not implemented
struct oCMobWheel : oCMobInter {};
```

#### oCMobDoor
```c++
struct oCMobDoor : oCMobInter {
    // Not parsed yet
    };
```

#### zCPFXControler
```c++
struct zCPFXControler : zCVob {
    // Not parsed yet
    };
```

#### zCVobAnimate
```c++
struct zCVobAnimate : zCVob {
    // Not parsed yet
    };
```

#### zCVobLensFlare
```c++
struct zCVobLensFlare : zCVob {
    // Not parsed yet
    };
```

#### zCVobLight
```c++
struct zCVobLight : zCVob {
    // Not parsed yet
    };
```

#### zCVobSpot
```c++
struct zCVobSpot : zCVob {};
```

#### zCVobStartpoint
```c++
struct zCVobStartpoint : zCVob {};
```

#### zCVobSound
```c++
struct zCVobSound : zCVob {
    float  sndVolume;
    enum   sndType;
    float  sndRandDelay;
    float  sndRandDelayVar;
    bool   sndStartOn;
    bool   sndAmbient3D;
    bool   sndObstruction;
    float  sndConeAngle;
    enum   sndVolType;
    float  sndRadius;
    string sndName;
    };
```

#### zCVobSoundDaytime
```c++
struct zCVobSoundDaytime : zCVobSound {
    float  sndStartTime;
    float  sndEndTime;
    string sndName2;
    };
```

#### oCZoneMusic
```c++
struct oCZoneMusic : zCVob {
    bool  enabled;
    int   priority;
    bool  ellipsoid;
    float reverbLevel;
    float volumeLevel;
    bool  loop;
    };
```

#### oCZoneMusicDefault
```c++
struct oCZoneMusicDefault : oCZoneMusic {};
```

#### zCZoneZFog
```c++
struct zCZoneZFog : zCVob {
    // Not parsed yet
    };
```

#### zCZoneZFogDefault
```c++
struct zCZoneZFogDefault : zCZoneZFog {
    // Not parsed yet
    };
```

#### zCZoneVobFarPlane
```c++
struct zCZoneVobFarPlane : zCVob {
    // Not parsed yet
    };
```

#### zCZoneVobFarPlane
```c++
struct zCZoneVobFarPlaneDefault : zCZoneVobFarPlane {
    // Not parsed yet
    };
```

#### zCCodeMaster
```c++
// Partial implementation: no order
struct zCCodeMaster : zCVob {
    string   triggerTarget;
    bool     orderRelevant;
    bool     firstFalseIsFailure;
    bool     untriggeredCancels;
    string   triggerTargetFailure;
    string[] slaveVobName;
    };
```

#### zCTrigger
```c++
struct zCTrigger : zCVob {
    string  triggerTarget;
    uint8_t flags;
    uint8_t filterFlags;
    string  respondToVobName;
    int     numCanBeActivated;
    float   retriggerWaitSec;
    float   damageThreshold;
    float   fireDelaySec;
    };
```

#### zCMover
```c++
struct zKeyFrame final {
    float3 pos={};
    float4 rotation={};  // Quaternion
    };
// Partial implementation: no animation
struct zCMover : zCTrigger {
    enum        moverBehavior;
    float       touchBlockerDamage;
    float       stayOpenTimeSec;
    bool        moverLocked;
    bool        autoLinkEnabled;
#ifdef GOTHIC2
    bool        autoRotate;
#endif    
    float       moveSpeed;
    enum        posLerpType;
    enum        speedType;
    zKeyFrame[] keyframes;
    string      sfxOpenStart;
    string      sfxOpenEnd;
    string      sfxMoving;
    string      sfxCloseStart;
    string      sfxCloseEnd;
    string      sfxLock;
    string      sfxUnlock;
    string      sfxUseLocked;
    };
```

#### oCTriggerScript
```c++
struct oCTriggerScript : zCTrigger {
    string scriptFunc;
    };
```

#### zCTriggerList
```c++
struct zCTriggerList::Entry final {
#ifdef GOTHIC2
    string triggerTarget{id};
#else
    string slaveVobName{id};
#endif
    float  fireDelay{id};
    };
// Fire delay is not implemented 
struct zCTriggerList : zCTrigger {
    int32   listProcess;
    Entry[] list;
    };
```

#### oCTriggerChangeLevel
```c++
struct oCTriggerChangeLevel : zCTrigger {
    string levelName;
    string startVobName;
    };
```

#### zCTriggerWorldStart
```c++
struct zCTriggerWorldStart : zCTrigger {
    // Not parsed
    };
```

#### zCMessageFilter
```c++
struct zCMessageFilter : zCVob {
    // Not parsed
    };
```

#### zCTouchDamage
```c++
struct zCTouchDamage : zCVob {
    // Not parsed
    };
```

#### oCTouchDamage
```c++
struct oCTouchDamage : zCTouchDamage {
    // Not parsed
    };
```

#### zCTriggerUntouch
```c++
struct zCTriggerUntouch : zCVob {
    // Not parsed
    };
```

#### zCCSCamera
```c++
struct zCCSCamera : zCVob {
    // Not parsed
    };
```

#### zCEarthquake
```c++
struct zCEarthquake : zCVob {
    // Not parsed
    };
```







