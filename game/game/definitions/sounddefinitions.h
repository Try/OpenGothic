#pragma once

#include <phoenix/ext/daedalus_classes.hh>

#include <unordered_map>

class SoundDefinitions final {
  public:
    SoundDefinitions();

    const phoenix::c_sfx& operator[](std::string_view name) const;

  private:
    std::unordered_map<std::string, std::shared_ptr<phoenix::c_sfx>> sfx;
  };

