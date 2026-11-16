BUILD_DIR ?= build
BUILD_TYPE ?= Release
PREFIX ?= $(if $(filter Haiku,$(shell uname -s)),/boot/system/non-packaged,/usr/local)

.PHONY: all configure build test install

all: build

configure:
	cmake -S . -B "$(BUILD_DIR)" \
		-DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" \
		-DCMAKE_INSTALL_PREFIX="$(PREFIX)"

build: configure
	cmake --build "$(BUILD_DIR)"

test: build
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure

install: build
	DESTDIR="$(DESTDIR)" cmake --install "$(BUILD_DIR)"
