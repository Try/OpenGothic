#pragma once

#include <phoenix/ext/daedalus_classes.hh>

class CameraDefinitions final {
  public:
    CameraDefinitions();

    const zenkit::ICamera& dialogCam()    const { return camModDialog;    }
    const zenkit::ICamera& inventoryCam() const { return camModInventory; }
    const zenkit::ICamera& stdCam()       const { return camModNormal; }
    const zenkit::ICamera& fpCam()        const { return camModFp;     }
    const zenkit::ICamera& backCam()      const { return camModBack;   }
    const zenkit::ICamera& meleeCam()     const { return camModMelee;  }
    const zenkit::ICamera& rangeCam()     const { return camModRange;  }
    const zenkit::ICamera& mageCam()      const { return camModMage;   }
    const zenkit::ICamera& deathCam()     const { return camModDeath;  }
    const zenkit::ICamera& swimCam()      const { return camModSwim;   }
    const zenkit::ICamera& diveCam()      const { return camModDive;   }
    const zenkit::ICamera& fallCam()      const { return camModFall;   }
    const zenkit::ICamera& mobsiCam(std::string_view tag, std::string_view pos) const;

  private:
    zenkit::ICamera getCam(std::string_view name);

    std::vector<std::pair<std::string, zenkit::ICamera>> cameras;
    const zenkit::ICamera* find(std::string_view name) const;

    zenkit::ICamera camModNormal, camModFp, camModBack;
    zenkit::ICamera camModDialog, camModInventory, camModDeath, camModSwim, camModDive, camModFall;
    zenkit::ICamera camModMelee, camModRange, camModMage;
  };
