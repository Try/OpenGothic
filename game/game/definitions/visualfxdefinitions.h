#pragma once

#include <phoenix/vm.hh>
#include <phoenix/ext/daedalus_classes.hh>

#include <unordered_map>
#include <memory>

class VisualFx;

class VisualFxDefinitions final {
  public:
    VisualFxDefinitions();
    ~VisualFxDefinitions();

    const VisualFx *get(std::string_view name);

  private:
    std::unique_ptr<phoenix::vm>                   vm;
    std::unordered_map<std::string,std::unique_ptr<VisualFx>> vfx;

    std::shared_ptr<phoenix::c_fx_base> implGet(std::string_view name);
  };
