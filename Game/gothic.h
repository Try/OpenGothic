#pragma once

#include <string>
#include <memory>

#include <daedalus/DaedalusVM.h>

class Gothic final {
  public:
    Gothic(int argc,const char** argv);

    const std::string&                    path() const { return gpath; }
    std::unique_ptr<Daedalus::DaedalusVM> createVm(const char* datFile);

  private:
    std::string gpath;
  };
