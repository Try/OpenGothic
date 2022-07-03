#pragma once

#include <phoenix/daedalus/interpreter.hh>
#include <phoenix/ext/daedalus_classes.hh>

class SpellDefinitions final {
  public:
    SpellDefinitions(phoenix::daedalus::vm &vm);
    ~SpellDefinitions();

    const phoenix::daedalus::c_spell& find(std::string_view instanceName) const;

  private:
    phoenix::daedalus::vm&                                   vm;
    std::vector<std::shared_ptr<phoenix::daedalus::c_spell>> spl;
  };
