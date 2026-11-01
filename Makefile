CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall
HEADERS := $(wildcard src/*.h) src/gui/gui.h

fa: src/main.cpp src/gui/gui_none.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) src/main.cpp src/gui/gui_none.cpp -o fa

# adds gui.window(...) support; needs raylib installed
gui: src/main.cpp src/gui/gui_raylib.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) src/main.cpp src/gui/gui_raylib.cpp -o fa -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

# copies fa to /usr/local/bin so you can type:  fa program.fa
install: fa
	install -m 755 fa /usr/local/bin/fa

clean:
	rm -f fa fa.exe
