# Grafika — handoff dla drugiego agenta (2026-10-17)

Drugi agent pracuje równolegle z agentem rozgrywki. Twoje: tekstury, oświetlenie, obraz (shadery, przebiegi po renderze), efekty, własne grafiki. Nie twoje: walka, drzewka, interfejs, mapa, loot, klawisze.

## Start
1. Dokumenty projektu claude/00-zasady.md i claude/02-plan.md oraz skille diathic-iteracja i diathic-build obowiązują, z wyjątkami poniżej.
2. Gałąź `faza12-grafika` od `origin/faza11-drzewka-broni` (ma wszystko z PR #10 i #11). Już jest → checkout + pull.
3. Pierwsze pytanie do użytkownika (jedno, przed kodem): od czego zacząć —
   (a) obraz i światło w shaderach (kolor, kontrast, cienie, mgła, poświata ognia i magii, ostrość) — w repo i w paczce;
   (b) tekstury w wyższej rozdzielczości — tylko paczki, które gracz instaluje sam, albo narzędzie uruchamiane u gracza (patrz „Prawa”);
   (c) własne grafiki (np. ikony umiejętności Q / E / F, ramki) — uzgodnić z agentem rozgrywki, to jego interfejs.
4. Wiedza o rendererze i ładowaniu tekstur: GOTHIC-DIABLO.md, „Ustalenia techniczne” → „Grafika i renderer”.

## Podział plików (mniej konfliktów)
- Twoje: nowe `shader/arpg_*` (poza `arpg.glsl` i `arpg_dof.frag`), nowe `common/graphics/arpggfx.cpp` / `.h` (stałe grafiki w jednym miejscu), nowe pliki w `assets/diathic/`, sekcja „Faza 12 — grafika” w GOTHIC-DIABLO.md, sekcja „Faza 12” w docs/plan/decyzje.md.
- Wspólne — tylko punkt zaczepienia (wywołanie, `arpg` w nazwie), wymienione w opisie PR: `common/graphics/renderer.cpp`, `shaders.cpp` / `.h`, `shader/CMakeLists.txt`, `common/resources.cpp`, `common/mainwindow.cpp`, `shader/arpg.glsl` (mgła wojny), `shader/materials/main.frag` (`arpgHidden`: wycięcia, prześwit, drzewa, podświetlenie wroga — nie przerabiać).
- Nie ruszać: `common/game/*`, PlayerControl, Npc, ArpgMenu, ArpgSkills, ArpgStatus, tabela ArpgKeys (potrzebny klawisz → poproś użytkownika, przełączniki przez Gothic.ini).

## Zasady grafiki
- Tylko tryb ARPG; `-classic` = OpenGothic bez zmian.
- Każdy efekt do wyłączenia w Gothic.ini, sekcja [DIATHIC] (klucze `gfx…`); domyślnie włączone tylko tanie. W opisie PR koszt efektu (karta zintegrowana, 1080p — publikacja.md B5).
- Styl: jak najbliżej oryginału Gothic II; czytelność z góry jak Diablo / Grim Dawn; bez przesadnej poświaty i „plastiku”.
- Prawa: grafik z gry ani ich przeróbek (także tekstur z VDF powiększonych AI) nie ma w repo ani w paczce (publikacja.md A4). Własne pliki (narysowane, wygenerowane) → assets/diathic + wpis w assets/diathic/AUTORZY.txt (źródło, licencja; publikacja.md A1).

## Sprawdzanie
- Piaskownica tylko kompiluje (`dev/piaskownica.sh build`, `clang`; shadery kompiluje glslang w buildzie) — obrazu nie zobaczysz.
- Użytkownik testuje: `graj_lokalnie.bat -Branch faza12-grafika`. Bez `-Branch` skrypt bierze gałąź z najnowszym commitem — może to być gałąź agenta rozgrywki. Launcher (CI) tak samo: `-Branch`.
- Proś o zrzuty przed / po z tych samych miejsc: Khorinis w dzień (targ), las, jaskinia, noc z pochodnią, walka z magią.

## Git i koordynacja
- PR `faza12-grafika` → baza `faza11-drzewka-broni`; po scaleniu #10 i #11: `git fetch --depth=300 origin master && git merge origin/master` (scalenia są squash, te same zmiany łączą się bez konfliktu), potem `gh pr edit <nr> --base master`.
- Zmiany bazy pobieraj przez merge (bez przepisywania historii); klon płytki → najpierw `git fetch --depth=300 origin <baza>`.
- Dokumenty projektu (claude/*.md): zapis nadpisuje cały plik, a drugi agent mógł go zmienić → tuż przed `project_write` zrób `project_read` i zmień tylko swoją linię (02-plan.md: linia „12 grafika”).
- GOTHIC-DIABLO.md i decyzje.md: dopisuj tylko w swoich sekcjach.

## Pomysły (do potwierdzenia z użytkownikiem, nie plan)
- Ostrzejsza ziemia z kamery 55°: filtrowanie anizotropowe i mipmapy tekstur świata
- Korekcja koloru ARPG (ciepłe światła, chłodniejsze cienie), winieta, delikatne wyostrzenie po skalowaniu
- Poświata tylko jasnych źródeł (ogień, magia, runy)
- Cienie i SSAO z widoku z góry
- Czytelność postaci na tle świata (rozbudowa podświetlenia z fazy 4: obrys, światło krawędzi)
- Paczki tekstur HD instalowane przez gracza: zgodność, kolejność archiwów, luźne pliki w _work
