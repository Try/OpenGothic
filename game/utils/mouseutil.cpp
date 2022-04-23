#include "mouseutil.h"

#include <Tempest/Platform>

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
    return float(mouseSpeed)/10.f;
    }
  return 1.f;
#else
  return 1.f;
#endif
  }
