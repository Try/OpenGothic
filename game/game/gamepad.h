#include <SDL2/SDL.h>

class Gamepad {
public:
    Gamepad(SDL_Renderer* renderer, SDL_Window* window);
    void handleInput(SDL_GameController* controller, MovementData& movement);

private:
    SDL_Renderer* renderer;
    SDL_Window* window;
    int screenWidth, screenHeight;
};
