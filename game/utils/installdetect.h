#pragma once

#include <Tempest/Platform>
#include <string>

class InstallDetect final {
  public:
    InstallDetect();

    std::u16string detectG2();
#ifdef __OSX__
    static std::u16string applicationSupportDirectory();
#endif

  private:
    std::u16string detectG2(std::u16string pfiles);

#ifdef __WINDOWS__
    static std::u16string programFiles(bool x86);
    std::u16string pfiles, pfilesX86;
#endif

#ifdef __OSX__
    std::u16string appDir;
#endif
  };
