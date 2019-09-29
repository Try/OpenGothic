#pragma once

#include <Tempest/Platform>
#include <string>

class InstallDetect final {
  public:
    InstallDetect();

    std::u16string detectG2();

  private:
#ifdef __WINDOWS__
    std::u16string programFiles();
#endif
    bool           check(const std::u16string &path);

    std::u16string pfiles;
  };
