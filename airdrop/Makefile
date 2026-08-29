PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin
BUILD_DIR := build
TARGET := $(BUILD_DIR)/airdrop
SOURCES := Sources/main.m Sources/AirDrop.m
CC := xcrun clang
CFLAGS := -fobjc-arc -fblocks -Wall -Wextra -Wpedantic -mmacosx-version-min=10.13
FRAMEWORKS := -framework Foundation -framework AppKit
UNIVERSAL_CFLAGS := $(CFLAGS) -arch arm64 -arch x86_64

all: build

build: $(TARGET)

$(TARGET): $(SOURCES) Sources/AirDrop.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SOURCES) $(FRAMEWORKS) -o $(TARGET)

test: build
	sh Tests/smoke-test.sh "$(abspath $(TARGET))"

universal:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(UNIVERSAL_CFLAGS)" build
	lipo "$(TARGET)" -verify_arch arm64
	lipo "$(TARGET)" -verify_arch x86_64

install: build
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 "$(TARGET)" "$(DESTDIR)$(BINDIR)/airdrop"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/airdrop"

clean:
	rm -rf "$(BUILD_DIR)"

.PHONY: all build test universal install uninstall clean
