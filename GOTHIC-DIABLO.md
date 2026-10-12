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
| Widoczny, wolny kursor myszy; promień spod kursora liczony co klatkę | `MainWindow::updateArpgAim` |
| Wnętrza: wycięcie wszystkiego powyżej głowy (sufity, dachy, piętra) w promieniu 30 m | `shader/materials/main.frag` — `arpgHidden` |
| Ściany między kamerą a postacią: przezroczysty krąg (siatka pikseli), promień 0,275 wysokości ekranu | `shader/materials/main.frag` — `arpgHidden` |
| Wnętrza: ogień/pochodnie (particle) nad głową też są wycinane | `shader/materials/main.frag` — `arpgHidden` |
| Fog of war: świat ciemnieje do czerni dalej od postaci; promień z opcji gry „Zasięg widzenia” (20–300%, 100% = 15 m) | `shader/arpg.glsl`, `shader/sky/fog.frag`, `Camera::updateArpgView` |
| Wykrywanie wnętrza: promień w górę z głowy postaci + sektor podłogi | `Camera::updateArpgView` |

Dialogi, śmierć, nurkowanie i widok z pierwszej osoby używają oryginalnej kamery.
Dachy i ściany dalej rzucają cienie (ukrywanie działa tylko w widoku kamery).

## Faza 2 — sterowanie myszą (jak Diablo)

| Element | Gdzie |
|---|---|
| Podświetlenie obiektu pod kursorem (nazwa / pasek życia jak fokus w Gothicu): postać, przedmiot, obiekt użytkowy | `PlayerControl::arpgPick` |
| LPM na ziemi: bieg do punktu; przytrzymanie: postać podąża za kursorem | `PlayerControl::arpgMouseDown`, `arpgClickTick` |
| LPM na przedmiocie: podejście i podniesienie | `arpgClickTick` |
| LPM na skrzyni / drzwiach / łóżku itp.: podejście i użycie | `arpgClickTick` |
| LPM na postaci: podejście i rozmowa (broń chowana automatycznie); na ciele: przeszukanie | `arpgClickTick` |
| LPM na wrogu (potwór, ork, wrogi człowiek): dobycie broni w biegu, podejście, atak; przytrzymanie = kolejne ataki | `arpgClickTick` |
| Ctrl + LPM: atak na dowolną postać | `arpgMouseDown` |
| Omijanie przeszkód: prosta linia, a gdy coś zasłania — sieć waypointów; zablokowanie → waynet → rezygnacja | `arpgNextPoint` |
| W/S/A/D przerywa ruch myszą | `PlayerControl::implMove` |
| Wskazywanie ignoruje wycięte wyższe piętra (promień startuje od płaszczyzny cięcia; przedmioty, obiekty i postacie nad „sufitem” pomijane) | `arpgPick`, `arpgCeiling` |
| Podnoszenie przedmiotów także z bronią w ręku (bez animacji); ponawianie, gdy postać jeszcze biegnie; zablokowany ≤3,2 m od celu → akcja i tak się wykona | `Npc::takeItem`, `arpgClickTick` |
| Z (przytrzymanie): etykiety wszystkich przedmiotów i obiektów użytkowych w zasięgu wzroku (mgła wojny, bez ścian); klik w etykietę = podejście i akcja | `MainWindow::paintArpgLabels`, `arpgVisibleObjects` |
| Walka: klik = jeden atak, klik w trakcie ciosu = kombinacja, przytrzymanie = ciągłe ataki; ogłuszony człowiek = cios dobijający | `arpgClickTick` |
| Unik: PPM lub skok (Alt) z bronią w ręku — krok w bok albo odskok w tył; kierunek z W/S/A/D, bez klawiszy = od wroga; postać zostaje zwrócona do wroga | `arpgDodge`, `arpgStartDodge` |

Łuki i czary — w fazie walki.

