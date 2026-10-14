# gothic-diablo

Fork [OpenGothic](https://github.com/Try/OpenGothic): Gothic II Noc Kruka jako izometryczny action RPG
(Diablo / Path of Exile / Grim Dawn) na oryginalnych plikach gry.

## Uruchomienie (Windows)

- Launcher (buildy z CI, release `build-<N>`): [`launcher/README.md`](launcher/README.md).
- Lokalny build (~1 min od commita do gry): [`dev/README.md`](dev/README.md).
- Ręcznie: `Gothic2Notr.exe -g "F:\SteamLibrary\steamapps\common\Gothic II"`; `-classic` wyłącza tryb ARPG
  (oryginalna kamera i sterowanie OpenGothic).

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
| Prześwit tylko wtedy, gdy coś zasłania postać (3 promienie kamera → głowa/klatka/kolana i te same z powrotem, postać → kamera: promienie widzą tylko zewnętrzną stronę ścian; z pominięciem wyciętego sufitu); otwiera się w 0,2 s, zamyka w 0,4 s | `Camera::updateArpgView` |
| Kamera w skale (na zewnątrz: za ścianą wąwozu, w zboczu góry): pierwsza ściana na drodze do postaci widziana od środka → wewnętrzna strona siatki świata bliżej niż postać nie jest rysowana (inaczej czarny ekran), trzyma 0,4 s; we wnętrzach robi to wycięcie sufitu | `Camera::updateArpgView` (`DynamicWorld::rayBothSides`), `main.frag` (`arpgPlayer.w` = 1 + odległość) |
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
| Zasięg ciosu w widoku ARPG mierzony płasko (zasięg Gothica: podstawy obu gildii + broń), do 1,2 m w górę / w dół się nie liczy (stoki, stopnie, wilk niżej od Bezimiennego); atak ładowany +20% zasięgu; podchodzi na 90% zasięgu. Pojedynczy klik na wroga = cios od razu: cios zaczęty w biegu zostaje (cios z biegu Gothica, nie ucina go zatrzymanie), cios, który się nie zaczął, nie liczy się i idzie jeszcze raz | `Npc::arpgInReach`, `commitDamageArpg`, `PlayerControl::arpgSwing` (`swingCheck`), `ArpgAttack` |
| Zatrzymana parada / zamach nie przesuwają postaci (bez ruchu z animacji) | `Pose::animMoveSpeed` |
| X = zmiana broni biała ↔ łuk; skradanie przeniesione z X na V (tylko w trybie ARPG); atak bez broni dobywa ostatnio używaną (łuk, jeśli są strzały) | `PlayerControl::arpgSwapWeapon`, `arpgDrawPreferred` |
| Łuk / kusza: LPM na wroga = strzał (podejście na zasięg do 25 m / promienia mgły i wolną linię strzału), przytrzymanie = seria, Ctrl + LPM w puste miejsce = strzał prosto w stronę kursora; brak strzał = komunikat i broń biała | `arpgShoot`, `arpgClickTick` |
| Łuk / kusza + trzymany Ctrl = napięty łuk, postać obraca się za kursorem (wróg pod kursorem albo punkt na ziemi); Ctrl + LPM strzela od razu; Ctrl w trybie ARPG nie działa jak klawisz akcji Gothica | `onKeyPressed`, `implMove` |
| Trafienie strzałą: szansa jak w Gothicu/OpenGothic (talent, z bliska prawie zawsze); chybienie pokazuje „Chybiono”; obrażenia po pancerzu ×0,5 (tylko bohater w trybie ARPG) | `Npc::shootBow`, `Npc::takeDamage`, `Bullet::arpgDamageMul` |
| Łuk / kusza + PPM = strzał ładowany: przytrzymanie = napięty łuk, pasek 2 s, okno 0,2 s → ×3 obrażeń zwykłego strzału, zawsze trafia, jasna smuga strzału; odnowienie 5 s | `implMove` (ładowanie), `Npc::shootBow` (`arpgShotMul`) |
| Łuk / kusza + środkowy przycisk = odepchnięcie: wrogowie do 3 m w łuku 120° przed postacią odlatują ok. 2 m, bez obrażeń (także przy lądowaniu), fala w świecie; odnowienie 5 s | `PlayerControl::arpgPush`, `Npc::arpgKnockBack`, `MoveAlgo::accessDamFly` (prędkość) |
| Odnowienia (atak ładowany, odepchnięcie) liczone w sekundach gry, nie w tickach świata — wczytanie zapisu nie wydłuża paska | `arpgSpecialCd`, `arpgPushCd` |
| Mikstury: 1 = zdrowie, 2 = mana (Gothic II: ItPo_Health_*, ItPo_Mana_*, bez ItPo_Perm_*); wybiera najmniejszą, która wystarczy na brakujące punkty, inaczej największą; klawisze 3–0 = sloty run i zwojów Gothica | `arpgPotion`, `arpgPickPotion`, `Npc::arpgUseItem` |
| Czary 3–0 (sloty run i zwojów Gothica, przypisanie jak w Gothicu: w ekwipunku zaznaczyć runę/zwój i nacisnąć klawisz): naciśnięcie = broń chowana od razu, runa wyjmowana animacją Gothica, obrót do kursora i rzut (wróg pod kursorem = cel, inaczej w stronę kursora); przytrzymanie = ładowanie czarów ładowanych, puszczenie = rzut; brak many = „Brak many”; LPM/PPM z runą w ręku wracają do broni | `arpgSpellKey`, `implMove` (`arpgSpellSlot`, `arpgSpellCasting`), `arpgCursorAim` |
| Fikołek pod Spacją (i lewym Alt z bronią) zamiast uniku: przewrót w stronę kursora (W/S/A/D wygrywają), animacja przewrotu z akrobatyki Gothica II (nakładka HUMANS_ACROBATIC na czas przewrotu, T_FALLDN_2_RUNL, ok. 1,6 m) odtwarzana 1,4× szybciej (ok. 0,7 s), przez pierwsze ~0,45 s brak obrażeń od broni, strzał i czarów (napis „Unik”); przerwa 0,25 s między fikołkami; później osobna umiejętność do nauczenia | `PlayerControl::arpgStartRoll`, `Npc::arpgSetInvulnerable`, `Npc::takeDamage` |
| Pomoc w celowaniu: żywy wróg łapie się kursorem z większej odległości (95 px przy 1080p, albo kursor na ziemi do 1,4 m od niego), wrogowie mają pierwszeństwo przed innymi postaciami | `arpgPick` (krok 3) |
| Kolejkowanie ciosów: klik w trakcie ciosu bronią białą obok wroga (nie trafiony kursorem) = kolejny cios w tego samego wroga w oknie combo, a nie odejście; wskaźnik combo większy, biała ramka = cios w kolejce, złote kwadraty = ciosy w serii | `arpgMouseDown`, `MainWindow::paintArpgCombo` |
| Trzymany LPM: po śmierci / powaleniu celu postać sama atakuje najbliższego widocznego wroga (do 4,5 m bronią białą, w zasięgu strzału łukiem; ci przed postacią mają pierwszeństwo) | `arpgClickTick` (`Attack`), `arpgNextEnemy` |
| Mikstury pod 1–2 działają od razu (skrypt mikstury + dźwięk DRINKBOTTLE z gry), broń zostaje w ręku | `Npc::arpgDrinkNow` |
| Koszt many na pasku przy runach i zwojach (stałe skryptów SPL_COST_<czar>, zwoje SPL_COST_SCROLL, czary ładowane STEP_<czar> za krok); za mało many = przyciemnione pole i czerwona liczba | `GameScript::arpgSpellCost`, `paintArpgBarOverlay` |
| Wróg pod kursorem i atakowany cel lekko świecą (jaśniejsi o 30% + ciepła poświata na krawędziach); pasek odnowienia pod stopami usunięty (jest na pasku umiejętności) | `scene.arpgHi` (UboGlobal), `main.frag`, `Camera::arpgSetHighlight`, `tickFocus` |
| Pasek umiejętności na dole: LPM (broń w ręku), PPM (broń z czerwoną poświatą = umiejętność specjalna, odnowienie ataku ładowanego), SPM (blok = tarcza z herbem Khorinis z tekstury OBJ_CITY_DECO_SHIELD_01, wycięta po obrysie; z łukiem = symbol runy Pięści Wiatru z ITARRUNE1, odnowienie odepchnięcia), 1–2 mikstury (model mikstury, która się wypije, liczba sztuk), 3–0 runy i zwoje z Gothica; modele przedmiotów jak w ekwipunku | `MainWindow::paintArpgBar`, `paintArpgBarOverlay`, `Renderer::draw` (`hudItems`) |
| Ctrl + LPM: na postaci = atak na każdego, w puste miejsce = atak w miejscu w stronę kursora (przytrzymanie = ciągłe ciosy za kursorem) | `arpgMouseDown`, `arpgClickTick` (`Strike`) |
| Przy każdym ciosie postać od razu zwraca się do celu (jak Diablo), w zasięgu obraca się szybciej | `PlayerControl::arpgSwing` |
| Cios bronią białą trafia wszystkich wrogów w łuku 120° przed postacią (cel ataku zawsze, inni tylko wrodzy) | `Npc::commitDamageArpg` |
| Ataki bohatera o 20% szybsze (animacje ataku przesuwane do przodu, zdarzenia trafienia i dźwięki nie giną) | `Npc::tick`, `Pose::warpAttack` |
| Liczby obrażeń nad trafionymi (białe = zadane, czerwone = otrzymane, „Blok”), paski życia nad wrogami w walce | `World::arpgPopup`, `MainWindow::paintArpgCombat` |
| Dojście do celu bez pętli: prosta linia ↔ waynet z histerezą (2 sprawdzenia z rzędu), pierwszy waypoint za plecami pomijany, ścieżka skracana do najdalszego widocznego punktu | `arpgNextPoint`, `arpgPlanPath`, `arpgPullPath` |

## Faza 5 — interfejs: ekwipunek, postać, drzewka umiejętności, rozmowy

| Co | Gdzie w kodzie |
|---|---|
| Okno ARPG pod I (ekwipunek), C (postać), T (drzewka); ten sam klawisz albo Esc zamyka, klik w zakładkę przełącza; Backspace dalej otwiera ekwipunek Gothica (handel i skrzynie też zostają po staremu) | `ArpgMenu`, `MainWindow::keyDownEvent` / `keyUpEvent` |
| Bohater w środku ekranu: kamera ARPG po otwarciu okna objeżdża go od przodu, zniża się i podjeżdża (8°, 2,9 m, cel na wysokości 1 m), więc widać prawdziwego Bezimiennego w animacji spoczynku i od razu każdą zmianę pancerza i broni; Q/E obracają kamerę wokół niego; po zamknięciu kamera wraca na swój kąt | `Camera::setArpgPortrait`, `Camera::tickArpg` |
| Wygląd otwartej księgi z gry: strony BOOK_BROWN_L / BOOK_BROWN_R (przy ekwipunku i postaci po bokach, bohater między nimi; przy drzewkach jedna rozkładówka na środku), ciemny atrament: czcionki książkowe FONT_10_BOOK / FONT_20_BOOK, etykiety i wartości w starej czcionce Gothica przyciemnionej na brąz i czerwień; zakładki, opis przedmiotu i opis węzła na kartkach LETTERS; sloty w ramkach INV_SLOT_FOCUS (założone: INV_SLOT_EQUIPPED_FOCUS z czerwonym brzegiem), najechany slot: rozjaśnienie i cienka czerwona linia tuż przy nim; paski BAR_*; modele przedmiotów jak w ekwipunku. Okno rośnie z wysokością ekranu (projekt na 1080 linii) | `ArpgMenu::drawPage`, `drawSlot`, `drawBar`, `Fonts`, `GthFont::setColor` |
| Ekwipunek: po lewej to, co bohater nosi (broń biała, dystansowa, pancerz, amulet, 2 pierścienie, pas) z nazwami, paski życia i many, runy i zwoje pod klawiszami 3–0; po prawej plecak 6 kolumn z rodzajami (Wszystko, Broń, Pancerze, Magia, Mikstury, Jedzenie, Dokumenty, Inne), liczby sztuk, kółko myszy przewija, złoto; opis najechanego przedmiotu jak w Gothicu (teksty i wartości ze skryptów + model) | `ArpgMenu::paintEquip`, `paintInfo`, `Inventory::equippedIn` |
| LPM na przedmiocie = jak w ekwipunku Gothica (załóż / wypij / przeczytaj), na założonym = zdejmij; najechana runa lub zwój + klawisz 3–0 = przypisanie do klawisza. Przeciąganie (LPM przytrzymany i ruch o 6 px): przedmiot leci za kursorem (model z gry), miejsce, z którego wyszedł, ciemnieje, cel ma zieloną (pasuje) albo czerwoną (nie pasuje) ramkę i podpis pod kursorem; upuszczenie na slot ekwipunku = załóż (broń biała, dystansowa, pancerz, amulet, pierścień na którymkolwiek z dwóch, pas), na klawisz 3–0 = runa lub zwój pod klawisz, na plecak = zdejmij, poza księgą = wyrzuć na ziemię cały stos (założone najpierw schodzi; bohater robi animację upuszczenia). PPM = wyrzuć jeden (Shift: cały stos), na założonym = zdejmij | `ArpgMenu::mouseDown`, `mouseUp`, `dropOn`, `paintDrag`, `spellKey` |
| Handel, skrzynie i przeszukiwanie ciał w księdze: lewa strona to czerwona księga drugiej strony (imię handlarza / nazwa skrzyni / imię przeszukiwanego, pod spodem czerwony podpis „Handlarz – od niego kupujesz”, „Skrzynia – bierzesz z niej”, „Przeszukujesz – bierzesz”, złoto handlarza albo przycisk „Zabierz wszystko”), prawa to brązowa księga Bezimiennego („Twój plecak – sprzedajesz” / „odkładasz do skrzyni”, nasze złoto); na obu rodzaje przedmiotów, siatka z cenami (złote liczby: ile kosztuje u handlarza / ile dostaniesz), kartka z opisem i ceną najechanego przedmiotu. LPM = jedna sztuka (ze skrzyni i z ciała cały stos), Shift+LPM albo PPM = cały stos, przeciągnięcie na drugą stronę = cały stos; założonych nie da się sprzedać ani odłożyć; złoto w handlu tylko w nagłówkach. Stan trzyma menu Gothica (rozmowa czeka na koniec handlu), Esc / I kończą; pasy rozmowy znikają; wytrych do zamków po staremu | `ArpgMenu::paintTrade`, `tradeMove`, `tradeDrop`, `InventoryMenu::isArpgBook`, `arpgMove` |
| Kamera księgi: w szczelinie między stronami handlarz (z przodu, nisko, jak portret Bezimiennego), skrzynia albo ciało (z góry); kamera stoi po stronie Bezimiennego, obrócona od linii między nimi (42° / 65°) tak, że Bezimienny jest za kartką; strona z większym miejscem, ściany przyciągają kamerę; płynne wejście i wyjście 0,4 s; przy handlu z rozmowy światło twarzy i głębia ostrości idą za nią. Modele przedmiotów w księdze handlu rysuje warstwa paska umiejętności (wcześniej menu Gothica, puste) | `Camera::tickArpgBook`, `setArpgBook`, `Renderer::draw` |
| Postać: poziom, doświadczenie (pasek), punkty nauki, punkty umiejętności (0, później od nauczycieli w zamian za punkty nauki), atrybuty, ochrona; umiejętności Gothica z nazwami i poziomami ze skryptów (TXT_TALENTS, TXT_TALENTS_SKILLS, procenty broni); walka ARPG (obrażenia wręcz i strzałą, szybkość ataku, atak ładowany, fikołek) | `ArpgMenu::paintStats` |
| Drzewka (na razie same opisy) jako graf: korzeń (sama umiejętność) — pień — 5 gałęzi po 2–3 węzły — węzeł kluczowy połączony czerwoną linią z gałęziami, których wymaga; węzły to ramki slotów z modelami przedmiotów z gry (np. Szeroki łuk = kosa, Skupienie = kamień ogniskujący, Wir = runa Trąba powietrzna); pod węzłem 0/max i nazwa; po lewej wybór drzewka (Cios, Atak ładowany, Fikołek), punkty umiejętności i opis najechanego węzła | `ArpgMenu::paintTrees`, `skillTrees()`, `ArpgMenu::iconItem` |
| Polskie litery w oknie: tekst UTF-8 zamieniany na Windows-1250, gdy gra jest polska (sprawdzane po NAME_VALUE = „Wartość:” w skryptach), inaczej litery bez ogonków | `ArpgMenu::gtxt` |
| Rozmowy kinowe (etap 1): rozmowa dwóch postaci w trybie ARPG — kamera ARPG przez ~0,45 s zjeżdża w stronę głów (przekroje i mgła wojny jeszcze działają), potem cięcie do ujęć „reżysera”; po rozmowie cięcie z powrotem do widoku z góry. Ujęcia z węższym obiektywem (50° zamiast 67,5°). Warsztaty i inne menu samego bohatera zostają w widoku z góry | `Camera::tickArpgDialog`, `Camera::setArpgDialog`, `MainWindow::tickCamera` |
| Reżyser ujęć: rozmowa otwiera się ujęciem miejsca z daleka i z góry (co najmniej 1,6 s, kamera lekko się podnosi), potem zza ramienia słuchającego na mówiącego, plan średni, zbliżenie twarzy, dwuujęcie; cięcie przy nowej kwestii, ale nie wcześniej niż po 1,3 s, jeśli twarz mówiącego już widać; szybka wymiana krótkich zdań (obie poniżej 2 s) zostaje w jednym ujęciu obu, bez cięć tam i z powrotem; ten sam mówiący dalej = inna wielkość planu dopiero po 3,5 s ujęcia; przy kwestii dłuższej niż 6 s w połowie reakcja słuchającego (1,4–2,2 s) i powrót; przy wyborze odpowiedzi zza ramienia bohatera na rozmówcę; każde ujęcie rusza się po swojemu (najazd do 10%, przejazd w bok do 30 cm, łuk do 6°, podniesienie do 14 cm), zawsze od linii między postaciami; kamera po jednej stronie tej linii (strona widoku z góry, chyba że tam ściany); przed ścianą kamera przysuwa się do postaci, a mocno zasłonięte ujęcie zamienia na inne; słuchający nie zasłania twarzy mówiącego (głowa i barki 58 cm od linii kamera–twarz: naramienniki, łuk na plecach — sprawdzane przy cięciu, ujęcie nie skacze); głowy wygładzane (gesty) | `ArpgDialogCam` |
| Ustawienie postaci: w chwili cięcia (cięcie to ukrywa) Bezimienny odwraca się do rozmówcy i staje w odległości rozmowy 1,3–1,7 m (ruch z kolizjami; nie, gdy siedzi albo czegoś używa) | `Camera::arpgPlaceHero` |
| Wyjście z rozmowy: kamera w 0,8 s wraca z ostatniego ujęcia do widoku z góry (obiektyw płynnie z 50° do 67,5°) | `Camera::tickArpg` (exitOn) |
| Światło na twarze: w ujęciach rozmowy miękkie, ciepłe światło nad kamerą i trochę przed nią, zasięg do twarzy + 1,8 m (noc, wnętrza); znika przy zmianie świata przed jego zniknięciem | `MainWindow::updateArpgDialogLight`, `Camera::arpgDialogLight`, `LightGroup::Light::release` |
| Rozmyte tło (głębia ostrości): w ujęciach rozmowy ostro wokół twarzy w ostrości (35 cm + 15% odległości), dalej coraz mocniej rozmyte do 9 px (przy 1080 liniach), bliżej (bark słuchającego) też; dysk 24 próbek, ostre twarze nie rozlewają się w tło; pojawia się w ~0,3 s, znika w ~0,25 s | `shader/arpg_dof.frag`, `Renderer::drawArpgDof`, `Camera::arpgDof` |
| Aktorstwo w rozmowie: obaj patrzą sobie w oczy (głowa podąża za głową rozmówcy, do 80° w bok; na trzecią osobę, gdy ona mówi); mruganie co 2,5–5,5 s — krótkie: R_EYESBLINK to jedna klatka (zamknięte oczy, w pliku 1 s na klatkę), więc grana jako 140 ms: 45 ms zamykania, chwila zamknięte, 75 ms otwierania; koniec nieruchomej miny jest liczony co klatkę (wcześniej twarz zostawała z zamkniętymi oczami, aż ruszyło się coś innego, np. usta); mina rozmówcy z jego stosunku do bohatera, delikatnie (siła 0,45: przyjazny S_FRIENDLY, zły S_ANGRY, wrogi S_HOSTILE, neutralny bez miny; odświeżana co 2 s, zatrzymywana po rozmowie); słuchający czasem potakuje (T_YES) w 40–65% kwestii dłuższej niż 3,5 s, z szansą 2/3, nie częściej niż co 6 s, tylko stojąc i bez broni w ręku; po rozmowie głowa prosto | `DialogMenu::tickActing`, `Npc::arpgLookAtHead`, `Npc::arpgNod`, `VisualObjects::startMMAnim` (czas −2 = raz: jedna klatka = mrugnięcie, `fadeIn` / `fadeOut`), `preFrameUpdateMorph` |
| Trzecia osoba w rozmowie: kwestia ze skryptu (AI_Output, nie okrzyk) postaci spoza rozmowy, do 15 m od bohatera — dotąd tylko dźwięk — wchodzi do filmu: napis z jej imieniem w dolnym pasie, kamera tnie na nią (zza ramienia bohatera, plan średni, zbliżenie, po tej samej stronie świata co główna linia), wszyscy na nią patrzą, odpowiedzi czekają, aż skończy | `DialogMenu::arpgThirdLine`, `ArpgDialogCam::pair`, `Gothic::onArpgSpeech` |
| Zaczepki w przelocie: każda kwestia powiedziana poza rozmową (okrzyki, zaczepki, pogawędki postaci, uwagi bohatera) do 25 m od bohatera = napis nad głową na ciemnym tle przez czas nagrania (+0,6 s), pojawia się w 0,15 s, znika w 0,3 s; najwyżej 4 naraz, jedna na postać; okrzyki ludzi wokół nie wchodzą do filmu rozmowy | `MainWindow::onArpgSpeech`, `paintArpgRemarks`, `GameScript::aiOutput` |
| Wygląd kinowy: czarne pasy 2,35:1 (wjeżdżają w 0,3 s), napis w dolnym pasie z imieniem mówiącego na złoto (Bezimienny / imię postaci), odpowiedzi jako lista w dolnym pasie (pas rośnie do wysokości listy, miękka krawędź), numery 1–0, wybrana złota z kreską, dopiski „(handel)”, „(nauka)”, już zadane stałe pytania wyszarzone (KONIEC, handel i nauka nie); nad obrazem nic więcej: bez nazwy celu i pasków życia / many | `DialogMenu::paintArpg`, `GameScript::DlgChoice::known` |
| Sterowanie w rozmowie: mysz najeżdża na odpowiedź, LPM wybiera (dopiero gdy lista jest cała, żeby klik pomijający kwestię nie wybrał odpowiedzi), LPM poza listą / PPM / Spacja = następna kwestia, 1–9 i 0 = odpowiedź o tym numerze, Enter jak w Gothicu; kursor myszy widoczny | `DialogMenu::mouseDownEvent`, `mouseMoveEvent`, `keyDownEvent` |
