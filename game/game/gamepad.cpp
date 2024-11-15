#include "playercontrol.h"
#include "gamepad.h"

void PlayerControl::handleControllerInput() {
    // Detect controller only once
    if (SDL_NumJoysticks() < 1) {
        std::cerr << "No joystick or controller detected!" << std::endl;
        return;
    }

    // Only print the message once when the controller is detected
    if (!controllerDetected) {
        std::cout << "Controller detected: " << SDL_JoystickNameForIndex(0) << std::endl;
        controllerDetected = true;
    }

    // Load controller mappings
    if (SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt") < 0) {
        std::cerr << "Failed to load controller mappings: " << SDL_GetError() << std::endl;
    }

    // Open the controller
    SDL_GameController* controller = SDL_GameControllerOpen(0);
    if (controller == nullptr) {
        std::cerr << "Unable to open controller: " << SDL_GetError() << std::endl;
        return;
    }

    const int DEADZONE = 8000;

    // Process movement using left stick
    int leftX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    int leftY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);

    if (abs(leftX) > DEADZONE) {
        if (leftX > 0) {
            movement.strafeRightLeft.main[0] = true;
        } else {
            movement.strafeRightLeft.reverse[0] = true;
        }
    } else {
        movement.strafeRightLeft.reset();
    }

    if (abs(leftY) > DEADZONE) {
        if (leftY > 0) {
            movement.forwardBackward.reverse[0] = true;
        } else {
            movement.forwardBackward.main[0] = true;
        }
    } else {
        movement.forwardBackward.reset();
    }

    // Handle D-pad and A/B buttons
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP)) {
        onKeyPressed(Action::WeaponMele, Tempest::KeyEvent::K_Space, KeyCodec::Mapping::Primary);
    }
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
        onKeyPressed(Action::WeaponBow, Tempest::KeyEvent::K_Space, KeyCodec::Mapping::Primary);
    }
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
        onKeyPressed(static_cast<Action>(currentMagicSlot), Tempest::KeyEvent::K_Space, KeyCodec::Mapping::Primary);
        currentMagicSlot = (currentMagicSlot >= Action::WeaponMage10) ? Action::WeaponMage3 : currentMagicSlot + 1;
    }
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A)) {
        onKeyPressed(KeyCodec::Action::Weapon, Tempest::KeyEvent::K_Return, KeyCodec::Mapping());
    }
    ctrl[Action::Jump] = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B);

    // Handle menu activation and selection with right stick
    menuActive = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSTICK);
    if (menuActive) {
        int rightX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
        int rightY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

        if (abs(rightX) > DEADZONE || abs(rightY) > DEADZONE) {
            float angle = atan2f(static_cast<float>(rightY), static_cast<float>(rightX)) * 180.0f / static_cast<float>(M_PI);
            if (angle < 0) angle += 360.0f;
            selectedOption = (angle < 45 || angle >= 315) ? 0 : (angle < 135) ? 1 : (angle < 225) ? 2 : 3;
            std::cout << "Selected Option: " << selectedOption << std::endl;
        }
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A)) {
            std::cout << "Option " << selectedOption + 1 << " selected" << std::endl;
        }
    }

    // Handle shoulder buttons for strafing
    movement.strafeRightLeft.reverse[0] = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    movement.strafeRightLeft.main[0] = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

    SDL_GameControllerClose(controller);
}
