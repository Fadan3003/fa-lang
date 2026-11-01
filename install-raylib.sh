#!/bin/sh
# Installs raylib (needed only for windows on screen).  Run once:   sh install-raylib.sh
# Afterwards:   sh build.sh gui      or      sh install.sh gui
set -e

SUDO=""
if [ "$(id -u)" -ne 0 ]; then SUDO="sudo"; fi

if command -v apt-get >/dev/null 2>&1; then
  echo "Installing build tools (needs your password)..."
  $SUDO apt-get update
  $SUDO apt-get install -y build-essential git cmake libx11-dev libgl1-mesa-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev
else
  echo "This script knows apt (Ubuntu/Debian). On other systems install: git, cmake, a C compiler and the X11/OpenGL dev packages."
fi

dir=$(mktemp -d)
echo "Downloading and building raylib (takes a minute)..."
git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git "$dir/raylib"
cmake -S "$dir/raylib" -B "$dir/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF -DBUILD_SHARED_LIBS=OFF
cmake --build "$dir/build" -j"$(nproc 2>/dev/null || echo 2)"
$SUDO cmake --install "$dir/build"
$SUDO ldconfig 2>/dev/null || true
rm -rf "$dir"

echo ""
echo "raylib is installed. Now run:  sh install.sh gui"
