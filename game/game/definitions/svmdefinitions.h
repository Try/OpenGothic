#pragma once

#include <phoenix/daedalus/interpreter.hh>
#include <phoenix/ext/daedalus_classes.hh>

#include <memory>

class SvmDefinitions final {
  public:
    SvmDefinitions(phoenix::daedalus::vm& vm);
    ~SvmDefinitions();

    const std::string& find(std::string_view speech, int id);

  private:
    phoenix::daedalus::vm&                                 vm;
    std::vector<std::shared_ptr<phoenix::daedalus::c_svm>> svm;
  };

