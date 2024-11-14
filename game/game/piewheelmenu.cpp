#include "piewheelmenu.h"
#include <cmath>
#include <iostream>

#define PI 3.14159265

// Constructor
PieWheelMenu::PieWheelMenu(SDL_Renderer* renderer, int screenWidth, int screenHeight)
    : renderer(renderer), screenWidth(screenWidth), screenHeight(screenHeight) {}

void PieWheelMenu::draw(int centerX, int centerY, int radius, int selectedOption) {
    const float anglePerOption = 360.0f / numOptions; // Divide the circle into 4 equal sections

    for (int i = 0; i < numOptions; ++i) {
        float startAngle = anglePerOption * i;
        float endAngle = startAngle + anglePerOption;

        // Color the options differently based on selection
        if (i == selectedOption) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);  // Highlight selected option (green)
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);  // Unselected option (white)
        }

        for (float angle = startAngle; angle < endAngle; angle += 0.1f) {
            float rad = angle * PI / 180.0f; // Convert to radians
            int x = centerX + static_cast<int>(cos(rad) * radius);
            int y = centerY + static_cast<int>(sin(rad) * radius);
            SDL_RenderDrawLine(renderer, centerX, centerY, x, y);
        }
    }

    // Optionally draw the center of the pie
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black center
    SDL_RenderFillCircle(renderer, centerX, centerY, 5); // Small circle at center
}

void PieWheelMenu::handleControllerInput(SDL_GameController* controller, int& selectedOption) {
    const int DEADZONE = 8000;
    int rightX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
    int rightY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

    // Calculate angle from joystick position
    if (abs(rightX) > DEADZONE || abs(rightY) > DEADZONE) {
        float angle = atan2f(static_cast<float>(rightY), static_cast<float>(rightX));
        angle = angle * 180.0f / static_cast<float>(PI); // Convert to degrees

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
    }
}
