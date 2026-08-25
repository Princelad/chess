# C++ Multiplayer Chess

A network multiplayer chess game in C++17 with an SFML GUI. No chess engine or
AI — just complete chess rules plus multiplayer over TCP.

## Quick Start

```sh
cmake -S . -B build && cmake --build build   # build
./build/server/chess-server                   # start server (port 5555)
./build/client/chess-client                   # start client — repeat in a second terminal
```

Open a second terminal and run `chess-client` again. Both players enter a name
and press Enter to connect. The server pairs them automatically.

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
# Fedora (note: ships SFML 2.x, which is too old — the fallback will kick in)
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

### CMake Presets

Presets are available for common configurations:

```sh
cmake --preset default          # Debug build (default)
cmake --preset release          # Release build (-O2 -DNDEBUG)
cmake --preset ci               # Release + strict warnings
cmake --build --preset default  # build with preset
```

## Running the tests

```sh
ctest --test-dir build
```

## Server Usage

```
chess-server [options]
  --port <N>        Listen port (default: 5555)
  --host <addr>     Bind address (default: 0.0.0.0)
  --max-clients <N> Max simultaneous clients (default: 64)
  --timeout <secs>  Idle timeout in seconds (default: 0 = disabled)
  --log-file <path> Log to file in addition to stdout
  --log-level <L>   Min log level: info, warn, error (default: info)
  --help            Show this help
```

The server is headless — no GUI, no game state display. It listens for TCP
connections, pairs players into matches, validates moves, and relays messages.

## Client Usage

```
chess-client
```

The client takes no command-line arguments. On launch it shows the Connect
Screen where you enter the server address, port, and your name.

## Controls

| Action | How |
|---|---|
| Move a piece | Click the piece, then click the destination square |
| Promote a pawn | Click-click the pawn to the last rank, then pick a piece from the popup |
| Offer a draw | Click the "Draw" button in the right panel |
| Resign | Click the "Resign" button in the right panel |
| Send chat | Click the chat input at the bottom-right, type, press Enter |
| Cancel selection | Press Escape |
| Switch input fields | Tab (on Connect Screen) |

## Project Layout

- `core/` — chess rules engine (pure C++, no I/O)
- `net/` — shared network protocol
- `server/` — headless match server
- `client/` — SFML GUI client
- `assets/pieces/` — piece sprites
- `assets/fonts/` — Inter font files
- `tests/` — GoogleTest suite

## License and attribution

Piece sprites are the lichess "cburnett" set
(`https://github.com/lichess-org/lila`, `public/piece/cburnett/`), rasterized
from the source SVGs, licensed under
[CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/).
Original piece set by Colin M.L. Burnett.
