#!/bin/sh
# Build and run the display simulator: writes sim/out/frames.json
set -e
cd "$(dirname "$0")"
mkdir -p out
g++ -std=c++17 -O1 -Wall -Wextra -I../include -I../src sim.cpp ../src/view.cpp ../src/canvas.cpp ../src/critters.cpp -o out/sim
./out/sim "$@" > out/frames.json
echo "wrote sim/out/frames.json"
