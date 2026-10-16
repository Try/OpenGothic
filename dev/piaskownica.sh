#!/usr/bin/env bash
# Build i sprawdzenie w piaskownicy Claude (Linux, GCC + clang). Wypisuje tylko błędy,
# pełne logi zostają w $DIATHIC_TMP (domyślnie /tmp/diathic).
# Użycie: dev/piaskownica.sh setup | build | clang [pliki.cpp...] | all
#   setup  - zależności apt, nagłówki Vulkan, submoduły, konfiguracja cmake (raz na sesję)
#   build  - przyrostowy build gry (Gothic2Notr); pierwszy build trwa długo - puszczać w tle
#   clang  - clang -Wconversion -Werror na podanych plikach albo na zmienionych .cpp
#   all    - setup + build + clang
set -u
R=$(cd "$(dirname "$0")/.." && pwd)
S=${DIATHIC_TMP:-/tmp/diathic}
mkdir -p "$S"
export VULKAN_SDK=$S/vkh

setup() {
  if ! command -v glslangValidator >/dev/null; then
    (apt-get update && apt-get install -y --no-install-recommends \
      glslang-tools libvulkan-dev libasound2-dev libxcursor-dev) >"$S/apt.log" 2>&1 \
      || { echo "apt: BŁĄD ($S/apt.log)"; return 1; }
  fi
  # nagłówki Vulkan z apt są za stare dla Tempest
  [ -d "$S/vkh" ] || git clone -q --depth 1 https://github.com/KhronosGroup/Vulkan-Headers.git "$S/vkh" >"$S/vkh.log" 2>&1 \
    || { echo "Vulkan-Headers: BŁĄD ($S/vkh.log)"; return 1; }
  git -C "$R" submodule update --init --recursive -q >"$S/sub.log" 2>&1 \
    || { echo "submoduły: BŁĄD ($S/sub.log)"; return 1; }
  if [ ! -f "$R/build/build.ninja" ]; then
    cmake -S "$R" -B "$R/build" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >"$S/cmake.log" 2>&1 \
      || { tail -20 "$S/cmake.log"; return 1; }
  fi
  echo "setup: OK"
}

reconfigure() {
  # nowe pliki .cpp (file GLOB) wchodzą do buildu dopiero po ponownej konfiguracji
  cmake "$R/build" >>"$S/cmake.log" 2>&1 || { tail -20 "$S/cmake.log"; return 1; }
}

build() {
  reconfigure || return 1
  cmake --build "$R/build" --target Gothic2Notr -j"$(nproc)" >"$S/build.log" 2>&1
  local rc=$?
  grep -E 'error|warning: ' -A4 "$S/build.log" | grep -v Winline | head -40
  if [ $rc = 0 ]; then echo "build: OK"; else echo "build: BŁĄD (pełny log: $S/build.log)"; fi
  return $rc
}

changed() {
  # .cpp zmienione względem master: zacommitowane, niezacommitowane i nowe
  { git -C "$R" diff --name-only origin/master...HEAD
    git -C "$R" diff --name-only HEAD
    git -C "$R" ls-files --others --exclude-standard
  } 2>/dev/null | grep -E '\.cpp$' | sort -u
}

clang_check() {
  local files=("$@")
  if [ ${#files[@]} = 0 ]; then mapfile -t files < <(changed); fi
  if [ ${#files[@]} = 0 ]; then echo "clang: brak zmienionych plików .cpp"; return 0; fi
  reconfigure || return 1
  python3 "$R/dev/clangcheck.py" "${files[@]}"
}

case "${1:-all}" in
  setup) setup ;;
  build) build ;;
  clang) shift; clang_check "$@" ;;
  all)   setup && build && clang_check ;;
  *)     echo "użycie: $0 setup | build | clang [pliki] | all"; exit 2 ;;
esac
