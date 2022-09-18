#pragma once

#include <phoenix/vm.hh>
#include <phoenix/ext/daedalus_classes.hh>

#include <memory>

class MusicDefinitions final {
  public:
    MusicDefinitions();
    ~MusicDefinitions();

    const phoenix::c_music_theme* operator[](std::string_view name) const;

  private:
    std::unique_ptr<phoenix::vm>  vm;
    std::vector<std::shared_ptr<phoenix::c_music_theme>> themes;
  };
