#include "mouseutil.h"

#include <Tempest/Platform>
#include <algorithm>

#ifdef __WINDOWS__
#include <windows.h>
#include <shlwapi.h>
#else
#include <sys/stat.h>
#endif

float MouseUtil::mouseSysSpeed() {
#ifdef __WINDOWS__
  int mouseSpeed = 10;
  if(SystemParametersInfoA(SPI_GETMOUSESPEED, 0, &mouseSpeed, 0)) {
    // MSDN says in between 1..20, but clamp to be extra safe
    mouseSpeed = std::clamp(mouseSpeed, 1, 20);
    return float(mouseSpeed)/10.f;
    }
  return 1.f;
#else
  return 1.f;
#endif
  }
