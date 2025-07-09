CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude
LDFLAGS = -lncurses -lfmt -lsqlite3

SRC = src/main.cpp src/library.cpp
OUT = build/musicplayer

all:
	$(CXX) $(CXXFLAGS) -o $(OUT) $(SRC) $(LDFLAGS)

clean:
	rm -f $(OUT)
