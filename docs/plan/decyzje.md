# Diathic — decyzje użytkownika i historia faz

Archiwum z dokumentów projektu claude.ai (02-plan, 03-faza6, 04-faza9), przeniesione 2026-10-16. Działanie funkcji opisuje GOTHIC-DIABLO.md, wiedzę o silniku jego sekcja „Ustalenia techniczne”. Aktualny stan i następne kroki: dokument projektu claude/02-plan.md.

## Ogólne
- 2026-10-11: baza = fork OpenGothic (C++, MIT) + oryginalne pliki Gothic II Noc Kruka ze Steam (wersja polska); standalone z assetami bez zgody THQ Nordic — nie (analiza: analiza-wykonalnosci.md)
- 2026-10-15: oficjalna nazwa **Diathic** (gothic-diablo = nazwa kodowa)
- Zasada raportowania: na koniec każdego buildu napisać, co następne
- 2026-10-13: lokalny build na PC (MSYS2 w G:\msys64, klon w G:\projekty\gothic-diablo\src, graj_lokalnie.bat, buduj_w_tle.bat; opis w dev/README.md); push na GitHuba zostaje, test lokalnym buildem bez czekania na CI
- 2026-10-15: CI buduje tylko Windows (fbac690); Linux, deb, Android, macOS wracają ze zmienną repo ALL_PLATFORMS = true; commity tylko z narzędziami / CI z `[skip ci]`
- W PowerShell znaki „ ” działają jak cudzysłów — w napisach w cudzysłowie podwójnym ich nie używać

## Wersja dla testerów (2026-10-14)
- Steam Workshop odpada (Workshop Gothic II kopiuje mody do folderu gry i uruchamia oryginalny silnik, a my mamy osobny program); testerzy dostają paczkę launchera; tylko wersje „testowe” — użytkownik sam wybiera build; zgłaszanie błędów z gry „nie teraz” (opis + log.txt ręcznie)
- Wymagania testera: Windows 10/11 64-bit, karta z Vulkan, własny Gothic II Gold (z Nocą Kruka; zalecana polska wersja); instalacja Gothic II nie jest zmieniana
- Później do rozważenia: samoaktualizacja launchera testerów, zgłaszanie błędów z gry

