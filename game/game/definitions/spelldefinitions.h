#pragma once

#include <phoenix/vm.hh>
#include <phoenix/ext/daedalus_classes.hh>

class SpellDefinitions final {
  public:
    SpellDefinitions(phoenix::vm &vm);
    ~SpellDefinitions();

    const phoenix::c_spell& find(std::string_view instanceName) const;

  private:
    phoenix::vm&                                   vm;
    std::vector<std::shared_ptr<phoenix::c_spell>> spl;
  };
