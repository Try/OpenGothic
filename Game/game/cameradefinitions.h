#pragma once

#include <daedalus/DaedalusStdlib.h>

class Gothic;

class CameraDefinitions final {
  public:
    CameraDefinitions(Gothic &gothic);

    const Daedalus::GEngineClasses::CCamSys& dialogCam() const { return camModDialog; }

  private:
    Daedalus::GEngineClasses::CCamSys loadCam(Daedalus::DaedalusVM &vm, const char *name);

    Daedalus::GEngineClasses::CCamSys camModDialog;
  };
