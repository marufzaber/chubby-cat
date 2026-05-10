# Top-level Makefile for chubby-cat.
# Use this if you don't have CMake; otherwise CMakeLists.txt does the same job.

CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
LDFLAGS  ?= -framework Hypervisor

SRC := src/vm.cpp src/vcpu.cpp src/main.cpp
OBJ := $(SRC:.cpp=.o)

BUILD_DIR    := build
TARGET       := $(BUILD_DIR)/chubby-cat
ENTITLEMENTS := chubby-cat.entitlements

.PHONY: all clean example
all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $@

$(TARGET): $(SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Isrc $(SRC) $(LDFLAGS) -o $@
	codesign --sign - --entitlements $(ENTITLEMENTS) --force $@

example:
	$(MAKE) -C examples

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C examples clean
