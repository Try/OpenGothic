#pragma once

#include <phoenix/vm.hh>
#include <phoenix/ext/daedalus_classes.hh>

#include <memory>

class SvmDefinitions final {
  public:
    SvmDefinitions(phoenix::vm& vm);
    ~SvmDefinitions();

    const std::string& find(std::string_view speech, int id);

  private:
    phoenix::vm&                                 vm;
    std::vector<std::shared_ptr<phoenix::c_svm>> svm;
  };

