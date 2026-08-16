# C++ Multiplayer Chess — Project Task List

A step-by-step plan for building a network multiplayer chess game in C++ with an
SFML GUI. No chess engine / AI — just complete rules + multiplayer.

**Stack decisions (pinned versions)**
- Language: C++17 (dev machine: GCC 16.1.1)
- Build: CMake `>= 3.16` (dev machine: CMake 4.3.0), Linux target
- GUI + networking: SFML **3.1.0** (`find_package(SFML 3 COMPONENTS graphics window network)`)
- Tests: GoogleTest **v1.17.0** via FetchContent, run through CTest
- Board representation: 0x88 mailbox array (didactic, recommended by chessprogramming.org)
- Network: SFML TCP (`sf::TcpListener` / `sf::TcpSocket` / `sf::Packet`) with a small text protocol
- Piece art: lichess "cburnett" PNG set v1 (CC BY-SA 3.0 — include attribution in README)
- Reference: [chessprogramming.org](https://www.chessprogramming.org/Main_Page) wiki

**Conventions**
- `[ ]` = not started, `[x]` = done, `[~]` = in progress
- Each phase has a **Goal**, **Tasks**, **Intermediate steps** per task, and an **Exit version**
- `▶` links point to the wiki page to consult while working

---

## Versioning

SemVer-style tags, one per completed phase. Tag the repo (`git tag v0.x.0 -m "…"`)
once the phase's final task is done and the build is green. Pre-1.0: MINOR bump per
phase, PATCH for bugfixes/hotfixes landed within a phase.

| Phase | Version | Meaning |
|---|---|---|
| 0 — Project setup | `v0.1.0` | Skeleton builds green, deps wired |
| 1 — Core rules engine | `v0.2.0` | Rules engine complete |
| 2 — Core unit tests | `v0.3.0` | Engine verified by perft |
| 3 — Network protocol | `v0.4.0` | Protocol defined + tested |
| 4 — Server | `v0.5.0` | Server hosts matches |
| 5 — Client networking | `v0.6.0` | Client connects |
| 6 — SFML GUI | `v0.7.0` | Playable GUI |
| 7 — Integration & polish | `v1.0.0` | First release |
| 8 — Stretch goals | `v1.x.0` | One MINOR bump per landed stretch item |

---

## Project layout

```
chess/
├── CMakeLists.txt            # top-level: builds core, net, server, client, tests
├── README.md                 # how to build, run, play; license/attribution
├── TODO.md                   # this file
├── .gitignore
├── core/                     # chesscore: pure rules engine (no I/O, no SFML)
│   ├── CMakeLists.txt
│   ├── include/chess/
│   │   ├── types.h           # Color, Piece, Square, enums
│   │   ├── board.h           # 0x88 board + state (castling, en passant, counters)
│   │   ├── move.h            # Move struct + flags
│   │   ├── movegen.h         # pseudo-legal + legal move generation
│   │   ├── fen.h             # FEN parse / emit
│   │   ├── san.h             # SAN parse / format
│   │   └── rules.h           # check/mate/stalemate/draw detection
│   └── src/
│       ├── board.cpp
│       ├── movegen.cpp
│       ├── fen.cpp
│       ├── san.cpp
│       └── rules.cpp
├── net/                      # shared protocol: serialization of messages
│   ├── CMakeLists.txt
│   ├── include/chess/net/messages.h
│   ├── include/chess/net/protocol.h
│   └── src/protocol.cpp
├── server/                   # chess-server binary (headless, no SFML needed...)
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp          # listener loop, CLI args (port)
│       ├── matchmaker.cpp    # queue players, pair into matches, assign colors
│       └── match.cpp         # one game session state machine
├── client/                   # chess-client binary (SFML GUI)
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp          # app entry, SFML window, main loop
│       ├── app.cpp           # screen state machine (menu / game / gameover)
│       ├── connection.cpp    # TCP connection wrapper (non-blocking, queue)
│       ├── boardview.cpp     # pixel<->square mapping, draw squares/pieces/highlights
│       ├── input.cpp         # click-click move input, promotion picker
│       └── hud.cpp           # status text, move history, chat/event log
├── assets/
│   └── pieces/               # 12 PNG sprites (cburnett)
└── tests/
    ├── CMakeLists.txt
    └── core/                 # perft, fen, san, rules tests
```

---

## Phase 0 — Project setup

**Goal:** a buildable CMake skeleton with all dependencies wired so every later
phase compiles standalone.

**Exit version:** `v0.1.0` — skeleton builds green

- [x] **0.1 Create repository scaffolding**
  - [x] Add `.gitignore` (build/, *.o, .cache, etc.)
  - [x] Create directory layout from the tree above
  - [x] Write top-level `CMakeLists.txt` with `project(chess CXX)`, C++17, and `add_subdirectory` for each component
  - [x] Add per-component `CMakeLists.txt` stubs (core, net, server, client, tests)
  - [x] Commit baseline (after 0.5)

- [x] **0.2 Set up dependencies**
  - [x] Detect SFML 3 via `find_package(SFML 3 REQUIRED COMPONENTS graphics window network)` (fall back to building SFML 3.1.0 from source if the distro package is too old)
  - [x] Wire GoogleTest **v1.17.0** via `FetchContent` (tag `v1.17.0`), enable `BUILD_TESTING` + `include(CTest)`
  - [x] Document `apt install libsfml-dev` (or distro equivalent) in README

- [x] **0.3 Verify build**
  - [x] `cmake -S . -B build && cmake --build build`
  - [x] Confirm both SFML and GoogleTest link without errors
  - [x] Make `ctest` run (even with zero tests yet)

- [x] **0.4 Fetch piece assets**
  - [x] Download the 12 cburnett PNG pieces from the lichess `cburnett` set (e.g. `github.com/lichess-org/lila` `public/piece/cburnett/`)
  - [x] Name them consistently: `wp.png wn.png wb.png wr.png wq.png wk.png`, same for black (`bp.png ... bk.png`)
  - [x] Add a README note with CC BY-SA 3.0 attribution

- [x] **0.5 First commit**
  - [x] Commit the skeleton + assets so the repo builds green

---

## Phase 1 — Core chess rules engine (`core/`)

**Goal:** a complete, correct rules engine — move generation, legality, special
moves, game-end detection — with no I/O. Pure C++, tested independently.

**Exit version:** `v0.2.0` — rules engine complete

▶ [Getting Started](https://www.chessprogramming.org/Getting_Started)
▶ [Board Representation](https://www.chessprogramming.org/Board_Representation)

### Task 1.1 — Types and board representation

**Goal:** represent colors, pieces, squares, and a board with game state.

▶ [0x88](https://www.chessprogramming.org/0x88)
▶ [Board Representation](https://www.chessprogramming.org/Board_Representation)

- [x] **1.1.1 Define enums in `types.h`**
  - [x] `enum class Color : int { White, Black, None }`, with `opposite()`
  - [x] `enum class PieceType { Pawn, Knight, Bishop, Rook, Queen, King }`
  - [x] `Piece` = color + type, `Piece::None` for empty square
  - [x] `enum class Square` or 0x88 constants: `squareOf(file, rank)` and `fileOf()/rankOf()`

- [x] **1.1.2 Implement the 0x88 board**
  - [x] 0x88 layout: 16 × 8 = 128 array `board[128]`, a1 = 0, b1 = 1, ..., h8 = 119
  - [x] `bool offBoard(int sq) { return sq & 0x88; }` helper (the whole point of 0x88)
  - [x] `Square` ↔ algebraic helpers: `"e4"` ↔ index, `file`/`rank` ↔ index

- [~] **1.1.3 Game-state struct**
  - [x] Side to move, castling rights (WK/WQ/BK/BQ flags), en-passant target square, halfmove clock, fullmove number
  - [x] Move history stack to support undo (position copies for repetition checks)
  - [~] `Board` class API: `pieceAt(Square)`, `sideToMove()`, `makeMove(Move)`, `undoMove()` — `makeMove`/`undoMove` deferred to 1.4 (need `Move` from 1.2.1)

- [x] **1.1.4 Set up starting position**
  - [x] `Board::fromStartPos()` — place 8 pawns, 4 back-rank pieces per side, kings/queens
  - [x] Sanity test: assert piece counts (16 per side), king on e1/e8

### Task 1.2 — Move representation and FEN

**Goal:** a `Move` struct with flags, and the ability to save/restore any position.

▶ [Forsyth-Edwards Notation](https://www.chessprogramming.org/Forsyth-Edwards_Notation)

- [x] **1.2.1 `Move` struct in `move.h`**
  - [x] Fields: `from`, `to` (0x88 squares), `flags` (quiet, capture, double-push, castle, en passant, promotion)
  - [x] Promotion piece type encoded in move (or 4 promotion moves: n/b/r/q)
  - [x] Encapsulate construction: `move(from, to)`, `promotion(from, to, type)`, `castle(...)`
  - [x] Streaming/pretty print for debugging

- [x] **1.2.2 FEN parser (`fen.h`/`fen.cpp`)**
  - [x] Parse placement field `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR`
  - [x] Parse side-to-move, castling, en-passant, halfmove clock, fullmove number
  - [x] `Board::fromFen(std::string_view)`; handle invalid FEN with an error result

- [x] **1.2.3 FEN emitter**
  - [x] `toFen()` producing a canonical FEN from any board state
  - [x] Round-trip test: `fromFen(toFen(b))` == `b` for many positions

### Task 1.3 — Move generation (pseudo-legal)

**Goal:** generate all candidate moves for a side, ignoring self-check for now.

▶ [Move Generation](https://www.chessprogramming.org/Move_Generation)
▶ [Pawn Moves](https://www.chessprogramming.org/Pawn_Moves) · [Knight Moves](https://www.chessprogramming.org/Knight) · [Bishop](https://www.chessprogramming.org/Bishop) · [Rook](https://www.chessprogramming.org/Rook) · [Queen](https://www.chessprogramming.org/Queen) · [King](https://www.chessprogramming.org/King_Moves)

- [ ] **1.3.1 Generate moves for fixed-step pieces**
  - [ ] Knight: 8 offsets, `!offBoard(to) && !ownPiece(to)`
  - [ ] King: 8 offsets (excluding castling for now)
  - [ ] Pawns: single push, double push from starting rank, diagonal captures, promotions (4 types), en passant

- [ ] **1.3.2 Generate sliding-piece moves**
  - [ ] Rook: walk 4 directions until board edge or blocker
  - [ ] Bishop: same with 4 diagonal directions
  - [ ] Queen: rook + bishop rays combined
  - [ ] Set `capture` flag when landing on enemy piece; stop at first blocker

- [ ] **1.3.3 Castling moves**
  - [ ] Only when castling-right flag set, squares between empty, king/rook unmoved
  - [ ] Generate king-side and queen-side castling as moves (legality of "not in/through check" deferred to Task 1.4)

- [ ] **1.3.4 Assemble `generateMoves(Board)`**
  - [ ] Iterate 0x88 squares, dispatch on piece type, collect into `std::vector<Move>`
  - [ ] Provide a `generateMoves<Filter>(board)` so callers can ask for captures-only later

### Task 1.4 — Make/unmake and legality

**Goal:** only legal moves remain; `makeMove`/`undoMove` maintain full state.

▶ [Move Making](https://www.chessprogramming.org/Move_Making) · [Check](https://www.chessprogramming.org/Check) · [King Attack](https://www.chessprogramming.org/King_Attack)

- [ ] **1.4.1 Implement `makeMove`**
  - [ ] Move piece, handle captures (remove piece, restore on undo)
  - [ ] Handle promotion (replace pawn with chosen piece)
  - [ ] Handle en passant capture (remove the captured pawn, not the target square's piece)
  - [ ] Handle castling (also move the rook)
  - [ ] Update castling rights (king/rook moved or square captured), en-passant target, clocks, side-to-move
  - [ ] Push full previous state onto history stack for `undoMove`

- [ ] **1.4.2 Implement `undoMove`**
  - [ ] Pop history, restore pieces/state exactly (including promotion piece restored to pawn)
  - [ ] Round-trip test: for every generated move, `makeMove` then `undoMove` returns original position

- [ ] **1.4.3 Check detection**
  - [ ] `isAttacked(sq, byColor)`: reuse movegen — is any enemy piece attacking `sq`?
  - [ ] Simplify: scan from `sq` outward (knight offsets, king offsets, rook/bishop/queen rays, pawn attacks)
  - [ ] `inCheck(board, color)` → king square attacked?

- [ ] **1.4.4 Legal move filtering**
  - [ ] Generate pseudo-legal moves, then simulate each with `makeMove` and keep only those where own king is not attacked
  - [ ] King "moves into check", pinned pieces, castling out of/through check all handled for free by the filter
  - [ ] Implement efficient-enough version first; optimization is a stretch goal

### Task 1.5 — Game-end detection and draw rules

**Goal:** know when the game is over and why.

▶ [Checkmate](https://www.chessprogramming.org/Checkmate) · [Stalemate](https://www.chessprogramming.org/Stalemate) · [Threefold Repetition](https://www.chessprogramming.org/Threefold_Repetition) · [Fifty-move Rule](https://www.chessprogramming.org/Fifty-move_Rule) · [Insufficient Material](https://www.chessprogramming.org/Insufficient_Material)

- [ ] **1.5.1 Checkmate / stalemate**
  - [ ] After filtering legal moves: if none and in check → checkmate; if none and not in check → stalemate
  - [ ] Return a `GameState` enum: `Ongoing, Checkmate, Stalemate, Draw`

- [ ] **1.5.2 Draw by repetition**
  - [ ] Track position history (hashed or FEN key per position)
  - [ ] `threefoldRepetition()` — same position 3 times (side to move matters)
  - [ ] Include en-passant and castling-rights state in the "same position" test

- [ ] **1.5.3 Fifty-move rule**
  - [ ] Halfmove clock reaches 100 → draw
  - [ ] Reset clock on pawn move or capture (already done in `makeMove`)

- [ ] **1.5.4 Insufficient material**
  - [ ] K vs K, K+B vs K, K+N vs K (and K+B vs K+B same-color bishops) → draw

- [ ] **1.5.5 Public rules API**
  - [ ] `GameStatus evaluate(Board)` combining all above
  - [ ] `bool isLegalMove(Board, Move)` for server-side validation

### Task 1.6 — SAN (algebraic notation)

**Goal:** parse and format human-readable moves, both for the protocol and the GUI.

▶ [Algebraic Notation](https://www.chessprogramming.org/Algebraic_Notation)

- [ ] **1.6.1 Format moves → SAN (`san::toSan`)**
  - [ ] Disambiguate identical moves (file/rank/both, e.g. `Nbd2`, `R1e3`, `Qh4e1`)
  - [ ] Capture marker `x`, promotion `=Q`, check `+`, checkmate `#`, castling `O-O` / `O-O-O`
  - [ ] En passant in SAN (e.g. `exd6`)

- [ ] **1.6.2 Parse SAN → move (`san::fromSan`)**
  - [ ] Resolve notation against generated legal moves (find the unique matching move)
  - [ ] Handle all the same cases as formatting (disambiguation, promotions, castles)
  - [ ] Return error on ambiguous/illegal input

---

## Phase 2 — Core unit tests

**Goal:** prove the engine is correct before building anything on top of it.

**Exit version:** `v0.3.0` — engine verified by perft

▶ [Perft](https://www.chessprogramming.org/Perft) · [Engine Testing](https://www.chessprogramming.org/Engine_Testing)

- [ ] **2.1 Perft (move-path counting)**
  - [ ] Implement `perft(board, depth)` counting leaf nodes
  - [ ] Verify starting position: `perft(1)=20, perft(2)=400, perft(3)=8902, perft(4)=197281`
  - [ ] Verify the standard "Kiwipete" position (e.g. `r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1`) at depths 1–3
  - [ ] Verify another known position (`8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1`) and the castling/en-passant-heavy position (`r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1`)

- [ ] **2.2 FEN tests**
  - [ ] Round-trip start position and several midgames
  - [ ] Reject malformed FEN strings gracefully

- [ ] **2.3 Special-move tests**
  - [ ] Castling rights granted/revoked correctly (king/rook move, rook captured)
  - [ ] En passant captured pawn removed; ep target square correct timing
  - [ ] All 4 promotions legal and revert on undo

- [ ] **2.4 Endgame/draw tests**
  - [ ] Fool's mate (checkmate in 2), scholar's mate, back-rank mate
  - [ ] Stalemate position, threefold repetition, fifty-move counter, insufficient material
  - [ ] Legal vs illegal: pinned-piece move, king-into-check, castling through check all rejected

- [ ] **2.5 SAN tests**
  - [ ] `toSan`↔`fromSan` round-trips across a game's move list
  - [ ] Disambiguation correctness (`Nbd2` vs `Nfd2`), castling, promotion with check

---

## Phase 3 — Network protocol (`net/`)

**Goal:** a shared, well-tested protocol used identically by server and client.

**Exit version:** `v0.4.0` — protocol defined + tested

- [ ] **3.1 Define message types (`messages.h`)**
  - [ ] Client→Server: `JOIN {name}`, `MOVE {san}`, `DRAW_OFFER`, `DRAW_ACCEPT`, `DRAW_DECLINE`, `RESIGN`, `CHAT {text}`, `PING`
  - [ ] Server→Client: `WELCOME {color} {opponent}`, `OPPONENT_JOINED {name}`, `OPPONENT_LEFT`, `MOVE {san}`, `DRAW_OFFER`, `GAME_OVER {result} {reason}`, `CHAT {name} {text}`, `PONG`, `ERROR {message}`
  - [ ] Define a `Result` enum: `WHITE_WINS, BLACK_WINS, DRAW, RESIGNATION, ABORT`

- [ ] **3.2 Wire format (`protocol.h`/`protocol.cpp`)**
  - [ ] Encode messages as `sf::Packet` (size-prefixed, binary) OR newline-delimited text — pick one and document it
  - [ ] `bool serialize(Packet&, Message&)` and `bool deserialize(Packet&, Message&)`
  - [ ] Version/`HELLO` handshake field to reject mismatched clients
  - [ ] Human-readable `debugString(Message)` for logs

- [ ] **3.3 Protocol tests**
  - [ ] Round-trip every message type through serialization
  - [ ] Truncated/garbage packet → clean error, no crash
  - [ ] Validate that `MOVE` payload is always valid SAN (deferred content check → server re-validates against engine)

---

## Phase 4 — Server (`server/`)

**Goal:** a headless server that runs any number of concurrent matches.

**Exit version:** `v0.5.0` — server hosts matches

- [ ] **4.1 TCP listener (`main.cpp`)**
  - [ ] Parse `--port` (default e.g. 5555) and `--host` from argv
  - [ ] `sf::TcpListener`, bind, `listen()`, accept loop
  - [ ] Non-blocking select loop across all connected sockets (or one thread per client — document the choice)
  - [ ] Clean shutdown on SIGINT

- [ ] **4.2 Matchmaker (`matchmaker.cpp`)**
  - [ ] Player queue: when 2 players are waiting, pair them
  - [ ] Assign colors (random or first-joined = white); send `WELCOME` to both
  - [ ] Handle a player leaving the queue

- [ ] **4.3 Match session (`match.cpp`)**
  - [ ] Own a `Board` instance, start from the initial position
  - [ ] Validate each `MOVE` with `core::isLegalMove`; reject with `ERROR` if illegal
  - [ ] Forward valid SAN to both clients, update engine state
  - [ ] Detect game end via `core::GameStatus`; broadcast `GAME_OVER`
  - [ ] Handle `DRAW_OFFER`/`DRAW_ACCEPT`/`DRAW_DECLINE` and `RESIGN`
  - [ ] Handle disconnects (opponent wins or game aborted), notify the other player
  - [ ] `CHAT` relay between the two players

- [ ] **4.4 Server robustness**
  - [ ] Max message size guard, stale-connection timeout / keepalive
  - [ ] One bad client must not crash the server
  - [ ] Logging (`INFO/WARN/ERROR`) to stdout, optional `--log-file`

---

## Phase 5 — Client networking (`client/`)

**Goal:** a reliable, responsive connection wrapper for the GUI loop.

**Exit version:** `v0.6.0` — client connects

- [ ] **5.1 Connection wrapper (`connection.cpp`)**
  - [ ] Wrap `sf::TcpSocket` in non-blocking mode
  - [ ] Outgoing message queue (drained each frame)
  - [ ] Incoming message queue (polled each frame, parsed via `net/`)
  - [ ] `connect(host, port)` with timeout and friendly error reporting

- [ ] **5.2 Keepalive**
  - [ ] Send `PING` every N seconds; if no `PONG` within timeout → mark disconnected
  - [ ] Detect server closing the socket and surface "connection lost"

- [ ] **5.3 Connection flow**
  - [ ] Build `JOIN` on connect, wait for `WELCOME`, surface `OPPONENT_JOINED`
  - [ ] Map incoming `MOVE`/`GAME_OVER`/`CHAT` events into the GUI's app state

---

## Phase 6 — SFML GUI (`client/`)

**Goal:** a clean, playable board UI.

**Exit version:** `v0.7.0` — playable GUI

### Task 6.1 — App skeleton

- [ ] **6.1.1 Main loop (`main.cpp`/`app.cpp`)**
  - [ ] SFML `RenderWindow` (e.g. 800×800 + side panel), `Event` polling
  - [ ] Fixed-timestep update loop; render/draw each frame
  - [ ] Screen state machine: `ConnectScreen → GameScreen → GameOverScreen`
  - [ ] FPS-independent movement (for later animations)

- [ ] **6.1.2 Connect screen**
  - [ ] Text fields for host, port, player name (SFML `Text` + simple keyboard input)
  - [ ] "Connect" button; show status ("connecting…", "waiting for opponent…")
  - [ ] Handle connection failure without blocking the UI

### Task 6.2 — Board rendering

- [ ] **6.2.1 Board geometry (`boardview.cpp`)**
  - [ ] Define square size + board origin; map `Square` ↔ pixel rect
  - [ ] Draw 64 alternating light/dark squares
  - [ ] Draw rank/file labels (a–h, 1–8) in the margin
  - [ ] Flip board 180° when playing black (configurable/optional)

- [ ] **6.2.2 Piece sprites**
  - [ ] Load the 12 PNGs into `sf::Texture` (from `assets/pieces/`)
  - [ ] Draw each piece centered in its square (board + piece separation so pieces layer over highlights)
  - [ ] Fallback: if textures fail to load, draw letters (K/Q/R/B/N/P) with `sf::Text`

- [ ] **6.2.3 Highlights and state**
  - [ ] Highlight the selected square and its legal-move targets (dot for quiet, ring for capture)
  - [ ] Highlight last move's from/to squares
  - [ ] Red highlight on own king when in check

### Task 6.3 — Input

- [ ] **6.3.1 Click-click moves (`input.cpp`)**
  - [ ] Click a friendly piece → select + show legal moves
  - [ ] Click a legal target (or same piece to reselect) → send `MOVE` (SAN via `core::san::toSan`)
  - [ ] Ignore input when it's not your turn or game over
  - [ ] Show "illegal move" feedback (e.g. status message) if server rejects

- [ ] **6.3.2 Promotion picker**
  - [ ] On promotion move, pause and show 4 piece choices (n/b/r/q)
  - [ ] Complete the move with the chosen piece

- [ ] **6.3.3 Other controls**
  - [ ] Buttons: "Resign", "Offer draw", "Accept/Decline draw" (when offered)
  - [ ] Chat: text input + send on Enter, event log with received messages

### Task 6.4 — HUD and game over

- [ ] **6.4.1 Status panel (`hud.cpp`)**
  - [ ] Whose turn, your color, opponent name
  - [ ] Move history (SAN list, scrollable or last-N lines)
  - [ ] Draw-offer / result / disconnection messages

- [ ] **6.4.2 Game-over screen**
  - [ ] Display result (e.g. "Checkmate — White wins"), reason, and a "Rematch" button
  - [ ] Return to connect screen or exit cleanly

---

## Phase 7 — Integration, polish, packaging

**Goal:** the game is fun, robust, and easy to run.

**Exit version:** `v1.0.0` — first release

- [ ] **7.1 End-to-end test**
  - [ ] Run server + two clients (second client via `-` window on same machine or second terminal)
  - [ ] Play a full game to checkmate; verify both screens and move history match
  - [ ] Test draw paths (repetition, stalemate, offer+accept), resign, disconnect-mid-game
  - [ ] Test on loopback and on another machine (if available)

- [ ] **7.2 Robustness pass**
  - [ ] Rapid spam / malformed input handled without crashes (client and server)
  - [ ] Server restart behavior; client reconnect path
  - [ ] Window resize (scale board proportionally) or fix to constant size — decide and be consistent

- [ ] **7.3 Readme + build polish**
  - [ ] README: prerequisites, build steps, `server` and `client` usage, controls, screenshots
  - [ ] `make install` / CMake install target or a `run.sh` convenience script
  - [ ] Release build flags (`-O2`, `-DNDEBUG`) documented in CMake presets

- [ ] **7.4 Final code-quality pass**
  - [ ] Run a linter/formatter (e.g. `clang-format`) if available
  - [ ] Compiler warnings clean (`-Wall -Wextra -Wpedantic`)
  - [ ] Review includes/const-correctness; remove dead code

---

## Phase 8 — Stretch goals (optional, in rough order)

**Exit version:** `v1.x.0` — one MINOR bump per landed stretch item

- [ ] **8.1 Chess clocks** — time controls (blitz/rapid), increment, timeout = loss
- [ ] **8.2 Move animation** — piece slides; requires the timestep-ready loop from 6.1
- [ ] **8.3 Chat polish** — timestamps, colors, mute
- [ ] **8.4 Multiple games / spectators** — server hosts several matches; watchers see moves
- [ ] **8.5 Rematch flow** — agreed rematch starts a new game without reconnecting
- [ ] **8.6 PGN export** — save finished games; load PGN into the engine
- [ ] **8.7 Sound effects** — move/capture/checkmate (requires `sf::SoundBuffer`)
- [ ] **8.8 Board interaction** — drag-and-drop, right-click deselect
- [ ] **8.9 Performance** — bitboard or precomputed tables; apply to movegen only after 2.x proves correctness
- [ ] **8.10 AI opponent** — when no human opponent, play against a minimax/alpha-beta engine (the natural next chessprogramming.org chapter)

---

## chessprogramming.org quick reference

| Topic | Page |
|---|---|
| Overview / starting point | https://www.chessprogramming.org/Getting_Started |
| Board representation | https://www.chessprogramming.org/Board_Representation |
| 0x88 board | https://www.chessprogramming.org/0x88 |
| Mailbox boards | https://www.chessprogramming.org/Mailbox |
| FEN | https://www.chessprogramming.org/Forsyth-Edwards_Notation |
| Move generation | https://www.chessprogramming.org/Move_Generation |
| Pawn moves | https://www.chessprogramming.org/Pawn_Moves |
| Castling | https://www.chessprogramming.org/Castling |
| En passant | https://www.chessprogramming.org/En_passant |
| Promotion | https://www.chessprogramming.org/Promotion |
| Check | https://www.chessprogramming.org/Check |
| Checkmate | https://www.chessprogramming.org/Checkmate |
| Stalemate | https://www.chessprogramming.org/Stalemate |
| Threefold repetition | https://www.chessprogramming.org/Threefold_Repetition |
| Fifty-move rule | https://www.chessprogramming.org/Fifty-move_Rule |
| Insufficient material | https://www.chessprogramming.org/Insufficient_Material |
| SAN | https://www.chessprogramming.org/Algebraic_Notation |
| Perft | https://www.chessprogramming.org/Perft |
| Make/unmake | https://www.chessprogramming.org/Move_Making |
