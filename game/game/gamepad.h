#include <SDL2/SDL.h>
#include "playercontrol.h"  // Include the MovementData header

class Gamepad {
public:
    Gamepad(SDL_Renderer* renderer, SDL_Window* window);
    void handleInput(SDL_GameController* controller, MovementData& movement);

private:
    SDL_Renderer* renderer;
    SDL_Window* window;
    int screenWidth, screenHeight;
};
