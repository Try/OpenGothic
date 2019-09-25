#pragma once

#include <daedalus/DaedalusStdlib.h>

#include <unordered_map>
#include <memory>

class Gothic;
class VisualFx;

class VisualFxDefinitions final {
  public:
    VisualFxDefinitions(Gothic &gothic);
    ~VisualFxDefinitions();

    const VisualFx *get(const char* name);

  private:
    std::unique_ptr<Daedalus::DaedalusVM>                     vm;
    std::unordered_map<std::string,std::unique_ptr<VisualFx>> vfx;

    Daedalus::GEngineClasses::CFx_Base *implGet(const char* name);
  };
