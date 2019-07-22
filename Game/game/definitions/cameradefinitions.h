#pragma once

#include <daedalus/DaedalusStdlib.h>

class Gothic;

class CameraDefinitions final {
  public:
    CameraDefinitions(Gothic &gothic);

    const Daedalus::GEngineClasses::CCamSys& dialogCam()    const { return camModDialog; }
    const Daedalus::GEngineClasses::CCamSys& inventoryCam() const { return camModInventory; }
    const Daedalus::GEngineClasses::CCamSys& stdCam()       const { return camModNormal; }
    const Daedalus::GEngineClasses::CCamSys& meleeCam()     const { return camModMelee;  }
    const Daedalus::GEngineClasses::CCamSys& rangeCam()     const { return camModRange;  }
    const Daedalus::GEngineClasses::CCamSys& mageCam()      const { return camModMage;   }

  private:
    Daedalus::GEngineClasses::CCamSys loadCam(Daedalus::DaedalusVM &vm, const char *name);

    Daedalus::GEngineClasses::CCamSys camModDialog, camModInventory, camModNormal;
    Daedalus::GEngineClasses::CCamSys camModMelee, camModRange, camModMage;
  };
