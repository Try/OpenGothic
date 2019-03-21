#pragma once

#include <string>

class InstallDetect final {
  public:
    InstallDetect();

    std::u16string detectG2();

  private:
    std::u16string programFiles();
    bool           check(const std::u16string &path);

    std::u16string pfiles;
  };
