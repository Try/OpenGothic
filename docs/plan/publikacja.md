# Publikacja — kroki przed wydaniem publicznym (plan, 2026-10-16)

Cel: Diathic dla szerszej publiczności. Model: mod wymagający własnego Gothic II Złota Edycja (Steam albo GOG, z Nocą Kruka); paczka zawiera tylko nasz program i własne pliki; niekomercyjnie. Kolejność: publiczna beta (GitHub, ModDB, fora) → 1.0 → Steam za zgodą THQ Nordic. Wzór: Kroniki Myrtany: Archolos — darmowa osobna pozycja na Steamie, wymaga Gothic II Gold, z dopiskiem o zgodzie THQ Nordic (2021). Steam Workshop odpada (decyzje.md, 2026-10-14).

Tło: Gothic 1 Remake wyszedł 5.06.2026 (ponad 500 tys. sztuk w tydzień). Gothic II na Steamie: PL, DE, EN, RU z dubbingiem, FR / IT / ES z napisami; GOG ma też wersję polską. Dialogi i głosy biorą się z gry gracza — tłumaczymy tylko napisy Diathic.

## Decyzje otwarte
1. Język pierwszej publicznej wersji: tylko PL albo od razu PL + DE + EN (zalecane PL + DE + EN; RU później)
2. Launcher: program exe w C++ (zalecane) / podpisany skrypt PowerShell / aktualizacja w samej grze
3. Fazy 7 (loot) i 8 (balans): przed betą czy po
4. Pad i Steam Deck: po 1.0 (dziś brak obsługi pada)

## Etap A — od razu (bez wpływu na grę)
- [ ] A1. Prawa do plików w assets/diathic (logo.png, menu.ogg, menu_bg.jpg): źródło i licencja każdego, spis w assets/diathic/AUTORZY.txt; niepewne → zastąpić własnymi
- [ ] A2. Licencje: LICENSE (MIT, OpenGothic) zostaje, dopisać własną linię copyright; w paczce folder licencje/ z licencjami bibliotek z lib/ (Tempest, ZenKit, bullet3, dmusic, miniz, rapidjson, TinySoundFont, edd-dbg)
- [ ] A3. README.md od nowa (dziś opis OpenGothic): Diathic po angielsku i polsku — co to jest, wymagania, instalacja, tryb klasyczny; podziękowania dla OpenGothic; zastrzeżenie: „Gothic” to znak towarowy THQ Nordic GmbH, projekt nie jest z nimi powiązany i wymaga oryginalnej gry
- [ ] A4. Kontrola paczki w CI: build.yml przerywa wydanie, gdy zip zawiera pliki gry (*.vdf, *.mod, *.zen, skompilowane skrypty .dat)
- [ ] A5. Numery wersji: tag v0.X.Y = wersja publiczna (dziś semver = 1.0.N, job setup w build.yml); w menu „Diathic 0.X.Y (build N)”; buildy bez tagu bez zmian
- [ ] A6. Kontakt z autorem OpenGothic (Try): informacja o projekcie; ogólne poprawki silnika jako PR do OpenGothic

## Etap B — gra gotowa do bety
- [ ] B1. Całą grę da się przejść w trybie ARPG: rozdziały 1–6 i Jarkendar (faza 10; bot testowy albo testerzy)
- [ ] B2. Zero znanych błędów blokujących (zawieszenie, utknięcie w fabule, utrata zapisu); w GitHub Issues etykieta „blokujący”
- [ ] B3. Tryb klasyczny = OpenGothic bez zmian (kontrolne przejście rozdziału 1)
- [ ] B4. Zapisy: każdy wpis ARPG z numerem wersji (ArpgProgress v2, ArpgMapData v1 — już jest; nowe wpisy, np. loot z fazy 7, tak samo); test: zapis z poprzedniej wersji wczytuje się w nowej
- [ ] B5. Wymagania sprzętowe: test na słabszym PC (grafika zintegrowana, 1080p) i na 4K; minimalne i zalecane w README (dziś: Windows 10/11 64-bit, karta z Vulkan)
- [ ] B6. Fazy 7–8 — według decyzji 3

## Etap C — języki
- [ ] C1. Tabela tekstów: wszystkie napisy ARPG (~290 literałów z polskimi literami + napisy bez ogonków, 123 wywołania gtxt) w jednym miejscu, np. common/ui/arpgtexts.h / .cpp: identyfikator → PL / DE / EN (/ RU); w kodzie tylko identyfikatory
- [ ] C2. Język gry: `polishGame()` w arpgmenu.cpp → `gameLanguage()` (PL / DE / EN / RU / inny) po NAME_VALUE ze skryptów; wartości DE / EN / RU odczytać z wersji Steam (Właściwości → Język, potem powrót na polski); ręczne nadpisanie w Gothic.ini, sekcja [DIATHIC]
- [ ] C3. Kodowanie według języka gry: Windows-1250 (PL), 1252 (DE, EN, FR, IT, ES), 1251 (RU); gtxt zamienia UTF-8 na kodowanie gry; brak tłumaczenia → angielski
- [ ] C4. Tłumaczenia DE i EN (RU później): drzewka i węzły, opisy, komunikaty walki, strona „Sterowanie”, menu Diathic, ekrany ładowania; sprawdzenie przez osobę znającą język
- [ ] C5. Test na grze DE i EN: menu, księga, drzewka, karta postaci, minimapa, dziennik — czy napisy mieszczą się w polach (niemieckie są dłuższe)
- [ ] C6. Launcher i teksty paczki (CZYTAJ.txt → README.txt) po polsku, niemiecku i angielsku, według języka systemu

