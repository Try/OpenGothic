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
| Ściany między kamerą a postacią: przezroczysty krąg (w fazie 3 zmieniony — patrz niżej) | `shader/materials/main.frag` — `arpgHidden` |
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
| Omijanie przeszkód: prosta linia, a gdy coś zasłania — sieć waypointów (co 0,5 s) | `arpgNextPoint` |
| Omijanie lokalne („jak robot sprzątający”): po zderzeniu marsz wzdłuż przeszkody po stronie bliższej celowi (normalna kolizji z `MoveAlgo::onMoveFailed`), „czułki” — 2 promienie na wysokości klatki piersiowej — skręt o 30/60/90° w stronę wolną, trzymanie się jednej strony | `arpgSteer`, `arpgFeelerClear`, `MoveAlgo::lastMoveFail` |
| Zablokowanie: brak postępu przez 0,4 s → druga strona; 2× → waynet; 4× (~1,6 s) → rezygnacja | `arpgNextPoint` |
| Obrót kamery: Q/E (zawsze, także w walce) albo przeciąganie środkowym przyciskiem; kąt nachylenia stały; zwykły klik środkowym = dotychczasowe przypisanie (np. broń) | `PlayerControl::tickMove`, `MainWindow::mouseDragEvent` |
| W/S/A/D przerywa ruch myszą | `PlayerControl::implMove` |
| Wskazywanie ignoruje wycięte wyższe piętra (promień startuje od płaszczyzny cięcia; przedmioty, obiekty i postacie nad „sufitem” pomijane) | `arpgPick`, `arpgCeiling` |
| Wskazywanie według tych samych zasad co Z: tylko to, co postać widzi (zasięg mgły wojny, linia wzroku od oczu — bez rzeczy za ścianą i piętro niżej); etykieta pod kursorem w stylu Z, wrogowie na czerwono | `arpgPick`, `arpgCanSee`, `MainWindow::paintArpgFocus` |
| Podnoszenie przedmiotów także z bronią w ręku (bez animacji); ponawianie, gdy postać jeszcze biegnie; zablokowany ≤3,2 m od celu → akcja i tak się wykona | `Npc::takeItem`, `arpgClickTick` |
| Z (przytrzymanie): etykiety wszystkich przedmiotów i obiektów użytkowych w zasięgu wzroku (mgła wojny, bez ścian); klik w etykietę = podejście i akcja | `MainWindow::paintArpgLabels`, `arpgVisibleObjects` |
| Walka: klik = jeden atak; klik w trakcie ciosu zostaje zapamiętany i wykonany dokładnie w oknie kombinacji (nigdy za wcześnie — Gothic wtedy przerywa combo); przytrzymanie = ciągłe ataki łączone w kombinacje; ogłuszony człowiek = cios dobijający | `arpgClickTick`, `Pose::comboTiming` |
| Wskaźnik combo nad postacią: pasek napełnia się do momentu kolejnego ciosu, złoty = teraz (kurczy się do końca okna), czerwony = przerwane; złote kwadraty = połączone ciosy, biała kropka = cios w kolejce | `MainWindow::paintArpgCombo` |
| Unik: PPM lub skok (Alt) z bronią w ręku — krok w bok albo odskok w tył; kierunek z W/S/A/D, bez klawiszy = od wroga; postać zostaje zwrócona do wroga | `arpgDodge`, `arpgStartDodge` |

Łuki i czary — w fazie walki.


## Faza 3 — zasłanianie

