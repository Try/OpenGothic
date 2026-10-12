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
| `-GothicPath "D:\Gothic II"` | inna ścieżka do gry (domyślnie Steam na F:) |

Pozostałe parametry trafiają do `Gothic2Notr.exe`, np. `uruchom_arpg.bat -window`.
