#!/bin/sh
# Build without make:  sh build.sh        (add "gui" for window support, needs raylib)
set -e
cd "$(dirname "$0")"
if [ "$1" = "gui" ]; then
  if ! g++ -O2 -std=c++17 src/main.cpp src/gui/gui_raylib.cpp -o fa -lraylib -lGL -lm -lpthread -ldl -lrt -lX11; then
    echo ""
    echo "Building with windows failed. raylib is probably missing - install it with:  sh install-raylib.sh"
    exit 1
  fi
else
  g++ -O2 -std=c++17 src/main.cpp src/gui/gui_none.cpp -o fa
fi
echo "built ./fa"
if [ "$1" = "gui" ]; then
  echo ""
  echo "Start it with the ./ in front, so you use THIS build:   ./fa examples/gui.fa"
  echo "To use it from every folder as plain 'fa':                sh install.sh gui"
fi
