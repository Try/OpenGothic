#include "Gamepad.h"
#include <iostream>
#include <SDL2/SDL.h>
#include "piewheelmenu.h"  // Include the PieWheelMenu if you want to call it here

#define DEADZONE 8000  // Deadzone threshold for analog sticks

Gamepad::Gamepad(SDL_Renderer* renderer, SDL_Window* window)
    : renderer(renderer), window(window) {
    // Get the window size
    SDL_GetWindowSize(window, &screenWidth, &screenHeight);
}

void Gamepad::handleInput(SDL_GameController* controller, MovementData& movement) {
    // Get the screen dimensions from the window
    SDL_GetWindowSize(window, &screenWidth, &screenHeight);
    std::cout << "Screen Width: " << screenWidth << ", Screen Height: " << screenHeight << std::endl;
    
    static bool controllerDetected = false;  // Static flag to track if controller is already detected
    static bool menuActive = false; // Track whether the radial menu is active
    static int selectedOption = 0;  // Index of the selected menu option

    // Detect the controller only once
    if (SDL_NumJoysticks() < 1) {
        std::cerr << "No joystick or controller detected!" << std::endl;
        return;
    }

    // Only print the message the first time the controller is detected
    if (!controllerDetected) {
        std::cout << "Controller detected: " << SDL_JoystickNameForIndex(0) << std::endl;
        controllerDetected = true;
    }

    // Check if Left Stick (L3) is pressed to activate the menu
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSTICK)) {
        menuActive = true;
    } else {
        menuActive = false;
    }

    if (menuActive) {
        // Menu is active, let's display the radial menu and handle navigation
        PieWheelMenu pieMenu(renderer, screenWidth, screenHeight);
        pieMenu.draw(screenWidth / 2, screenHeight / 2, 100, selectedOption);
        pieMenu.handleControllerInput(controller, selectedOption);

        // Get the right joystick values (right analog stick)
        int rightX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
        int rightY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

        // Only process the right joystick movement if it exceeds the deadzone
        if (abs(rightX) > DEADZONE || abs(rightY) > DEADZONE) {
            // Calculate the angle of the right joystick movement
            float angle = atan2f(static_cast<float>(rightY), static_cast<float>(rightX)); // Get the angle in radians

            // Normalize the angle to degrees (0° to 360°)
            angle = angle * 180.0f / static_cast<float>(M_PI);

            // Determine the selected option based on the angle
            if (angle >= -45 && angle < 45) {
                selectedOption = 0; // Option 1 (right)
            } else if (angle >= 45 && angle < 135) {
                selectedOption = 1; // Option 2 (down)
            } else if (angle >= 135 || angle < -135) {
                selectedOption = 2; // Option 3 (left)
            } else if (angle >= -135 && angle < -45) {
                selectedOption = 3; // Option 4 (up)
            }

            // Optional: Visualize the selected option in the console (or UI)
            std::cout << "Selected Option: " << selectedOption << std::endl;
        }

        // Check if the user selects an option (e.g., pressing the A button)
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A)) {
            // Execute the action for the selected option
            switch (selectedOption) {
                case 0:
                    std::cout << "Option 1 selected" << std::endl;
                    // Trigger corresponding action for option 1
                    break;
                case 1:
                    std::cout << "Option 2 selected" << std::endl;
                    // Trigger corresponding action for option 2
                    break;
                case 2:
                    std::cout << "Option 3 selected" << std::endl;
                    // Trigger corresponding action for option 3
                    break;
                case 3:
                    std::cout << "Option 4 selected" << std::endl;
                    // Trigger corresponding action for option 4
                    break;
            }

            // Reset menu state after selection
            menuActive = false;
        }
    }

    // Movement input handling
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) {
        movement.strafeRightLeft.reverse[0] = true;
    } else {
        movement.strafeRightLeft.reverse[0] = false;
    }

    // Check if the right shoulder button is pressed
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
        movement.strafeRightLeft.main[0] = true;
    } else {
        movement.strafeRightLeft.main[0] = false;
    }

    // Optionally, add more movement logic using analog sticks or buttons
    int leftX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    int leftY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);

    // Handle movement if the analog sticks are pushed past the deadzone
    if (abs(leftX) > DEADZONE) {
        if (leftX > 0) {
            movement.strafeRightLeft.main[0] = true;
        } else {
            movement.strafeRightLeft.reverse[0] = true;
        }
    }
    
    if (abs(leftY) > DEADZONE) {
        if (leftY > 0) {
            movement.forwardBackward.reverse[0] = true;
        } else {
            movement.forwardBackward.main[0] = true;
        }
    }
}
