#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <SDL2/SDL.h>

class Gamepad {
public:
    Gamepad(SDL_Renderer* renderer, int screenWidth, int screenHeight);
    void handleInput(SDL_GameController* controller);
    
private:
    SDL_Renderer* renderer;
    int screenWidth;
    int screenHeight;

    // Add additional variables if needed
};

#endif // GAMEPAD_H
