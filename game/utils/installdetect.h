#pragma once

#include <Tempest/Platform>
#include <filesystem>
#include <string>

class InstallDetect final {
  public:
    InstallDetect();

    std::u16string detectG2();

  private:
    std::u16string detectG2(std::u16string pfiles);

#ifdef __WINDOWS__
    std::u16string programFiles(bool x86);
#endif
    std::u16string pfiles, pfilesX86;
  };
