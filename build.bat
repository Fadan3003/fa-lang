@echo off
REM Windows build (needs MinGW g++). Add "gui" to include gui.window via raylib.
if "%1"=="gui" (
  g++ -O2 -std=c++17 src/main.cpp src/gui/gui_raylib.cpp -o fa.exe -lraylib -lopengl32 -lgdi32 -lwinmm -lws2_32 -Wl,--stack,67108864
) else (
  g++ -O2 -std=c++17 src/main.cpp src/gui/gui_none.cpp -o fa.exe -lws2_32 -Wl,--stack,67108864
)
