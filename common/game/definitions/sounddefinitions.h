#pragma once

#include <vector>
#include <initializer_list>

#include <zenkit/addon/daedalus.hh>

#include <unordered_map>

class SoundDefinitions final {
  public:
    SoundDefinitions();

    const zenkit::ISoundEffect& operator [](std::string_view name) const;
    // Diathic: names of the sounds that contain one of the parts (sorted, shortest first)
    std::vector<std::string>    find(std::initializer_list<std::string_view> parts) const;

  private:
    std::unordered_map<std::string, std::shared_ptr<zenkit::ISoundEffect>> sfx;
  };

