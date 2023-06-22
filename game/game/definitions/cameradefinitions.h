#pragma once

#include <phoenix/ext/daedalus_classes.hh>

class CameraDefinitions final {
  public:
    CameraDefinitions();

    const phoenix::c_camera& dialogCam()    const { return camModDialog;    }
    const phoenix::c_camera& inventoryCam() const { return camModInventory; }
    const phoenix::c_camera& stdCam()       const { return camModNormal; }
    const phoenix::c_camera& fpCam()        const { return camModFp;     }
    const phoenix::c_camera& backCam()      const { return camModBack;   }
    const phoenix::c_camera& meleeCam()     const { return camModMelee;  }
    const phoenix::c_camera& rangeCam()     const { return camModRange;  }
    const phoenix::c_camera& mageCam()      const { return camModMage;   }
    const phoenix::c_camera& deathCam()     const { return camModDeath;  }
    const phoenix::c_camera& swimCam()      const { return camModSwim;   }
    const phoenix::c_camera& diveCam()      const { return camModDive;   }
    const phoenix::c_camera& fallCam()      const { return camModFall;   }
    const phoenix::c_camera& mobsiCam(std::string_view tag, std::string_view pos) const;

  private:
    phoenix::c_camera getCam(std::string_view name);

    std::vector<std::pair<std::string, phoenix::c_camera>> cameras;
    const phoenix::c_camera* find(std::string_view name) const;

    phoenix::c_camera camModNormal, camModFp, camModBack;
    phoenix::c_camera camModDialog, camModInventory, camModDeath, camModSwim, camModDive, camModFall;
    phoenix::c_camera camModMelee, camModRange, camModMage;
  };
