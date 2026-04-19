#pragma once

#include <zenkit/DaedalusVm.hh>
#include <zenkit/addon/daedalus.hh>

#include <functional>
#include <memory>

class MusicDefinitions final {
  public:
    MusicDefinitions();
    ~MusicDefinitions();

    const zenkit::IMusicTheme* operator[](std::string_view name) const;

    std::string_view completeThemeName(std::string_view prefix, bool& fullword) const;
    void listMatchingThemeNames(std::string_view prefix,
                                const std::function<void(std::string_view)>& cb) const;

  private:
    std::unique_ptr<zenkit::DaedalusVm>  vm;
    std::vector<std::shared_ptr<zenkit::IMusicTheme>> themes;
  };
