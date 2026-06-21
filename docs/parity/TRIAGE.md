# OpenGothic issue triage

Disposition of every open GitHub issue against the Gothic-2 parity loop (decompile `Gothic2.exe` → compare → fix). Generated alongside `PARITY_MAP.md`.

**9 fixed** (code applied + builds) · **27 analyzed/deferred** (findings docs; need runtime or larger work) · **75 out-of-scope** (feature / platform / hardware / mod / Gothic 1). Total 111.

## FIXED

| # | bucket | title | note |
|---|---|---|---|
| [#946](https://github.com/Try/OpenGothic/issues/946) | animation | setSkeleton animation adjustment is not implemented | null-guard setSkeleton |
| [#920](https://github.com/Try/OpenGothic/issues/920) | gameplay | Time domain&stacking of "time.slw" ? | time.slw no longer stacks |
| [#907](https://github.com/Try/OpenGothic/issues/907) | saveload | Stone on pedestal dissapears after game load | re-attach mobsi slot visual on load |
| [#881](https://github.com/Try/OpenGothic/issues/881) | platform | Add `-c` arg to specify `Gothic.ini` location | -c Gothic.ini path arg |
| [#772](https://github.com/Try/OpenGothic/issues/772) | input | Ignore mouse-click when bringing window back into focus | swallow refocus menu click |
| [#647](https://github.com/Try/OpenGothic/issues/647) | ai | Incorrect offset of npc when interacting with "THRONE" | preserve NPC Y at mobsi slot |
| [#639](https://github.com/Try/OpenGothic/issues/639) | ai | More perc issues | sneak footstep perception |
| [#585](https://github.com/Try/OpenGothic/issues/585) | ai | NPC walk-teleport mislignment | exact-snap on goto arrival |
| [#92](https://github.com/Try/OpenGothic/issues/92) | ui | Chest labeled CHEST_LOBART | STRING-type guard on mob name |

## DEFER

| # | bucket | title | note |
|---|---|---|---|
| [#940](https://github.com/Try/OpenGothic/issues/940) | world | Picking up: things disappear a bit too early | defer world-item removal to take-anim contact event (changes takeItem contract) |
| [#939](https://github.com/Try/OpenGothic/issues/939) | physics | Jumping uphill clips floor | sweep ascending jump vs slope; core movement, needs playtest |
| [#911](https://github.com/Try/OpenGothic/issues/911) | world | Temple trap is not killing me | route touch-damage via oCNpc::OnDamage; needs trap data + runtime |
| [#903](https://github.com/Try/OpenGothic/issues/903) | physics | Jarkendar polypodiums collisions | hoist windy collision guard to MODEL/MORPH path; needs ZEN asset |
| [#901](https://github.com/Try/OpenGothic/issues/901) | animation | VFX events in animations | animation-event VFX lighting under HDR pipeline; architectural |
| [#899](https://github.com/Try/OpenGothic/issues/899) | sound | Implement missing sound options | soundUseReverb needs OpenAL EFX backend (soundEnabled already works) |
| [#898](https://github.com/Try/OpenGothic/issues/898) | rendering | Blinking texture | texture flicker; needs the repro save to localize material |
| [#873](https://github.com/Try/OpenGothic/issues/873) | input | b,c,m keys not working sometimes | unmapped keys wedge dispatch; fix is in Tempest submodule |
| [#871](https://github.com/Try/OpenGothic/issues/871) | rendering | Magic fog around potions is missing, light problem | mesh-emitter CPU particles + light clamp; feature+exposure |
| [#858](https://github.com/Try/OpenGothic/issues/858) | gameplay | Rupert pee during talking or trading | force-detach addressed NPC from routine scheme at dialog start |
| [#857](https://github.com/Try/OpenGothic/issues/857) | gameplay | Cooking with frying pan not working in fast mode | cooking mobsi loop re-entry; needs runtime |
| [#799](https://github.com/Try/OpenGothic/issues/799) | platform | Command line argument compatibility with original game | umbrella: original -z* CLI aliases as small follow-ups |
| [#791](https://github.com/Try/OpenGothic/issues/791) | ai | Npc full-day simulation | daily-routine convergence/pathing; runtime state machine |
| [#749](https://github.com/Try/OpenGothic/issues/749) | input | G2. Bartok can't hit anything with bow. | Bartok bow miss; gravity-arc/hitchance, needs runtime+mod |
| [#719](https://github.com/Try/OpenGothic/issues/719) | stability | Changing a global script variable does not re-trigger an important inf | re-issue PERC_ASSESSTALK after arrival idle; runtime AI loop |
| [#713](https://github.com/Try/OpenGothic/issues/713) | ui | Enabling / disabling subtitles | wire subTitlesAmbient/Noise to SVM path; needs classification |
| [#707](https://github.com/Try/OpenGothic/issues/707) | scripting | Implement edit focus in marvin-mode | marvin edit-focus command; new command+UI feature |
| [#684](https://github.com/Try/OpenGothic/issues/684) | ai | Mercenaries, outfit, talking, bug | mercenary refuse-talk latch; needs save repro |
| [#665](https://github.com/Try/OpenGothic/issues/665) | rendering | "bloodDetail" param | bloodDetail drives an absent blood-VFX emission path |
| [#656](https://github.com/Try/OpenGothic/issues/656) | ai | Npc runtime corner cases | isMonster keys off live guild not trueGuild; broad refactor |
| [#642](https://github.com/Try/OpenGothic/issues/642) | physics | Cannot Jump-Climb to edge from standing position with free space at fr | decouple ledge probe from forward-space short-circuit; needs playtest |
| [#637](https://github.com/Try/OpenGothic/issues/637) | world | Move trigger Problems | elevator floor-clip (rider/tick-order); needs save repro |
| [#620](https://github.com/Try/OpenGothic/issues/620) | gameplay | Magic | umbrella: missing precipitation global-FX + heal VFX |
| [#523](https://github.com/Try/OpenGothic/issues/523) | scripting | Revisit `aiQueue` and `aiQueueOverlay` | route PointAt via aiQueueOverlay; save-format + maintainer-invalid |
| [#312](https://github.com/Try/OpenGothic/issues/312) | physics | Climbing issues | port dedicated DetectClimbUpLedge probe window; core traversal |
| [#262](https://github.com/Try/OpenGothic/issues/262) | input | Gothic2 controls(useGothic1Controls=0) issues | target-lock action: keyLockTarget parsed but unmapped |
| [#177](https://github.com/Try/OpenGothic/issues/177) | gameplay | G2 NotR - weapons smithing needs detailled investigation - multiple er | equipped/sellable stack model + forge anim-event timing |

## OUT-OF-SCOPE

| # | bucket | title | note |
|---|---|---|---|
| [#945](https://github.com/Try/OpenGothic/issues/945) | rendering | Global illumination V2 | global illumination v2 — feature |
| [#944](https://github.com/Try/OpenGothic/issues/944) `[G1]` | uncategorized | OpenGothic (Gothic 1) big bug report | Gothic 1 umbrella bug report — needs Gothic1.exe |
| [#934](https://github.com/Try/OpenGothic/issues/934) `[G1]` | animation | [G1] Name of sitting NPC | Gothic 1 — needs Gothic1.exe (not imported) |
| [#932](https://github.com/Try/OpenGothic/issues/932) `[G1]` | rendering | [G1] Extreme bright Light Sources | Gothic 1 — needs Gothic1.exe (not imported) |
| [#931](https://github.com/Try/OpenGothic/issues/931) `[G1]` | physics | [G1]  Climbing near Ore Lamps | Gothic 1 — needs Gothic1.exe (not imported) |
| [#922](https://github.com/Try/OpenGothic/issues/922) `[G1]` | sound | [G1] Sfx/Speech Imbalance | Gothic 1 — needs Gothic1.exe (not imported) |
| [#909](https://github.com/Try/OpenGothic/issues/909) | world | Body visible through floor | no ragdoll/corpse-settle physics — missing feature |
| [#888](https://github.com/Try/OpenGothic/issues/888) | platform | CI workflow for benchmarking (Regression test) | platform/build/packaging — not a parity bug |
| [#887](https://github.com/Try/OpenGothic/issues/887) | platform | Create project homepage using GH Pages | platform/build/packaging — not a parity bug |
| [#882](https://github.com/Try/OpenGothic/issues/882) | platform | Add a DEB repository | platform/build/packaging — not a parity bug |
| [#864](https://github.com/Try/OpenGothic/issues/864) | mods | LHiver Mod: Burning rotten corpses | mod-specific — needs mod assets/runtime repro |
| [#852](https://github.com/Try/OpenGothic/issues/852) | mods | [NOTR/Remaster Mod] Can't get back from Jharkendar to Khorinis | mod-specific — needs mod assets/runtime repro |
| [#842](https://github.com/Try/OpenGothic/issues/842) | platform | Ubuntu 24.04 configure and build warnings | platform/build/packaging — not a parity bug |
| [#841](https://github.com/Try/OpenGothic/issues/841) | scripting | Implement missing `PLAYER_` callbacks | PLAYER_MOB_* callbacks are dead (no xref) in the original too |
| [#826](https://github.com/Try/OpenGothic/issues/826) | mods | LHiver Mod: warning messages at start | mod-specific — needs mod assets/runtime repro |
| [#818](https://github.com/Try/OpenGothic/issues/818) | mods | LHiver Mod: equipping 2H + shield | mod-specific — needs mod assets/runtime repro |
| [#817](https://github.com/Try/OpenGothic/issues/817) | gameplay | Stacking rings | by-design (both rings equip; labeled invalid) |
| [#780](https://github.com/Try/OpenGothic/issues/780) | platform | Request: Prebuilt macOS .app bundle for OpenGothic (M1/M2 support) | platform/build/packaging — not a parity bug |
| [#771](https://github.com/Try/OpenGothic/issues/771) | mods | The Nightmare modification is not running | mod-specific — needs mod assets/runtime repro |
| [#747](https://github.com/Try/OpenGothic/issues/747) | mods | Issue launching any type of mods over Linux version of .sh file | mod-specific — needs mod assets/runtime repro |
| [#746](https://github.com/Try/OpenGothic/issues/746) | stability | G1, G2, G2Classic Crash (vanilla version) | hardware/driver-specific — needs that hardware |
| [#737](https://github.com/Try/OpenGothic/issues/737) | mods | Standard mod ini fails to start the game | mod-specific — needs mod assets/runtime repro |
| [#730](https://github.com/Try/OpenGothic/issues/730) `[G1]` | ai | [Gothic 1] Swampsharks follow / attack indefinitely | Gothic 1 — needs Gothic1.exe (not imported) |
| [#726](https://github.com/Try/OpenGothic/issues/726) | stability | Gothic I and II:  Hang after main menu on Intel(R) Arc(TM) B580 Graphi | hardware/driver-specific — needs that hardware |
| [#712](https://github.com/Try/OpenGothic/issues/712) | stability | Crash on macOS Sequoia | hardware/driver-specific — needs that hardware |
| [#711](https://github.com/Try/OpenGothic/issues/711) | platform | Game do not start / No log entry available (windows) | platform/build/packaging — not a parity bug |
| [#710](https://github.com/Try/OpenGothic/issues/710) | rendering | Graphical issue on intel core ultra 5 (arch linux) | rendering feature/hardware-specific |
| [#698](https://github.com/Try/OpenGothic/issues/698) | rendering | Native HDR Output | native HDR output — feature |
| [#681](https://github.com/Try/OpenGothic/issues/681) | rendering | Virtual shadowmap | virtual shadowmap — feature |
| [#680](https://github.com/Try/OpenGothic/issues/680) | performance | Got lags when use lamp in G2NoTR. | performance/hardware — needs profiling on the device |
| [#662](https://github.com/Try/OpenGothic/issues/662) | ui | Feature Request: Pressing space bar for equipped magic spell switcher  | equipped-spell ring switcher — new UI feature |
| [#661](https://github.com/Try/OpenGothic/issues/661) `[G1]` | gameplay | [Gothic 1] Magic Quirks | Gothic 1 — needs Gothic1.exe (not imported) |
| [#657](https://github.com/Try/OpenGothic/issues/657) | world | Full world simulation issues | unlimited-distance world simulation — architectural feature |
| [#649](https://github.com/Try/OpenGothic/issues/649) | platform | Display builds only from only `master` branch in nightly | platform/build/packaging — not a parity bug |
| [#643](https://github.com/Try/OpenGothic/issues/643) | stability | Crash on game start Steam G2:NotR, Intel Mac | hardware/driver-specific — needs that hardware |
| [#626](https://github.com/Try/OpenGothic/issues/626) `[G1]` | gameplay | [Gothic 1] Trading with Merchants working the "Gothic 2 way" allowing  | Gothic 1 — needs Gothic1.exe (not imported) |
| [#624](https://github.com/Try/OpenGothic/issues/624) `[G1]` | ai | [Gothic 1] equipped weapon cannot be looted from NPCs inventory | Gothic 1 — needs Gothic1.exe (not imported) |
| [#623](https://github.com/Try/OpenGothic/issues/623) | rendering | Walking under water & switch for moving bookshelf | trigger/water; main case fixed upstream, residual claimed by maintainer |
| [#614](https://github.com/Try/OpenGothic/issues/614) `[G1]` | saveload | [Gothic 1] Common directory for save-games with Gothic 2 NOTR | Gothic 1 — needs Gothic1.exe (not imported) |
| [#600](https://github.com/Try/OpenGothic/issues/600) | world | Mount `_work` directory to virtual file system | mount _work to VFS — feature |
| [#594](https://github.com/Try/OpenGothic/issues/594) | platform | Update wiki | platform/build/packaging — not a parity bug |
| [#584](https://github.com/Try/OpenGothic/issues/584) | mods | It's really dark outside with certain graphics mods | mod-specific — needs mod assets/runtime repro |
| [#577](https://github.com/Try/OpenGothic/issues/577) | stability | Crash unless meshlets disabled | AMDVLK mesh-shader driver bug; workaround -ms 0 exists (shader-side) |
| [#573](https://github.com/Try/OpenGothic/issues/573) | mods | provide an API for moders to port DMA mods to OpenGothic | DMA mod API — feature |
| [#535](https://github.com/Try/OpenGothic/issues/535) | mods | Implement FOV/interface features from SystemPack | SystemPack FOV/interface — feature |
| [#521](https://github.com/Try/OpenGothic/issues/521) | platform | Issue in install from AUR | platform/build/packaging — not a parity bug |
| [#520](https://github.com/Try/OpenGothic/issues/520) | platform | Doesn't start on Linux Mint | platform/build/packaging — not a parity bug |
| [#518](https://github.com/Try/OpenGothic/issues/518) | platform | Gothic Classic / Gothic II Complete Classic for Switch. Results | platform/build/packaging — not a parity bug |
| [#517](https://github.com/Try/OpenGothic/issues/517) | platform | Appimage binaries | platform/build/packaging — not a parity bug |
| [#511](https://github.com/Try/OpenGothic/issues/511) | gameplay | G2 Classic 1.1 mod: killed creatures resurrect | not a surgical Gothic-2 parity fix |
| [#494](https://github.com/Try/OpenGothic/issues/494) | gameplay | Implement item-effects | item-effects — new feature subsystem |
| [#479](https://github.com/Try/OpenGothic/issues/479) | ui | Integrating Accessibility into Open Gothic | accessibility — feature |
| [#476](https://github.com/Try/OpenGothic/issues/476) | rendering | Raspberry Pi 4 graphic issues | rendering feature/hardware-specific |
| [#451](https://github.com/Try/OpenGothic/issues/451) | rendering | Problems with starting OpenGothic on 2nd GFX card. | rendering feature/hardware-specific |
| [#410](https://github.com/Try/OpenGothic/issues/410) | saveload | Load the save games made with the original G2 | load original .sav — large legacy save-format reader (feature) |
| [#409](https://github.com/Try/OpenGothic/issues/409) | uncategorized | [GI] small assortment of interesting bugs | G1 assortment — needs Gothic1.exe + itemization |
| [#407](https://github.com/Try/OpenGothic/issues/407) | rendering | MacOS: Issue with player and NPC model rendering | rendering feature/hardware-specific |
| [#387](https://github.com/Try/OpenGothic/issues/387) | rendering | An autoexposure/eye adaptation system for HDR implemenation request | autoexposure/eye-adaptation — feature |
| [#386](https://github.com/Try/OpenGothic/issues/386) | rendering | A tree (vegetation) transperency shader implementaion request | tree transparency shader — feature |
| [#357](https://github.com/Try/OpenGothic/issues/357) | rendering | [gothic1] No magical barrier | rendering feature/hardware-specific |
| [#304](https://github.com/Try/OpenGothic/issues/304) | platform | Suggestion: Add a .clang-format file | platform/build/packaging — not a parity bug |
| [#287](https://github.com/Try/OpenGothic/issues/287) | input | Joystick support (for turning) | joystick turning — feature |
| [#231](https://github.com/Try/OpenGothic/issues/231) | scripting | Support for Ikarus/LeGo | Ikarus/LeGo — large compat feature |
| [#230](https://github.com/Try/OpenGothic/issues/230) | uncategorized | Multiplayer | multiplayer — out of project scope |
| [#215](https://github.com/Try/OpenGothic/issues/215) | scripting | Finalize 'Marvin' mode | finalize Marvin mode — broad ongoing feature |
| [#184](https://github.com/Try/OpenGothic/issues/184) | mods | HumanRemaster + Gothic II Gold Remaster issue v1.0.1123 and v1.0.1177 | mod-specific — needs mod assets/runtime repro |
| [#182](https://github.com/Try/OpenGothic/issues/182) | physics | Collision model | not a surgical Gothic-2 parity fix |
| [#171](https://github.com/Try/OpenGothic/issues/171) | rendering | Implement weather effects (rain) | rain/weather — feature |
| [#127](https://github.com/Try/OpenGothic/issues/127) | platform | Flathub/Flatpak package? | platform/build/packaging — not a parity bug |
| [#126](https://github.com/Try/OpenGothic/issues/126) | saveload | Implement save/load for effects | save/load for effects — feature |
| [#117](https://github.com/Try/OpenGothic/issues/117) | sound | Audible difference in music playback | audio fidelity — needs runtime A/B; no clear code defect |
| [#105](https://github.com/Try/OpenGothic/issues/105) | rendering | [Feature Request] VR viewing | VR — feature |
| [#75](https://github.com/Try/OpenGothic/issues/75) | platform | Will it be possible to compile this for android? | platform/build/packaging — not a parity bug |
| [#39](https://github.com/Try/OpenGothic/issues/39) | scripting | Scripting: api-comlete gothic2 | api-complete G2 — huge ongoing |
| [#14](https://github.com/Try/OpenGothic/issues/14) | platform | Analyze and test non-ascii file path support | platform/build/packaging — not a parity bug |
