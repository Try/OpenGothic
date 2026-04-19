#include "musicdefinitions.h"

#include <Tempest/Log>
#include <cctype>

#include "gothic.h"

using namespace Tempest;

namespace {

bool startsWith(std::string_view str, std::string_view prefix) {
  if(prefix.size() > str.size())
    return false;
  for(size_t i = 0; i < prefix.size(); ++i) {
    if(std::tolower(str[i]) != std::tolower(prefix[i]))
      return false;
    }
  return true;
  }

bool compareNoCase(std::string_view a, std::string_view b) {
  if(a.size() != b.size())
    return false;
  for(size_t i = 0; i < a.size(); ++i) {
    const int ca = std::tolower(a[i]);
    const int cb = std::tolower(b[i]);
    if(ca != cb)
      return false;
    }
  return true;
  }

std::string_view fileStem(std::string_view path) {
  const size_t slash = path.find_last_of("/\\");
  if(slash != std::string_view::npos)
    path = path.substr(slash + 1);

  const size_t dot = path.find_last_of('.');
  if(dot != std::string_view::npos)
    path = path.substr(0, dot);

  return path;
  }

}

MusicDefinitions::MusicDefinitions() {
  vm = Gothic::inst().createPhoenixVm("Music.dat");

  vm->enumerate_instances_by_class_name("C_MusicTheme", [this](zenkit::DaedalusSymbol& s) {
    themes.push_back(vm->init_instance<zenkit::IMusicTheme>(&s));
    });
  }

MusicDefinitions::~MusicDefinitions() {
  }

const zenkit::IMusicTheme* MusicDefinitions::operator [](std::string_view name) const {
  if(!vm || name.empty())
    return nullptr;

  auto findBySymbolIndex = [&](uint32_t index) -> const zenkit::IMusicTheme* {
    for(auto& i : themes) {
      auto* sym = vm->find_symbol_by_instance(i);
      if(sym != nullptr && sym->index() == index)
        return i.get();
      }
    return nullptr;
    };

  if(auto* sym = vm->find_symbol_by_name(name)) {
    if(auto* theme = findBySymbolIndex(sym->index()))
      return theme;
    }

  for(auto& i : themes) {
    auto* sym = vm->find_symbol_by_instance(i);
    if(sym != nullptr && compareNoCase(sym->name(), name))
      return i.get();
    }

  const auto shortName = fileStem(name);
  for(auto& i : themes) {
    if(compareNoCase(i->file, name) || compareNoCase(fileStem(i->file), shortName))
      return i.get();
    }

  return nullptr;
  }

void MusicDefinitions::listMatchingThemeNames(std::string_view prefix,
                                              const std::function<void(std::string_view)>& cb) const {
  if(!vm)
    return;
  for(auto& theme : themes) {
    auto* sym = vm->find_symbol_by_instance(theme);
    if(sym == nullptr)
      continue;
    auto name = std::string_view(sym->name());
    if(startsWith(name, prefix))
      cb(name);
    }
  }

std::string_view MusicDefinitions::completeThemeName(std::string_view prefix, bool& fullword) const {
  if(!vm || prefix.empty())
    return "";
  std::string_view match = "";
  fullword = true;
  for(auto& theme : themes) {
    auto* sym = vm->find_symbol_by_instance(theme);
    if(sym == nullptr)
      continue;
    auto name = std::string_view(sym->name());
    if(!startsWith(name, prefix))
      continue;
    if(match.empty()) {
      match = name;
      continue;
      }
    // Shrink to longest common prefix among all matching names.
    if(name.size() > match.size())
      name = name.substr(0, match.size());
    for(size_t k = 0; k < name.size(); ++k) {
      if(std::tolower(name[k]) != std::tolower(match[k])) {
        name     = name.substr(0, k);
        fullword = false;
        break;
        }
      }
    match    = name;
    fullword = (match.size() == name.size()) && fullword;
    }
  return match;
  }
