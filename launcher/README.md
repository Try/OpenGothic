# gothic-diablo launcher (Windows)

Każdy build Windows z `master` albo z PR tej repo publikowany jest przez CI jako release `build-<N>`
(paczka `gothic-diablo-win64.zip`, tylko gra bez Spacera). Trzymanych jest 15 ostatnich.

`uruchom_arpg.bat` / `uruchom_klasycznie.bat` → `launcher.ps1`:

1. pobiera najnowszy release (bez logowania), sprawdza rozmiar i SHA-256, rozpakowuje do `gra\wersje\build-<N>`,
2. zapisy, `Gothic.ini`, `log.txt`, `crash.log` są w `gra\dane` (katalog roboczy gry, wspólny dla wszystkich wersji),
3. usuwa starsze wersje (zostawia 2),
4. uruchamia `Gothic2Notr.exe -g "<Gothic II>"`.

Bez internetu uruchamia to, co jest na dysku.

Opcje (dopisać w `.bat` albo podać w wierszu poleceń):

| opcja | działanie |
|---|---|
| `-Branch master` | tylko buildy z danej gałęzi |
| `-Build 3631` | konkretny build (cofnięcie wersji) |
| `-Offline` | bez sprawdzania aktualizacji |
| `-NoStart` | tylko aktualizacja |
| `-Keep 3` | ile wersji trzymać |
| `-GothicPath "D:\Gothic II"` | inna ścieżka do gry (domyślnie Steam na F:) |

Pozostałe parametry trafiają do `Gothic2Notr.exe`, np. `uruchom_arpg.bat -window`.
