#include "entities.hpp"
#include "config.hpp"

#include <raylib.h>

int main() {
    InitWindow(Config::ScreenWidth, Config::ScreenHeight, "Top-Down Pub Manager Prototype");
    SetTargetFPS(60);

    Game game;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        game.Update(dt);
        game.Draw();
    }

    CloseWindow();
    return 0;
}
