#include "piewheelmenu.h"
#include <cmath>
#include <iostream>
#include <SDL2/SDL.h>

void renderRadialMenu(SDL_Renderer* renderer, int selectedOption) {
    const int centerX = 400;  // Center of the radial menu
    const int centerY = 300;
    const int radius = 100;   // Radius of the menu
    const int optionCount = 4; // Number of options

    // Define the angles for each option (in degrees)
    float angles[] = { 0, 90, 180, 270 };

    // Set color for the radial menu (can be customized)
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // Blue for the menu

    // Draw the circular sections for the options
    for (int i = 0; i < optionCount; i++) {
        float angleStart = angles[i] * (M_PI / 180.0f); // Convert degrees to radians
        float angleEnd = angles[(i + 1) % optionCount] * (M_PI / 180.0f);

        // Draw the arc for each option (just as a section of a circle)
        for (float angle = angleStart; angle < angleEnd; angle += 0.05f) {  // Adjust step size for smoother arcs
            int x = centerX + static_cast<int>(radius * cos(angle));
            int y = centerY + static_cast<int>(radius * sin(angle));
            SDL_RenderDrawLine(renderer, centerX, centerY, x, y);
        }
    }

    // Highlight the selected option
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red for the selected option
    float selectedAngleStart = angles[selectedOption] * (M_PI / 180.0f);
    float selectedAngleEnd = angles[(selectedOption + 1) % optionCount] * (M_PI / 180.0f);

    // Highlight the selected section
    for (float angle = selectedAngleStart; angle < selectedAngleEnd; angle += 0.05f) {  // Smooth arc drawing
        int x = centerX + static_cast<int>(radius * cos(angle));
        int y = centerY + static_cast<int>(radius * sin(angle));
        SDL_RenderDrawLine(renderer, centerX, centerY, x, y);
    }

    // Optional: Add text or icons to represent the options
    // You can use SDL_ttf for rendering text or images for the options
}
