#pragma once

#include <daedalus/DaedalusStdlib.h>

class Gothic;

class SvmDefinitions final {
  public:
    SvmDefinitions(Daedalus::DaedalusVM &vm);

    const std::string &                    find(const char* speech,const int id);

  private:
    Daedalus::DaedalusVM&                        vm;
    std::vector<Daedalus::GEngineClasses::C_SVM> svm;
  };

