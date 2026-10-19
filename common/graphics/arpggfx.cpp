#include "arpggfx.h"

#include <algorithm>

#include "gothic.h"
#include "resources.h"

namespace {
int iniValue(std::string_view key, int def) {
  if(Gothic::settingsGetS("DIATHIC", key).empty())
    return def;
  return Gothic::settingsGetI("DIATHIC", key);
  }

bool cachedAnisotropy = false;
}

namespace ArpgGfx {
const std::vector<Option>& options() {
  static const std::vector<Option> opts = {
    {"gfxColorGrade", "Korekcja kolorów", "Nadaje obrazowi ARPG subtelne chłodniejsze cienie i cieplejsze światła.",
     {"Wyłączona", "Włączona"}, 0, false},
    {"gfxAnisotropy", "Filtrowanie anizotropowe", "Wyostrza tekstury świata oglądane pod kątem.",
     {"Wyłączone", "Włączone"}, 1, false},
    };
  return opts;
  }

int levelsSupported(const Option& o) {
  if(o.ini == "gfxAnisotropy" && !Resources::device().properties().anisotropy)
    return 1;
  return int(o.levels.size());
  }

int value(std::string_view ini) {
  for(const auto& o : options()) {
    if(o.ini != ini)
      continue;
    const int maxLevel = std::max(0, levelsSupported(o)-1);
    return std::clamp(iniValue(ini, o.def), 0, maxLevel);
    }
  return 0;
  }

void refresh() {
  cachedAnisotropy = value("gfxAnisotropy") != 0;
  }

Tempest::Sampler worldSampler(bool arpgEnabled) {
  if(!arpgEnabled || cachedAnisotropy)
    return Tempest::Sampler::anisotrophy();
  return Tempest::Sampler::trillinear();
  }
}
