# Pub Manager Prototype

Minimal top-down pub management simulation prototype built with C++ and raylib.

## Features

- Single-room pub map with entrance, bar, tables, standing area, toilets, and exit.
- Customer state machine: enter, queue, get served, sit/stand, socialize, optional toilet, reorder, leave.
- Mood system impacted by queue time, seat availability, dirt, price pressure, and stock outages.
- Bartender serves queue one-by-one.
- Cleaner seeks and removes mess over time.
- Dirt entities spawned by customers and incidents.
- Random incidents: spills, blocked toilet, arguments, stock shortages.
- Drink stock and adjustable price before opening.
- End-of-day accounting and summary screen.

## Controls

- `Up / Down`: Adjust drink price before opening.
- `Enter`: Start day.
- `M`: Toggle customer mood overlay during the day.
- `R`: Restart after end-of-day summary.
- `Esc`: Quit.

## Build (Linux)

Prerequisites:

- CMake 3.15+
- C++17 compiler
- raylib 5.x development package

Build:

```bash
cmake -S . -B build
cmake --build build -j
```

Run:

```bash
./build/pub_manager
```
