#pragma once

#include <phoenix/daedalus/interpreter.hh>
#include <phoenix/ext/daedalus_classes.hh>

#include <memory>

class MusicDefinitions final {
  public:
    MusicDefinitions();
    ~MusicDefinitions();

    const phoenix::daedalus::c_music_theme* operator[](std::string_view name) const;

  private:
    std::unique_ptr<phoenix::daedalus::vm>  vm;
    std::vector<std::shared_ptr<phoenix::daedalus::c_music_theme>> themes;
  };
