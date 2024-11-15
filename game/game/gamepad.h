// gamepad.h
#pragma once

#include <SDL2/SDL.h>

class PlayerControl;

class Gamepad {
public:
    explicit Gamepad(PlayerControl& playerControl);
    ~Gamepad();

    void handleControllerInput();

private:
    PlayerControl& playerControl;
    SDL_GameController* controller = nullptr;

    bool controllerDetected = false;
    bool menuActive = false;
    int selectedOption = 0;
    int currentMagicSlot = 0;
};
