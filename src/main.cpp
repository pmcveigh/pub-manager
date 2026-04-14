#include "config.hpp"
#include "entities.hpp"

#include <SDL2/SDL.h>

int main() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Top-Down Pub Manager Prototype",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        Config::ScreenWidth,
        Config::ScreenHeight,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Game game;
    bool running = true;
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    double frequency = static_cast<double>(SDL_GetPerformanceFrequency());

    while (running) {
        InputState input;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE) running = false;
                if (key == SDLK_UP) input.upPressed = true;
                if (key == SDLK_DOWN) input.downPressed = true;
                if (key == SDLK_RETURN) input.enterPressed = true;
                if (key == SDLK_m) input.toggleOverlayPressed = true;
                if (key == SDLK_r) input.restartPressed = true;
            }
        }

        Uint64 currentCounter = SDL_GetPerformanceCounter();
        float dt = static_cast<float>((currentCounter - previousCounter) / frequency);
        previousCounter = currentCounter;

        game.Update(dt, input);
        game.Draw(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
