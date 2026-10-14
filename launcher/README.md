# gothic-diablo launcher (Windows)

Każdy build Windows z `master` albo z PR tej repo CI publikuje jako release `build-<N>`
(paczka `gothic-diablo-win64.zip`, gra bez Spacera). Trzymanych jest 15 ostatnich.

Pliki kopiuje się do `G:\projekty\gothic-diablo`:

- `Gothic Diablo.bat` – okno launchera: sam pobiera najnowszy build (pasek postępu), pokazuje „co nowego”,
  pozwala wybrać starszą wersję, przyciski **Graj (ARPG)** / **Graj klasycznie**. Przy pierwszym uruchomieniu
  tworzy skrót `Gothic Diablo.lnk` (bez migającej konsoli, z ikoną Gothic II) – można go przypiąć do paska / pulpitu.
- `uruchom_arpg.bat` / `uruchom_klasycznie.bat` – szybki start bez okna (aktualizacja + gra).

Co robi `launcher.ps1`:

1. pobiera release (bez logowania), sprawdza rozmiar i SHA-256, rozpakowuje do `gra\wersje\build-<N>`,
2. zapisy, `Gothic.ini`, `log.txt`, `crash.log` są w `gra\dane` (katalog roboczy gry, wspólny dla wszystkich wersji),
   stary folder `build\` jest przenoszony i usuwany,
3. usuwa starsze wersje (zostawia 2),
4. uruchamia `Gothic2Notr.exe -g "<Gothic II>"`.

Folder Gothic II launcher szuka sam: Steam (wszystkie biblioteki z `libraryfolders.vdf`, gra 39510), GOG,
wpisy instalatorów w rejestrze, typowe ścieżki na każdym dysku. Musi być `System\Gothic2.exe`
i `Data\Worlds_Addon.vdf` (Noc Kruka). Gdy nic nie znajdzie, okno launchera prosi o wskazanie folderu;
zmiana później: link **Folder Gothic II**. Wybór zapisuje się w `launcher.json` obok launchera.

Bez internetu uruchamia to, co jest na dysku.

Opcje (dopisać w `.bat` albo podać w wierszu poleceń):

| opcja | działanie |
|---|---|
| `-Console` | bez okna |
| `-Classic` | tryb klasyczny (z `-Console`) |
| `-Branch master` | tylko buildy z danej gałęzi |
| `-Build 3631` | konkretny build (cofnięcie wersji) |
| `-Offline` | bez sprawdzania aktualizacji |
| `-NoStart` | tylko aktualizacja |
| `-Keep 3` | ile wersji trzymać |
| `-GothicPath "D:\Gothic II"` | folder gry bez szukania |
| `-Channel tester` | tylko wersje testowe (`tester-<N>`); to samo daje `"Channel": "tester"` w `launcher.json` |

Pozostałe parametry trafiają do `Gothic2Notr.exe`, np. `uruchom_arpg.bat -window`.

## Wersje dla testerów

Testerzy dostają stały link do paczki launchera:
`https://github.com/wr0b3l/gothic-diablo/releases/download/testerzy/GothicDiablo-testerzy.zip`
(`Gothic Diablo.bat`, `launcher.ps1`, `launcher.json` z kanałem `tester`, `CZYTAJ.txt` z instrukcją).
Ich launcher pobiera tylko wydania `tester-<N>` – nie każdy build.

Udostępnienie buildu testerom: GitHub → **Actions** → **Wydanie dla testerów** → **Run workflow**
(numer buildu – puste = najnowszy z `master`; opcjonalnie wiadomość dla testerów, widoczna w launcherze).
Workflow kopiuje paczkę `build-<N>` do wydania `tester-<N>` (zostaje 10 ostatnich) i odświeża paczkę launchera.
Teksty: `testerzy/CZYTAJ.txt` (w paczce), `testerzy/wydanie.md` (strona wydania `testerzy`).
