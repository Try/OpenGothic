#pragma once

#include <zenkit/DaedalusVm.hh>
#include <zenkit/addon/daedalus.hh>

#include <memory>

class SvmDefinitions final {
  public:
    SvmDefinitions(zenkit::DaedalusVm& vm);
    ~SvmDefinitions();

    std::string_view find(std::string_view speech, int id);

  private:
    zenkit::DaedalusVm&                                    vm;
    std::vector<std::shared_ptr<zenkit::DaedalusInstance>> svm;
  };

