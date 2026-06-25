CXX      := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
LDFLAGS  :=

SERVER_SRCS := src/server.cpp src/store.cpp src/pubsub.cpp \
               src/parser.cpp src/commands.cpp src/client.cpp src/persistence.cpp
SERVER_OBJS := $(SERVER_SRCS:src/%.cpp=build/%.o)
SERVER_BIN  := build/redis_clone

CLI_BIN := build/redis_cli

all: $(SERVER_BIN) $(CLI_BIN)

$(SERVER_BIN): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(CLI_BIN): build/cli.o | build
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -Isrc -c -o $@ $<

build:
	mkdir -p build

clean:
	rm -rf build

.PHONY: all clean