## Fazy 0–3 (2026-10-11 – 13)
- Faza 0: stałej kamery ARPG nie da się zrobić skryptami (CAMERA.DAT)
- Faza 1 (PR #1): kamera 55°, 11 m, bez kolizji, Q/E obrót, ruch względem ekranu, ukrywanie sufitów, mgła wojny, kursor; 2026-10-15: zoom kółkiem tylko ±10% (10–12 m)
- Faza 2 (PR #2): sterowanie myszą jak w Diablo, wskazywanie według zasad klawisza Z, omijanie przeszkód
- Faza 3 (PR #3): czarne przekroje, linia wzroku we wnętrzach, prześwit, jaśniejsze postacie, minimapa
- Pętla przy kliknięciu (krążenie wokół stołów; poprawka 3637 nie pomogła) → 2026-10-16 siatka ArpgNav; skok / przejście przez niską przeszkodę użytkownik dopuścił, na razie tylko obejście
- Znane problemy: wiszące dekoracje ucięte na wysokości cięcia, jaskinie wielopoziomowe, wydajność promieni we wnętrzach

## Faza 4 — walka (PR #4, 2026-10-14)
- 1 = mikstura zdrowia, 2 = mana, 3–0 = czary; Spacja = fikołek z klatkami nieczułości (później umiejętność do nauczenia); LPM = atak, PPM = atak ładowany (2 s, okno 0,2 s, ×3, 180°, 5 s), SPM = blok / odepchnięcie łukiem; cios trafia wszystkich w łuku; mikstury od razu jak w Diablo; łuk: celność jak w Gothicu, obrażenia ×0,5, Ctrl = celowanie za kursorem; pasek umiejętności z ikonami z gry; podświetlenie wroga; błysk przy trafieniu niepotrzebny
- 2026-10-16 („walka dziwna”): szybkość ataku bazowo jak w Gothicu (było +20%), Płynne ruchy +7% za punkt (do +35%); walka dalej do poprawy większą liczbą animacji
- 2026-10-16 parowanie: blok zaczęty najwyżej 150 ms przed ciosem wroga na wprost → bez obrażeń, wróg dostaje Zachwianie; dźwięk: dzwoneczek z plików gry albo głuche uderzenie (teraz szczęk metalu z iskrami + dzwonek, jeśli jest)
- Zachwianie = pierwszy z wielu statusów negatywnych (potwory, NPC, bohater): przerwanie ciosu, brak ataku 250 ms, obrażenia ×4, niebieskie gwiazdki; 250 ms to mniej niż czas ciosu bohatera — sprawdzić w grze; Podpalenie i Krwawienie to też statusy

## Faza 5 — interfejs i dialogi (PR #5, 2026-10-14)
- Ekwipunek ze slotami jak w RPG; panel postaci; drzewka jak w Last Epoch (każda umiejętność ma własne); na start 3 drzewka: zwykły atak, atak ładowany, fikołek
- Interfejs z jak największą liczbą tekstur i modeli z gry; okno postaci i ekwipunek pokazują Bezimiennego animowanego w spoczynku z założonym ekwipunkiem
- Dialogi kinowe jak w Mass Effect; ryzyka: ciasne wnętrza, postacie przy ścianie, skrypty z odejściem postaci w trakcie rozmowy, sceny wieloosobowe (Lord Hagen), dużo napisów nad głowami w walce
- Do sprawdzenia w dużej rundzie testowej: trzecia osoba w rozmowie, kamera księgi, kamera w skale, walka po poprawkach

## Faza 6 — rozwój postaci (PR #6, 2026-10-14)
- Źródła punktów: poziom, nauczyciele (za punkty nauki), rzadkie znaleziska; bez punktów za używanie umiejętności; pula mieszana (z poziomu i od nauczycieli wolne, znaleziska do swojej gałęzi)
- Procenty broni (1H, 2H, łuk, kusza) w drzewku Biegłość; siła, zręczność, mana, skradanie, alchemia, kowalstwo, zamki itd. jak w Gothicu
- Bez nauczyciela do 30%, odblokowanie do maksimum nauczyciela za punkty nauki; u każdego nauczyciela 5 punktów nauki = 1 punkt umiejętności; kamienne tablice walki = punkty do gałęzi biegłości (siła, zręczność, życie, mana po staremu); reset drzewek za złoto, cena rośnie z poziomem; testy u Pomocnika („insert ch”)
- Wartości na start (do balansu): 1 punkt za poziom; odblokowanie 5 punktów nauki za 10% (30→60 = 15, 60→75 = 8, 75→90 = 8, 90→100 = 5); ranga biegłości +5% (Nowicjusz 4 rangi do 30%, Wojownik / Strzelec 6 do 60%, Mistrz 8 do 100%); tablica poziom 1/2/3 = 1/2/3 punkty; reset 100 złota × poziom; rangi razem: Cios 44, Atak ładowany 32, Fikołek 23, Biegłość 72
- 2026-10-15: bonus z broni (np. +3% noża) nie liczy się do limitu Biegłości
- Po testach: „balans jest ok, zostawmy to na dłuższą sesję”

## Fazy 7–8 (odłożone 2026-10-14)
- Faza 7 loot: rzadkość, afiksy na bazie przedmiotów z Gothic II, dropy z potworów
- Faza 8 świat i balans: gęstość potworów / spawnery, balans łuku względem broni białej, fikołek jako umiejętność do nauczenia (akrobatyka?), balans drzewek, kolejne statusy negatywne

## Faza 9 — interfejs: menu, kursor, paski, mapa (2026-10-14/15)
- Kule życia i many: NIE — „paski Gothica, tylko ładniejsze”
- Mapa: duża mapa + zapis odkrytego terenu; szkic Bezimiennego atramentem, kupiona mapa zamienia go na malowaną; handlarze i nauczyciele z dziennika na mapie (w aktualnym miejscu postaci); znaki z map misji + własne pinezki; obie formy: nakładka na grę (jak Diablo II) i pergamin na cały ekran
- Zamiast proponowanych dodatków: kursor = model miecza, czerwony nad wrogami; w menu nazwa Diathic i wersja gry; logo, muzyka i tło menu od użytkownika (assets/diathic)
- Launcher i paczka dla testerów pod nazwą Diathic
- 2026-10-15 wieczorem: menu „bardziej nowoczesne” z nowym tłem, muzyką, logo i assetami z gry; muzyka Nocy Kruka nie może nakładać się na naszą; skróty starego sterowania nakładały się na nowe (skradanie, chód / bieg) → nowe skróty i strona „Sterowanie”
- Test tester-3679: ekran ładowania, zapisu i dziennik w starym stylu → przerobić; „większy prześwit albo drzewa ukryte do 2× wzrostu bohatera” — wybrane oba; minimapa pokazywała potwory pod ziemią i na niższych piętrach

### Lista do testu (PR #10 i dalej)
- Menu: tylko nasza muzyka, tło z iskrami, wybór myszą i klawiaturą, menu w grze (Esc), podmenu (opcje, wczytywanie)
- Sterowanie: R chód / bieg, V skradanie (z talentem / komunikat bez), brak podwójnych działań klawiszy, strona „Sterowanie” (zmiana, zamiana, domyślne, Esc)
- Kursor: czubek przy myszy, rozmiar, czy nie „pływa”, czerwony nad wrogiem
- Paski: położenie, liczby, ślad ubytku, pulsowanie, doświadczenie (najechanie)
- Tab trzy razy: minimapa, linie na grze, nic; M: bez mapy krainy szkic odkrytego terenu, z mapą (np. od Brahima) malowana, zakładki map
- Zapis i wczytanie: odkryty teren i pinezki zostają; znaczniki nauczycieli i handlarzy po rozmowach
- PR #10: ekran ładowania (nowa gra, wczytanie, zmiana świata), zapis gry, opcje / zapis / wczytanie, dziennik (L), prześwit i korony drzew w lesie, minimapa przy jaskiniach i piętrach, ruch na klik przy meblach, parowanie i zachwianie, szybkość ataku

## Faza 10 — przejście gry (2026-10-15)
- Poprawki map pionowych (Górnicza Dolina, Jarkendar, wieże), jaskinie wielopoziomowe, kamera, ruch na klik
- Ręczny test zbyt czasochłonny → bot testowy, „zrobimy to później” (plan: bot-testowy.md)

## Publikacja (2026-10-16)
- Użytkownik: „dodajmy do planu dokładne kroki do zrobienia przed publikacją” → publikacja.md (etapy A–G, decyzje otwarte: język pierwszej wersji, launcher, fazy 7–8 przed betą, pad)
- Model: mod wymagający własnego Gothic II Gold (Steam / GOG), niekomercyjnie; publiczna beta (GitHub, ModDB, fora) → 1.0 → Steam za zgodą THQ Nordic

## Faza 11 — drzewka broni (2026-10-16)
- Użytkownik: „zróbmy brancha z osobnym drzewkiem i skillami do łuków, tarcz, broni 1H, 2H” — po burzy mózgów:
  - jedno drzewko na broń (umiejętności jako gałęzie, nie osobne drzewko na każdą umiejętność)
  - łuk i kusza we wspólnym drzewku
  - tarcza i blok bronią to samo: drzewko Blok działa dla bloku bronią i pięściami, do czasu dodania przedmiotu tarczy
  - Q / E / F dla umiejętności; obrót kamery przeniesiony na strzałki (przypisywanie umiejętności do PPM i 3–0 — możliwe później)
- Do testu: wszystkie umiejętności (Q / E z 1H, 2H, łukiem; F), przeniesione punkty łuku w starym zapisie, wygląd księgi z 8 drzewkami, pasek z Q / E / F, strona „Sterowanie”

## Faza 12 — grafika (2026-10-17)
- 2026-10-17: użytkownik wskazał poprawę jakości tekstur jako następne ulepszenie; pierwszy krok planu to audyt mipmap i próbnika tekstur świata.
- 2026-10-17: użytkownik dodał ocenę DLSS Super Resolution przez Streamline/Vulkan jako późniejszy, opcjonalny krok; nie zastępuje to poprawy samych tekstur.
- 2026-10-17: użytkownik chce panel „Grafika” w grze do zmiany i zapisu dostępnych opcji bez ręcznej edycji `Gothic.ini`; wdrożenie UI wymaga uzgodnienia z agentem rozgrywki.
- 2026-10-17: kierunek grafiki: mroczny Gothic — klimat Gothic II na pierwszym planie, subtelne akcenty Diablo.
- Użytkownik: drugi agent ulepsza tekstury i grafikę równolegle z pracą nad grą → gałąź faza12-grafika, zasady i podział plików w grafika.md
- Agent grafiki to ChatGPT (bez skilli, pamięci i dokumentów projektu) → grafika.md samowystarczalny, stan w jego sekcji „Stan”