## Etap D — launcher, podpis, paczka
- [ ] D1. Launcher według decyzji 2. Zalecane: Diathic.exe w C++ (WinAPI) w launcher/, budowany w CI, z funkcjami launcher.ps1: lista wydań z API GitHuba, pobranie z paskiem postępu, SHA-256, rozpakowanie (miniz z lib/), szukanie Gothic II jak dziś (Steam libraryfolders.vdf, GOG, rejestr), wybór wersji, start gry
- [ ] D2. Kanał „stabilny”: workflow wydanie.yml (wzór: testerzy.yml) robi z wybranego tester-N wydanie v0.X.Y (Latest, opis PL / EN, trzymane na stałe); publiczny launcher bierze tylko te wydania; kanały build i tester zostają
- [ ] D3. Program gry Diathic.exe zamiast Gothic2Notr.exe (OUTPUT_NAME w CMakeLists.txt; launcher, dev/graj_lokalnie.bat, build.yml) — mniej pomyłek z oryginalnym Gothic2.exe
- [ ] D4. Podpis kodu: wniosek do SignPath Foundation (darmowy podpis dla open source; wymaga automatycznego buildu w CI z repozytorium); w build.yml podpis programu gry i launchera przed spakowaniem; strona z zasadami podpisu w repozytorium, jeśli SignPath jej wymaga
- [ ] D5. Antywirusy: każde wydanie przez VirusTotal; fałszywe alarmy zgłaszać do Microsoft (Defender) i producentów
- [ ] D6. Pierwsze uruchomienie bez Gothic II: komunikat PL / DE / EN, czego potrzeba (Gothic II Złota Edycja ze Steam albo GOG) i jak wskazać folder
- [ ] D7. Instalator (np. Inno Setup) dopiero przy 1.0; w becie zip

## Etap E — zgłaszanie błędów
- [ ] E1. .github/ISSUE_TEMPLATE: formularz błędu PL / EN (wersja Diathic, język gry, Steam / GOG, opis, kroki, log.txt, crash.log, zapis gry)
- [ ] E2. W launcherze „Zgłoś błąd”: otwiera folder gra\dane i stronę nowego zgłoszenia
- [ ] E3. Logi bez danych osobowych: ścieżki z nazwą użytkownika Windows zamieniane na %USERPROFILE% (sprawdzić log.txt i crash.log)

## Etap F — materiały i ogłoszenie bety
- [ ] F1. Zrzuty ekranu (6–8: walka, drzewka, mapa, dialogi, menu) i film 1–2 min (YouTube)
- [ ] F2. Opis krótki i długi PL / EN (DE), FAQ, znane problemy, wymagania
- [ ] F3. Strona projektu na GitHub Pages (osobna gałąź gh-pages, bez mieszania z docs/plan)
- [ ] F4. ModDB: strona moda w Gothic II: Night of the Raven, plik wydania v0.X.Y
- [ ] F5. Ogłoszenie tego samego dnia: forum WorldOfPlayers (moddowanie Gothica, DE / EN), polskie fora Gothica, r/worldofgothic, Discord OpenGothic
- [ ] F6. Pierwszy tydzień po wydaniu: codziennie przegląd zgłoszeń, poprawki jako kolejne v0.X.Y

## Etap G — po 1.0
- [ ] G1. Kontakt z THQ Nordic: zgoda na darmową pozycję na Steamie (jak Archolos)
- [ ] G2. Steam: konto Steamworks, opłata Steam Direct, strona sklepu z wymogiem posiadania Gothic II: Gold Edition
- [ ] G3. Linux: ALL_PLATFORMS = true dla wydań, launcher na Linuksa; Steam Deck według decyzji 4 (pad albo Steam Input)
- [ ] G4. Nexus Mods; tłumaczenie RU

## Gotowe do publicznej bety, gdy
Decyzje 1–3 podjęte, etapy A–F zamknięte (bez D7), ostatnie wydanie tester-N przez tydzień bez błędów blokujących.

Źródła: [Steam — Gothic II: Gold Edition](https://store.steampowered.com/app/39510/), [Steam — Archolos](https://store.steampowered.com/app/1467450/), [GOG DB — Gothic 2](https://www.gogdb.org/product/1207658718/releasenotes), [SignPath — open source](https://about.signpath.io/product/open-source), [Gameswelt — Gothic Remake](https://www.gameswelt.de/gothic-remake/news/thq-nordic-vermeldet-halbe-million-verkaufte-einheiten-333975)
