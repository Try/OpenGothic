#pragma once

#include <daedalus/DaedalusStdlib.h>

class CameraDefinitions final {
  public:
    CameraDefinitions();

    const Daedalus::GEngineClasses::CCamSys& dialogCam()    const { return camModDialog; }
    const Daedalus::GEngineClasses::CCamSys& inventoryCam() const { return camModInventory; }
    const Daedalus::GEngineClasses::CCamSys& stdCam()       const { return camModNormal; }
    const Daedalus::GEngineClasses::CCamSys& fpCam()        const { return camModFp; }
    const Daedalus::GEngineClasses::CCamSys& backCam()      const { return camModBack; }
    const Daedalus::GEngineClasses::CCamSys& meleeCam()     const { return camModMelee;  }
    const Daedalus::GEngineClasses::CCamSys& rangeCam()     const { return camModRange;  }
    const Daedalus::GEngineClasses::CCamSys& mageCam()      const { return camModMage;   }
    const Daedalus::GEngineClasses::CCamSys& deathCam()     const { return camModDeath;  }
    const Daedalus::GEngineClasses::CCamSys& swimCam()      const { return camModSwim;   }
    const Daedalus::GEngineClasses::CCamSys& diveCam()      const { return camModDive;   }
    const Daedalus::GEngineClasses::CCamSys& mobsiCam(std::string_view tag, std::string_view pos) const;

  private:
    Daedalus::GEngineClasses::CCamSys loadCam(std::string_view name);

    struct Camera:Daedalus::GEngineClasses::CCamSys{
      std::string name;
      };
    std::vector<Camera> cameras;
    const Camera* find(std::string_view name) const;

    Daedalus::GEngineClasses::CCamSys camModNormal, camModFp, camModBack;
    Daedalus::GEngineClasses::CCamSys camModDialog, camModInventory, camModDeath, camModSwim, camModDive;
    Daedalus::GEngineClasses::CCamSys camModMelee, camModRange, camModMage;
  };
