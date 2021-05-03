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
    const Daedalus::GEngineClasses::CCamSys& deathCam()     const { return camModDeath;  }
    const Daedalus::GEngineClasses::CCamSys& swimCam()      const { return camModSwim;   }
    const Daedalus::GEngineClasses::CCamSys& diveCam()      const { return camModDive;   }
    const Daedalus::GEngineClasses::CCamSys& mobsiCam(const char* tag,const char* pos) const;

  private:
    Daedalus::GEngineClasses::CCamSys loadCam(const char *name);

    struct Camera:Daedalus::GEngineClasses::CCamSys{
      std::string name;
      };
    std::vector<Camera> cameras;
    const Camera* find(const char* name) const;

    Daedalus::GEngineClasses::CCamSys camModDialog, camModInventory, camModNormal, camModDeath, camModSwim, camModDive;
    Daedalus::GEngineClasses::CCamSys camModMelee, camModRange, camModMage;
  };
