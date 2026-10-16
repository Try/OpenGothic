# Gothic II NK → izometryczny ARPG — analiza wykonalności (2026-10-11)

## Werdykt
Osiągalne jako mod / fork silnika wymagający oryginalnej gry. Standalone z assetami Gothic II (modele, mapy, dźwięki) bez zgody THQ Nordic — nie.

## Instalacja użytkownika (F:\SteamLibrary\steamapps\common\Gothic II)
- Wersja Steam z obsługą Workshop (gałąź Unreal wyłączona 2026-10-11, folder UnrealCommunityRemaster usunięty)
- Gothic II Noc Kruka: Data\*.vdf (Anims, Meshes, Sounds, Speech, Textures, Worlds + _Addon), Data\ModVDF pusty
- system\: Gothic2.exe, Spacer2.exe, SystemPack (SystemPack.ini, Data\SystemPack.vdf), launcher Steam Workshop
- gd3d11 — tylko ustawienia, renderer DX11 nieaktywny (brak ddraw.dll)
- _work\Data\Scripts zawiera tylko skompilowane .DAT (GOTHIC.DAT, CAMERA.DAT, FIGHT.DAT…), brak źródeł .d

## Ścieżki techniczne
| Ścieżka | Plusy | Minusy |
|---|---|---|
| **A. Fork OpenGothic (wybrana)** | MIT, C++, aktywny rozwój, gra przechodzalna w 100% (fabuła + dodatek), czyta oryginalne pliki i skrypty Daedalus przez ZenKit, Vulkan/DX12/Metal | brak Union/LeGo, Ikarus częściowo |
| B. Oryginalny silnik + Union/Ikarus/LeGo | najszybszy prototyp | 32-bit, stary renderer, hakowanie binarki |
| C. Unity/Godot + ZenKit (np. Gothic-UnZENity) | pełna swoboda gameplayu | trzeba napisać od nowa AI, questy, dialogi, VM Daedalus |
| D. Remaster na Unreal Engine | ładna grafika | kod zamknięty, jeden autor, wersja beta |

## OpenGothic — punkty zaczepienia w kodzie (common/)
- camera.cpp + game/definitions/cameradefinitions.cpp — kamera ładowana z Camera.dat (instancje CCamSys: CamModNormal, CamModMelee…)
- game/playercontrol.cpp — sterowanie graczem
- game/fightalgo.cpp, game/damagecalculator.cpp — walka
- world/waymatrix.cpp, waypath.cpp — wyszukiwanie ścieżek po sieci waypointów
- world/objects/npc.cpp, item.cpp, game/inventory.cpp — NPC, przedmioty
- Zależności: Tempest (renderer), bullet3 (fizyka), ZenKit (formaty Gothic), dmusic

## Główne wyzwania (projektowe)
1. Mapy projektowane pod kamerę zza pleców: dachy, wnętrza, jaskinie, góry zasłaniają widok
2. Wertykalność (wspinaczka, skoki, wieże, Górnicza Dolina)
3. Walka combo → hack&slash (klik, pasek umiejętności, zaklęcia jako umiejętności)
4. Stałe przedmioty → loot z rzadkością i afiksami
5. Mało przeciwników → większa gęstość / spawnery, nowy balans
6. Dialogi i questy zostają z Daedalus bez zmian
