// PlayerControl.h
#pragma once

#include <SDL2/SDL.h>
#include <iostream>
#include <cmath>
#include "utils/keycodec.h"
#include "playercontrol.h"

class PlayerControl {
public:
    void handleControllerInput();

private:
    bool controllerDetected = false;   // Track if controller is detected
    bool menuActive = false;           // Track if the radial menu is active
    int selectedOption = 0;            // Index of the selected menu option
    int currentMagicSlot = Action::WeaponMage3; // Track current magic slot
};
