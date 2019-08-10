### Support for VOBS & Triggers

+ [x] [zCVob](#zCVob)
    + [x] [zCVobLevelCompo](#zCVobLevelCompo)
    + [x] [oCItem](#oCItem)
    + [x] [oCMOB](#oCMOB)
        + [x] [oCMobInter](#oCMobInter)
            + [ ] oCMobBed
            + [ ] oCMobFire
            + [ ] oCMobLadder
            + [ ] oCMobSwitch
            + [ ] oCMobWheel
            + [x] oCMobContainer
            + [ ] oCMobDoor
    + [ ] zCPFXControler
    + [ ] zCVobAnimate
    + [ ] zCVobLensFlare
    + [ ] zCVobLight
    + [ ] zCVobSpot
    + [x] zCVobStartpoint
    + [x] zCVobSound
        + [ ] zCVobSoundDaytime
    + [x] oCZoneMusic
        + [x] oCZoneMusicDefault
    + [ ] zCZoneZFog
        + [ ] zCZoneZFogDefault
    + [ ] zCZoneVobFarPlane
        + [ ] zCZoneVobFarPlaneDefault
    + [ ] zCCodeMaster
    + [x] zCTrigger
        + [x] zCMover
        + [x] oCTriggerScript
        + [ ] zCTriggerList
        + [x] oCTriggerChangeLevel
        + [ ] zCTriggerWorldStart
    + [ ] zCMessageFilter
    + [ ] zCTouchDamage
        + [ ] oCTouchDamage
    + [ ] zCTriggerUntouch
    + [ ] zCCSCamera
    + [ ] zCEarthquake

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














