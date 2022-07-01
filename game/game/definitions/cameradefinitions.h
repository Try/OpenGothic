#pragma once

#include <phoenix/ext/daedalus_classes.hh>

class CameraDefinitions final {
  public:
    CameraDefinitions();

    const phoenix::daedalus::c_camera& dialogCam()    const { return *camModDialog; }
    const phoenix::daedalus::c_camera& inventoryCam() const { return *camModInventory; }
    const phoenix::daedalus::c_camera& stdCam()       const { return *camModNormal; }
    const phoenix::daedalus::c_camera& fpCam()        const { return *camModFp; }
    const phoenix::daedalus::c_camera& backCam()      const { return *camModBack; }
    const phoenix::daedalus::c_camera& meleeCam()     const { return *camModMelee;  }
    const phoenix::daedalus::c_camera& rangeCam()     const { return *camModRange;  }
    const phoenix::daedalus::c_camera& mageCam()      const { return *camModMage;   }
    const phoenix::daedalus::c_camera& deathCam()     const { return *camModDeath;  }
    const phoenix::daedalus::c_camera& swimCam()      const { return *camModSwim;   }
    const phoenix::daedalus::c_camera& diveCam()      const { return *camModDive;   }
    const phoenix::daedalus::c_camera& mobsiCam(std::string_view tag, std::string_view pos) const;

  private:
    phoenix::daedalus::c_camera* getCam(std::string_view name);

    std::vector<std::pair<std::string, std::shared_ptr<phoenix::daedalus::c_camera>>> cameras;
    const phoenix::daedalus::c_camera* find(std::string_view name) const;

    phoenix::daedalus::c_camera* camModNormal, *camModFp, *camModBack;
    phoenix::daedalus::c_camera* camModDialog, *camModInventory, *camModDeath, *camModSwim, *camModDive;
    phoenix::daedalus::c_camera* camModMelee, *camModRange, *camModMage;
  };
