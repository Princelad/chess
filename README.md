# C++ Multiplayer Chess

A network multiplayer chess game in C++17 with an SFML GUI. No chess engine or
AI - just complete chess rules plus multiplayer over TCP.

## Stack

- C++17, CMake `>= 3.16`
- SFML **3.1.0** (`graphics`, `window`, `network` modules)
- GoogleTest **v1.17.0** (via `FetchContent`) run through CTest
- Board representation: 0x88 mailbox array

## Prerequisites

### CMake and a compiler

- CMake `>= 3.16`
- A C++17 compiler (GCC, Clang)

### SFML 3

The build first tries `find_package(SFML 3 COMPONENTS graphics window network)`.
If SFML 3 is not found, it falls back to building SFML **3.1.0** from source via
`FetchContent`.

Install a system SFML 3 if your distro ships it:

```sh
# Debian / Ubuntu
sudo apt install libsfml-dev
```

```sh
# Fedora (note: ships SFML 2.x, which is too old - the fallback will kick in)
sudo dnf install SFML-devel
```

If the fallback build is used, you additionally need the development headers for
SFML's dependencies (Fedora names shown):

```sh
sudo dnf install freetype-devel libX11-devel libXrandr-devel \
    libXcursor-devel libXi-devel systemd-devel
```

Debian/Ubuntu equivalents: `libfreetype-dev libx11-dev libxrandr-dev
libxcursor-dev libxi-dev libudev-dev`.

The `SFML_USE_SYSTEM_DEPS` option is forced `OFF` for the fallback build so the
remaining bundled dependencies (FreeType, HarfBuzz, MbedTLS, libssh2) are
fetched automatically.

### GoogleTest

Fetched automatically via `FetchContent` (tag `v1.17.0`). Requires network
access on the first configure.

## Building

```sh
cmake -S . -B build
cmake --build build
```

Artifacts:

- `build/server/chess-server`
- `build/client/chess-client`
- `build/tests/chess-tests`

## Running the tests

```sh
ctest --test-dir build
```

## Project layout

- `core/` - chess rules engine (pure C++, no I/O)
- `net/` - shared network protocol
- `server/` - headless match server
- `client/` - SFML GUI client
- `assets/pieces/` - piece sprites
- `tests/` - GoogleTest suite

## License and attribution

Piece sprites are the lichess "cburnett" set
(`https://github.com/lichess-org/lila`, `public/piece/cburnett/`), rasterized
from the source SVGs, licensed under
[CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/).
Original piece set by Colin M.L. Burnett.
