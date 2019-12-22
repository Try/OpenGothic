#pragma once

#include <daedalus/DaedalusStdlib.h>
#include <daedalus/ZString.h>
#include <memory>

class Gothic;

class SvmDefinitions final {
  public:
    SvmDefinitions(Daedalus::DaedalusVM &vm);
    ~SvmDefinitions();

    const Daedalus::ZString& find(const char* speech,const int id);

  private:
    Daedalus::DaedalusVM&                                         vm;
    std::vector<std::unique_ptr<Daedalus::GEngineClasses::C_SVM>> svm;
  };

