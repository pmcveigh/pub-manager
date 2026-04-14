# Pub Manager Prototype

Minimal top-down pub management simulation prototype built with C++ and SDL2.

## Controls

- `Up / Down`: Adjust drink price before opening.
- `Enter`: Start day.
- `M`: Toggle customer mood overlay during the day.
- `R`: Restart after end-of-day summary.
- `Esc`: Quit.

## Build and run (Linux)

Requirements:

- CMake 3.15+
- C++17 compiler
- SDL2 development package (`libsdl2-dev` on Debian/Ubuntu)

```bash
cmake -S . -B build
cmake --build build -j
./build/pub_manager
```
