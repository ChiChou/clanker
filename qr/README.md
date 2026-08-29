# qr

A tiny dependency-free C tool that renders QR codes as bitmaps directly in the terminal. It supports
iTerm2 inline images and the kitty graphics protocol, including compatible terminals such as WezTerm
and Windows Terminal.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

From the repository root, use `cmake -S qr -B build/qr` instead.

## Usage

```sh
qr "https://example.com"
echo "hello" | qr
qr --version
```

Set `QR_PROTOCOL=iterm2` or `QR_PROTOCOL=kitty` to override terminal auto-detection.

Tags in the form `qr-vMAJOR.MINOR.PATCH` publish prebuilt macOS universal and Linux x86_64 archives
and their SHA-256 checksums to this repository's GitHub Releases.
