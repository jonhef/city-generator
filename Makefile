BUILD_DIR ?= build
CMAKE_BUILD_TYPE ?= Release

.PHONY: all configure build test clean

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

build: configure
	cmake --build $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR) citygen compile_commands.json
