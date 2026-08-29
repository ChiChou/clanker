# airdrop-cli (Objective-C)

A command-line tool for sharing local files and web URLs with Apple devices through AirDrop.

This project is an Objective-C rewrite of
[vldmrkl/airdrop-cli](https://github.com/vldmrkl/airdrop-cli), which is written in Swift. It builds
with Apple's Command Line Tools, so a full Xcode installation is not required.

## Requirements

- macOS 10.13 or newer
- Xcode Command Line Tools (`xcode-select --install`)
- GNU Make (the `make` included with macOS is sufficient)

## Build

From this directory:

```sh
make
```

Or from the repository root:

```sh
make -C airdrop
```

The executable is written to `build/airdrop`.

To build a universal binary for both Apple silicon and Intel Macs:

```sh
make universal
```

## Test

```sh
make test
```

## Install

With Homebrew:

```sh
brew install chichou/tap/airdrop
```

Or build and install from source:

```sh
sudo make install
```

The default destination is `/usr/local/bin/airdrop`. Override it with `PREFIX`, for example:

```sh
make PREFIX="$HOME/.local" install
```

Tags in the form `airdrop-vMAJOR.MINOR.PATCH` publish a universal macOS archive and its SHA-256
checksum to this repository's GitHub Releases for use by the Homebrew formula.

## Usage

```sh
airdrop document.pdf
airdrop image1.jpg image2.png
airdrop file.txt https://apple.com/
```

Use `airdrop --help` for the complete command summary.
