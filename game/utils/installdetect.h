#pragma once

#include <Tempest/Platform>
#include <string>

class InstallDetect final {
  public:
    InstallDetect();

    std::u16string detectG2();
#if defined(__OSX__) || defined(__IOS__) || defined(__ANDROID__)
    static std::u16string applicationSupportDirectory();
#endif

  private:
    std::u16string detectG2(std::u16string pfiles);

#ifdef __WINDOWS__
    static std::u16string programFiles(bool x86);
    std::u16string pfiles, pfilesX86;
#endif

#if defined(__OSX__) || defined(__IOS__) || defined(__ANDROID__)
    std::u16string appDir;
#endif
  };
