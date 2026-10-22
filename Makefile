# Wrapper over CMakePresets.json. Select a preset with `make PRESET=release`.
PRESET ?= debug
BUILD_DIR := build/$(PRESET)

.PHONY: all build test clean

all: build

# CMake regenerates itself on CMakeLists.txt changes, but not on preset changes.
$(BUILD_DIR)/CMakeCache.txt: CMakePresets.json
	cmake --preset $(PRESET)

build: $(BUILD_DIR)/CMakeCache.txt
	cmake --build --preset $(PRESET)

test: build
	ctest --preset $(PRESET)

clean:
	cmake -E rm -rf $(BUILD_DIR)
