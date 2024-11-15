#include "playercontrol.h"
#include "gamepad.h"

PlayerControl::PlayerControl() {
    // Initialize controller if available
    if (SDL_NumJoysticks() > 0) {
        controller = SDL_GameControllerOpen(0);
        if (controller) {
            controllerDetected = true;
            std::cout << "Controller detected: " << SDL_JoystickNameForIndex(0) << std::endl;
            SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");  // Load mappings
        } else {
            std::cerr << "Unable to open controller: " << SDL_GetError() << std::endl;
        }
    }
}

PlayerControl::~PlayerControl() {
    if (controller) {
        SDL_GameControllerClose(controller);
    }
}

void PlayerControl::handleControllerInput() {
    const int DEADZONE = 8000;

    if (!controllerDetected) {
        std::cerr << "No controller detected!" << std::endl;
        return;
    }

    // Process movement using left stick
    int leftX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    int leftY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);

    // Left Stick Horizontal - Strafe
    if (std::abs(leftX) > DEADZONE) {
        if (leftX > 0) {
            movement.strafeRightLeft.main[0] = true;
            movement.strafeRightLeft.reverse[0] = false;
        } else {
            movement.strafeRightLeft.reverse[0] = true;
            movement.strafeRightLeft.main[0] = false;
        }
    } else {
        movement.strafeRightLeft.reset();
    }

    // Left Stick Vertical - Forward/Backward
    if (std::abs(leftY) > DEADZONE) {
        if (leftY > 0) {
            movement.forwardBackward.reverse[0] = true;
            movement.forwardBackward.main[0] = false;
        } else {
            movement.forwardBackward.main[0] = true;
            movement.forwardBackward.reverse[0] = false;
        }
    } else {
        movement.forwardBackward.reset();
    }

    // Handle D-pad and button presses
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

    // Handle radial menu with right stick and selection
    menuActive = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSTICK);
    if (menuActive) {
        int rightX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
        int rightY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

        if (std::abs(rightX) > DEADZONE || std::abs(rightY) > DEADZONE) {
            float angle = std::atan2(static_cast<float>(rightY), static_cast<float>(rightX)) * 180.0f / M_PI;
            angle = angle < 0 ? angle + 360.0f : angle;

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
}
