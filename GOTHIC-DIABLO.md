# gothic-diablo

Fork [OpenGothic](https://github.com/Try/OpenGothic): Gothic II Noc Kruka jako izometryczny action RPG
(Diablo / Path of Exile / Grim Dawn) na oryginalnych plikach gry.

## Uruchomienie (Windows)

1. Pobierz artefakt `opengothic_*_win_x64` z zakładki **Actions** (ostatni zielony build) i rozpakuj.
2. Uruchom:
   ```
   Gothic2Notr.exe -g "F:\SteamLibrary\steamapps\common\Gothic II"
   ```
3. `-classic` wyłącza tryb ARPG (oryginalna kamera i sterowanie OpenGothic).

## Faza 1 — kamera ARPG

| Element | Gdzie |
|---|---|
| Stała kamera izometryczna (kąt 55°, 11 m, bez kolizji, nie obraca się za postacią) | `common/camera.cpp` — `tickArpg` |
| Zoom kółkiem myszy (5–22 m) | `Camera::changeZoom` |
| Q / E (klawisze obrotu) obracają kamerę | `PlayerControl::implMove` |
| W/S/A/D = góra/dół/lewo/prawo ekranu, postać obraca się w stronę ruchu | `PlayerControl::implMove` |
| Widoczny, wolny kursor myszy; jego punkt w świecie liczony co klatkę (na razie nieużywany — wróci przy walce) | `MainWindow::updateArpgAim` |
| Wnętrza: wycięcie wszystkiego powyżej głowy (sufity, dachy, piętra) w promieniu 30 m | `shader/materials/main.frag` — `arpgHidden` |
| Ściany między kamerą a postacią: przezroczysty krąg (siatka pikseli), promień 0,275 wysokości ekranu | `shader/materials/main.frag` — `arpgHidden` |
| Wnętrza: ogień/pochodnie (particle) nad głową też są wycinane | `shader/materials/main.frag` — `arpgHidden` |
| Fog of war: świat ciemnieje do czerni dalej od postaci; promień z opcji gry „Zasięg widzenia” (20–300%, 100% = 15 m) | `shader/arpg.glsl`, `shader/sky/fog.frag`, `Camera::updateArpgView` |
| Wykrywanie wnętrza: promień w górę z głowy postaci + sektor podłogi | `Camera::updateArpgView` |

Dialogi, śmierć, nurkowanie i widok z pierwszej osoby używają oryginalnej kamery.
Dachy i ściany dalej rzucają cienie (ukrywanie działa tylko w widoku kamery).
