# Grafika — handoff dla agenta grafiki (2026-10-17)

Ten plik jest jedynym źródłem zasad dla agenta grafiki (ChatGPT): nie ma on dostępu do dokumentów projektu claude.ai, skilli ani pamięci Claude. Wszystko, czego potrzebuje, jest tutaj i w repozytorium. Agent rozgrywki (Claude) pracuje równolegle na swoich gałęziach.

## Projekt w skrócie
- **Diathic** (nazwa kodowa gothic-diablo): Gothic II Noc Kruka jako izometryczny action RPG (Diablo, Path of Exile, Grim Dawn) — ta sama fabuła, przedmioty, modele i mapy, jak najbliżej oryginału.
- Fork silnika [OpenGothic](https://github.com/Try/OpenGothic) (C++, Vulkan przez bibliotekę Tempest) + oryginalne pliki gry gracza (polska wersja). Repo: github.com/wr0b3l/gothic-diablo.
- Tryb ARPG (domyślny) i klasyczny (`-classic`, OpenGothic bez zmian).
- Gra u użytkownika (Windows): `graj_lokalnie.bat` (buduje commit z GitHuba lokalnie, ~1 min) albo launcher z buildami z CI. Log gry: `gra\dane\log.txt`.
- CI: push na master albo do PR → wydanie `build-<N>` (Windows).
- Stan (2026-10-17): PR #10 (gałąź faza9-ekrany) i PR #11 (faza11-drzewka-broni, baza faza9-ekrany) czekają na test użytkownika. Dlatego baza dla grafiki to `faza11-drzewka-broni` — ma wszystko.

## Twoje zadanie
Tekstury, oświetlenie, obraz (shadery, przebiegi po renderze), efekty, własne grafiki. Nie twoje: walka, drzewka umiejętności, interfejs, mapa, loot, klawisze.

Pierwszy krok: jedno pytanie do użytkownika, od czego zacząć — przed kodem:
- (a) obraz i światło w shaderach (kolor, kontrast, cienie, mgła, poświata ognia i magii, ostrość) — w repo i w paczce;
- (b) tekstury w wyższej rozdzielczości — tylko paczki, które gracz instaluje sam, albo narzędzie uruchamiane u gracza (patrz „Prawa”);
- (c) własne grafiki (np. ikony umiejętności Q / E / F, ramki) — to interfejs agenta rozgrywki, uzgodnić przez użytkownika.

## Gdzie co jest
- `GOTHIC-DIABLO.md` — mapa projektu: każda funkcja gry (co robi | gdzie w kodzie), sekcja „Ustalenia techniczne”, w niej **„Grafika i renderer”** (ładowanie tekstur, archiwa VDF, przebiegi obrazu, materiały świata). Czytaj ją najpierw.
- `docs/plan/decyzje.md` — decyzje użytkownika z datami; `docs/plan/publikacja.md` — plan wydania publicznego (prawa, paczka, wymagania sprzętowe).
- Kod: `common/` (gra i renderer, `common/graphics/`), `shader/` (GLSL, kompilowane glslang w buildzie), `assets/diathic/` (własne pliki wbudowane w program: `DiathicAsset::get("nazwa")`).

## Zasady pracy
- Kod szukaj tanio: `grep -n` w GOTHIC-DIABLO.md → `grep -rn` w kodzie → fragment ±40 linii. Całych plików powyżej 300 linii nie czytaj.
- Edycje punktowe, nie przepisywanie plików.
- Tylko tryb ARPG: nowe zachowanie za warunkiem trybu ARPG; `-classic` bez zmian.
- Kod w plikach `arpg*`; w plikach OpenGothic tylko punkty zaczepienia z `arpg` w nazwie.
- Liczby (siła efektu, progi) jako nazwane stałe w jednym miejscu (`common/graphics/arpggfx.h`).
- Teksty w grze po polsku przez `ArpgMenu::gtxt` (Windows-1250).
- Odpowiedzi do użytkownika krótkie i konkretne, przykłady tylko na prośbę; niejasna decyzja → jedno pytanie, nie zgadywanie.

## Podział plików z agentem rozgrywki
- Twoje: nowe `shader/arpg_*` (poza `arpg.glsl` i `arpg_dof.frag`), nowe `common/graphics/arpggfx.cpp` / `.h`, nowe pliki w `assets/diathic/`, sekcja „Faza 12 — grafika” w GOTHIC-DIABLO.md, sekcja „Faza 12” w docs/plan/decyzje.md, ten plik.
- Wspólne — tylko punkt zaczepienia (wywołanie), wymienione w opisie PR: `common/graphics/renderer.cpp`, `shaders.cpp` / `.h`, `shader/CMakeLists.txt`, `common/resources.cpp`, `common/mainwindow.cpp`, `shader/arpg.glsl` (mgła wojny), `shader/materials/main.frag` (`arpgHidden`: wycięcia sufitów, prześwit, drzewa, podświetlenie wroga — nie przerabiać).
- Nie ruszać: `common/game/*`, PlayerControl, Npc, ArpgMenu, ArpgSkills, ArpgStatus, tabela klawiszy ArpgKeys (potrzebny klawisz → zapytaj użytkownika; przełączniki przez Gothic.ini).

## Zasady grafiki
- Każdy efekt do wyłączenia w Gothic.ini, sekcja [DIATHIC] (klucze `gfx…`); domyślnie włączone tylko tanie. W opisie PR koszt efektu (cel: karta zintegrowana, 1080p).
- Styl: jak najbliżej oryginału Gothic II; czytelność z góry jak Diablo / Grim Dawn; bez przesadnej poświaty i „plastiku”.
- Prawa: grafik, dźwięków i modeli z gry ani ich przeróbek (także tekstur z VDF powiększonych AI) nie ma w repo ani w paczce — paczka ma zawierać tylko program i własne pliki. Własne pliki (narysowane, wygenerowane) → `assets/diathic/` + wpis w `assets/diathic/AUTORZY.txt` (źródło, licencja). Nowy plik w assets = ponowna konfiguracja cmake.

## Build i sprawdzenie
- Jeśli masz terminal Linux: `dev/piaskownica.sh setup` (raz: apt, nagłówki Vulkan, submoduły, cmake), `dev/piaskownica.sh build` (pierwszy długo, potem przyrostowo; wypisuje tylko błędy, logi w `/tmp/diathic`), `dev/piaskownica.sh clang` (clang `-Wconversion -Werror` na zmienionych .cpp — macOS w CI jest ostrzejszy niż GCC: int ↔ size_t, float ↔ int; `Texture2d::w()/h()` zwracają int). Push dopiero bez błędów.
- Bez terminala: pushuj, a błędy kompilacji pokaże build u użytkownika (`gra\lokalne\build.log`) albo CI w PR.
- Gry nie da się uruchomić u ciebie (brak karty i plików gry). Test robi użytkownik: `graj_lokalnie.bat -Branch faza12-grafika` — bez `-Branch` skrypt bierze gałąź z najnowszym commitem, czyli może wziąć gałąź agenta rozgrywki.
- Proś o zrzuty przed / po z tych samych miejsc: Khorinis w dzień (targ), las, jaskinia, noc z pochodnią, walka z magią.

## Git
- Gałąź `faza12-grafika` od `origin/faza11-drzewka-broni`. Jeśli twoje narzędzie samo nadaje nazwę gałęzi — podaj ją użytkownikowi (do `-Branch`).
- Commity po polsku: pierwsza linia = co się zmieniło dla gracza, potem lista zmian. Zmiany tylko dokumentów → `[skip ci]` na początku pierwszej linii.
- PR do bazy `faza11-drzewka-broni` (tytuł i opis po polsku). Po scaleniu #10 i #11 do master: `git merge origin/master` (scalenia są squash, te same zmiany łączą się bez konfliktu) i zmiana bazy PR na master.
- Zmiany bazy pobieraj przez merge, bez przepisywania historii. Scalenie PR tylko na polecenie użytkownika.

## Na koniec każdego zadania
1. GOTHIC-DIABLO.md, sekcja „Faza 12 — grafika”: wiersz tabeli (co robi | gdzie w kodzie) dla każdej nowej albo zmienionej rzeczy, w tym samym commicie; wiedza o rendererze → „Ustalenia techniczne → Grafika i renderer”.
2. Decyzje użytkownika z datą → docs/plan/decyzje.md, sekcja „Faza 12”.
3. Sekcja „Stan” poniżej: 2–5 linii (co w PR, co czeka na test, co następne) — z niej agent rozgrywki przenosi stan do planu projektu.
4. Raport dla użytkownika 3–6 linii: co zrobione, co sprawdzić w grze (z `-Branch`), co następne.

## Pomysły (do potwierdzenia z użytkownikiem, nie plan)
- Ostrzejsza ziemia z kamery 55°: filtrowanie anizotropowe i mipmapy tekstur świata
- Korekcja koloru ARPG (ciepłe światła, chłodniejsze cienie), winieta, delikatne wyostrzenie po skalowaniu
- Poświata tylko jasnych źródeł (ogień, magia, runy)
- Cienie i SSAO z widoku z góry
- Czytelność postaci na tle świata (rozbudowa podświetlenia wroga z fazy 4: obrys, światło krawędzi)
- Paczki tekstur HD instalowane przez gracza: zgodność, kolejność archiwów, luźne pliki w _work

## Następne ulepszenie — jakość tekstur

- Cel: poprawić czytelność tekstur świata z kamery izometrycznej, zachowując surowy wygląd Gothic II.
- Krok 1: sprawdzić sampler tekstur świata i to, które tekstury mają mipmapy; nie zakładać, że `Resources::shadowSampler` jest samplerem świata.
- Krok 2: jeśli tor świata na to pozwala, dodać filtrowanie anizotropowe ograniczone do trybu ARPG, sterowane przez `[DIATHIC] gfxAnisotropy`; dobrać poziom według obsługi GPU i zmierzyć koszt na 1080p.
- Krok 3: porównać te same ujęcia przed/po — targ w Khorinis, ziemia w lesie i jaskini. Jeśli tekstury nadal są zbyt miękkie, zaplanować osobną paczkę lub narzędzie HD uruchamiane u gracza.
- Krok 4 (dodatkowy, po poprawie próbkowania): ocenić DLSS Super Resolution przez Streamline/Vulkan. Sprawdzić wymagane bufory obrazu, głębi i wektorów ruchu, obsługę GPU oraz koszt. DLSS skaluje obraz renderowany w niższej rozdzielczości — nie podnosi rozdzielczości źródłowych tekstur — więc traktować go jako opcjonalne ulepszenie jakości obrazu i wydajności; z fallbackiem, tylko w trybie ARPG.
- Prawa: żadnych oryginalnych tekstur z VDF ani ich przeróbek w repozytorium lub paczce Diathic; ewentualna paczka pozostaje plikiem instalowanym przez gracza.

## Stan
- 2026-10-17: PR #12 otwarty do `faza11-drzewka-broni`; korekcja koloru ARPG czeka na CI i test użytkownika.
- Następne: audyt próbkowania tekstur świata i dostępności mipmap; pierwszy kandydat to anizotropia dla kamery izometrycznej, tylko ARPG, z przełącznikiem `[DIATHIC] gfxAnisotropy`.
- Po tym kroku: ocena DLSS Super Resolution przez Streamline/Vulkan jako osobnego ulepszenia obrazu; DLSS nie zastępuje tekstur HD.
- Jeśli próbkowanie nie wystarczy, przygotować narzędzie lub paczkę HD instalowaną przez gracza; tekstury gry ani ich przeróbki nie trafią do repozytorium ani paczki projektu.