| Element | Gdzie |
|---|---|
| Czarne „przekroje”: wewnętrzna strona siatki świata (widoczna przez wycięcie i dziury) rysowana na czarno jak lita skała — zasłania niższe lokacje i dziury w siatce | `Shaders::materialPipeline` (krajobraz bez odrzucania tylnych ścian), `shader/materials/main.frag` — `ARPG_BACKFACE` |
| Wnętrza: to, co leży głęboko pod stopami (3,5 m → 9 m), przechodzi w czerń — piwnice, dolina pod wieżą; płynne przejście przy wejściu/wyjściu | `shader/arpg.glsl` — `arpgFogOfWar`, `Camera::updateArpgView` |
| Prześwit tylko wtedy, gdy coś zasłania postać (3 promienie kamera → głowa/klatka/kolana, z pominięciem wyciętego sufitu); otwiera się w 0,2 s, zamyka w 0,4 s | `Camera::updateArpgView` |
| Prześwit: stały rozmiar w świecie (~2,3 m), czysty środek, miękka krawędź (szum gradientowy zamiast siatki 4×4), ściany ścięte do cokołu 50 cm | `shader/materials/main.frag` — `arpgHidden` |
| Liście drzew (materiały z kanałem alfa) przed postacią zawsze prześwitują | `arpgHidden` |
| Wnętrza: linia wzroku — 128 kierunków (promienie na wysokości kolan, głowy i pochylony w górę, ¼ kierunków na klatkę, wygładzane); to, czego postać nie widzi (za ścianami jaskini), w czerń | `Camera::updateArpgView`, `shader/arpg.glsl` — `arpgOutOfSight` |
| Przekroje zostają czyste czarne także po mgle (bit 4 w alfie G-bufora, przebieg mgły czyta G-bufor) | `main.frag`, `shader/sky/fog.frag`, `fog_epipolar.frag`, `Renderer::drawFog` |
| Postacie i potwory (siatki animowane) jaśniejsze o 40% w widoku ARPG | `main.frag` — `mainGBuffer` |
| Minimapa (Tab): mapa świata z przedmiotu-mapy w skryptach (odczyt kodu bajtowego Doc_SetPage/Doc_SetLevel/Doc_SetLevelCoords), obrócona jak kamera, kółko myszy nad mapą = przybliżenie (20–100 m), nieodkryte pola 4 m ciemne, wrogowie czerwoni, strzałka postaci, „N”; ekwipunek pod I (Backspace dalej działa) | `MainWindow::paintArpgMinimap`, `GameScript::arpgWorldMaps` |

### Poprawki sterowania (faza 3)
- Oba kierunki wciśnięte (np. zgubione puszczenie S w menu) → wygrywa ostatnio wciśnięty; czyszczenie klawiszy przy otwarciu/zamknięciu menu i utracie fokusu okna
- Łup przy zwłokach: przedmiot ma pierwszeństwo przed martwym potworem pod kursorem

## Faza 4 — walka hack&slash

| Element | Gdzie |
|---|---|
| Spacja = unik (krok w bok / odskok), także w trakcie bloku — po uniku blok wraca, jeśli środkowy przycisk wciąż wciśnięty | `PlayerControl::onKeyPressed`, `arpgStartDodge` |
| Lewy Shift = dobycie / schowanie broni (ostatnio używanej) | `PlayerControl::onKeyPressed` |
| Środkowy przycisk z bronią białą / pięściami = blok trzymany: postać zwraca się do wroga, animacja parady zatrzymana w oknie obrony, puszczenie kończy blok; w trybie ARPG da się parować także potwory; bez broni: przeciąganie = obrót kamery, klik = dobycie broni | `implMove`, `Pose::holdParade`, `Npc::takeDamage` |
| PPM = atak ładowany: przytrzymanie — zamach z boku zatrzymany w zamachu, pasek nad głową ładuje się 2 s; puszczenie w ciągu 0,2 s po pełnym naładowaniu (złoty pasek) = jeden zamach 180° z obrażeniami ×3, przewraca wrogów, jasna smuga cięcia; puszczenie za wcześnie albo za późno = zwykły cios; odnowienie 5 s po udanym (niebieski pasek pod stopami); unik i blok przerywają ładowanie | `implMove`, `Pose::holdAttack`, `Npc::commitDamageArpg`, `MainWindow::paintArpgCombat` |
| Zatrzymana parada / zamach nie przesuwają postaci (bez ruchu z animacji) | `Pose::animMoveSpeed` |
| X = zmiana broni biała ↔ łuk; skradanie przeniesione z X na V (tylko w trybie ARPG) | `PlayerControl::arpgSwapWeapon` |
| Ctrl + LPM: na postaci = atak na każdego, w puste miejsce = atak w miejscu w stronę kursora (przytrzymanie = ciągłe ciosy za kursorem) | `arpgMouseDown`, `arpgClickTick` (`Strike`) |
| Przy każdym ciosie postać od razu zwraca się do celu (jak Diablo), w zasięgu obraca się szybciej | `PlayerControl::arpgSwing` |
| Cios bronią białą trafia wszystkich wrogów w łuku 120° przed postacią (cel ataku zawsze, inni tylko wrodzy) | `Npc::commitDamageArpg` |
| Ataki bohatera o 20% szybsze (animacje ataku przesuwane do przodu, zdarzenia trafienia i dźwięki nie giną) | `Npc::tick`, `Pose::warpAttack` |
| Liczby obrażeń nad trafionymi (białe = zadane, czerwone = otrzymane, „Blok”), paski życia nad wrogami w walce | `World::arpgPopup`, `MainWindow::paintArpgCombat` |
| Dojście do celu bez pętli: prosta linia ↔ waynet z histerezą (2 sprawdzenia z rzędu), pierwszy waypoint za plecami pomijany, ścieżka skracana do najdalszego widocznego punktu | `arpgNextPoint`, `arpgPlanPath`, `arpgPullPath` |
