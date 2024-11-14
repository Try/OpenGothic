#include "Gamepad.h"
#include <iostream>
#include <SDL2/SDL.h>
#include "piewheelmenu.h"  // Include the PieWheelMenu if you want to call it here

#define DEADZONE 8000  // Deadzone threshold for analog sticks

Gamepad::Gamepad(SDL_Renderer* renderer, int screenWidth, int screenHeight)
    : renderer(renderer), screenWidth(screenWidth), screenHeight(screenHeight) {}

void Gamepad::handleInput(SDL_GameController* controller) {
    static bool controllerDetected = false;

    if (SDL_NumJoysticks() < 1) {
        std::cerr << "No joystick or controller detected!" << std::endl;
        return;
    }

    // Handle controller button press for showing pie menu
    int selectedOption = 0;
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) {
        PieWheelMenu pieMenu(renderer, screenWidth, screenHeight);
        pieMenu.draw(screenWidth / 2, screenHeight / 2, 100, selectedOption);
        pieMenu.handleControllerInput(controller, selectedOption);

        // Action based on selected option
        if (selectedOption == 0) {
            // Action for option 1
        } else if (selectedOption == 1) {
            // Action for option 2
        } else if (selectedOption == 2) {
            // Action for option 3
        } else if (selectedOption == 3) {
            // Action for option 4
        }
    }

    // Handle other button inputs here
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A)) {
        std::cout << "Button A pressed!" << std::endl;
        // Action for Button A (example)
    }

    // Handle D-pad and analog stick for movement
    int leftX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    int leftY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);

    if (abs(leftX) > DEADZONE) {
        // Process strafeLeftRight
    }

    if (abs(leftY) > DEADZONE) {
        // Process forwardBackward movement
    }
    
    // Check for other buttons like D-pad, shoulder buttons, etc.
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP)) {
        std::cout << "D-pad Up pressed!" << std::endl;
        // Action for D-pad Up
    }
}
