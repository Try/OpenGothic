#pragma once

#include <daedalus/DaedalusStdlib.h>

#include <unordered_map>
#include <memory>

class VisualFx;

class VisualFxDefinitions final {
  public:
    VisualFxDefinitions();
    ~VisualFxDefinitions();

    const VisualFx *get(std::string_view name);

  private:
    std::unique_ptr<Daedalus::DaedalusVM>                     vm;
    std::unordered_map<std::string,std::unique_ptr<VisualFx>> vfx;

    bool implGet(std::string_view name, Daedalus::GEngineClasses::CFx_Base& out);
  };
