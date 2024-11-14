#ifndef PIEWHEELMENU_H
#define PIEWHEELMENU_H

#include <SDL2/SDL.h>

class PieWheelMenu {
public:
    PieWheelMenu(SDL_Renderer* renderer, int screenWidth, int screenHeight);
    void draw(int centerX, int centerY, int radius, int selectedOption);
    void handleControllerInput(SDL_GameController* controller, int& selectedOption);

private:
    SDL_Renderer* renderer;
    int screenWidth;
    int screenHeight;
    const int numOptions = 4; // Number of options in the pie menu
};

#endif // PIEWHEELMENU_H
