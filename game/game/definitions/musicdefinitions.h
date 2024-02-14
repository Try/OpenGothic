#pragma once

#include <phoenix/vm.hh>
#include <phoenix/ext/daedalus_classes.hh>

#include <memory>

class MusicDefinitions final {
  public:
    MusicDefinitions();
    ~MusicDefinitions();

    const zenkit::IMusicTheme* operator[](std::string_view name) const;

  private:
    std::unique_ptr<zenkit::DaedalusVm>  vm;
    std::vector<std::shared_ptr<zenkit::IMusicTheme>> themes;
  };
