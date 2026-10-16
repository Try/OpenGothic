# Lokalny build (Windows)

Szybka pętla na własnym PC: commit na GitHubie → build przyrostowy u siebie (~1 min) → gra.
Każda zmiana dalej idzie na GitHuba, CI i launcher (`build-<N>`) działają jak dotąd.

## Instalacja (raz)

`dev_setup.bat` w folderze launchera (`G:\projekty\gothic-diablo`):

1. MSYS2 do `G:\msys64` (albo istniejący `C:\msys64`) + gcc, cmake, ninja, glslang, Vulkan, ccache, lld,
2. klon repo do `src\` (z submodułami),
3. pierwszy pełny build (kilka minut).

## Na co dzień

| plik | działanie |
|---|---|
| `graj_lokalnie.bat` | pobiera najnowszy commit (gałąź z najnowszym commitem, jak launcher), buduje przyrostowo, uruchamia grę |
| `buduj_w_tle.bat` | zostaw otwarte: co 15 s sprawdza GitHuba i od razu buduje nowy commit (dźwięk = gotowe) |
| `Diathic.bat` (wcześniej `Gothic Diablo.bat`) | launcher bez zmian – buildy z CI |

Gra startuje z `gra\dane` – te same zapisy i `Gothic.ini` co w launcherze.
Zbudowane wersje: `gra\lokalne\<commit>\` (2 ostatnie). Logi: `gra\lokalne\build.log`, `setup.log`.

Opcje (`graj_lokalnie.bat ...`): `-Classic`, `-Branch master`, `-NoPull`, `-NoStart`, `-GothicPath "D:\Gothic II"`;
pozostałe parametry trafiają do `Gothic2Notr.exe`.

Build lokalny: RelWithDebInfo z `-g1` (lżejsze info debug), tylko `Gothic2Notr` (bez Spacera), ccache, linker lld.
`dev.ps1` i `.bat` w folderze launchera aktualizują się same z `src\dev`.

## Piaskownica Claude (Linux)
`dev/piaskownica.sh` — build i sprawdzenie przed pushem, wypisuje tylko błędy:
- `setup` (raz na sesję: apt, nagłówki Vulkan, submoduły, cmake), `build` (przyrostowo; pierwszy build w tle), `clang` (clang `-Wconversion -Werror` jak macOS w CI, na zmienionych `.cpp` albo podanych plikach), `all`
- pełne logi w `/tmp/diathic` (`DIATHIC_TMP`)
