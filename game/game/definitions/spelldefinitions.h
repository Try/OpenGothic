#pragma once

#include <phoenix/vm.hh>
#include <phoenix/ext/daedalus_classes.hh>

class SpellDefinitions final {
  public:
    SpellDefinitions(zenkit::DaedalusVm &vm);
    ~SpellDefinitions();

    const zenkit::ISpell& find(std::string_view instanceName) const;

  private:
    zenkit::DaedalusVm&                          vm;
    std::vector<std::shared_ptr<zenkit::ISpell>> spl;
  };
