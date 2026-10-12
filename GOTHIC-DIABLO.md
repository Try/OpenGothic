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
| W/S/A/D = góra/dół/lewo/prawo ekranu | `PlayerControl::implMove` |
| Postać patrzy w stronę kursora myszy; ruch w bok/tył = animacje strafe/cofania (jak Path of Exile 2) | `PlayerControl::implMove`, `MainWindow::updateArpgAim` |
| Widoczny, wolny kursor myszy (nie jest łapany na środku ekranu) | `MainWindow::processMouse` |
| Wnętrza: wycięcie wszystkiego powyżej głowy (sufity, dachy, piętra) w promieniu 30 m | `shader/materials/main.frag` — `arpgHidden` |
| Ściany między kamerą a postacią: przezroczysty krąg (siatka pikseli), promień 0,275 wysokości ekranu | `shader/materials/main.frag` — `arpgHidden` |
| Wnętrza: ogień/pochodnie (particle) nad głową też są wycinane | `shader/materials/main.frag` — `arpgHidden` |
| Wnętrza: wszystko poza pomieszczeniem (za ścianami, >6 m poniżej podłogi, niebo) jest czarne — linia wzroku z 16 promieni wokół postaci | `shader/arpg.glsl`, `shader/sky/fog.frag`, `Camera::updateArpgView` |
| Wykrywanie wnętrza: promień w górę z głowy postaci + sektor podłogi | `Camera::updateArpgView` |

Dialogi, śmierć, nurkowanie i widok z pierwszej osoby używają oryginalnej kamery.
Dachy i ściany dalej rzucają cienie (ukrywanie działa tylko w widoku kamery).
