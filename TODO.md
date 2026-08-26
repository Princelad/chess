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
| 8 — Engine integration (UCI) | `v1.1.0` | Play + analyze with an external engine |
| 9 — Client UI/UX overhaul | `v1.2.0` | Menu shell, widget layer, move navigator |
| 10 — Persistence & ratings | `v1.3.0` | Accounts, Glicko-2, game archive |
| 11 — Multiplayer QoL | `v1.4.0` | Clocks, rematch, reconnect/resume, premove, themes |
| 12 — Variants & community | `v1.5.0` | Chess960, puzzles, tournaments, spectators |
| 13 — Release engineering | `v1.6.0` | CI, packaging, portability |

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
├── server/                   # chess-server binary (headless, no SFML needed)
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp          # listener loop, CLI args (port)
│       ├── client.h          # per-client state machine
│       ├── matchmaker.cpp    # queue players, pair into matches, assign colors
│       ├── match.cpp         # one game session state machine
│       ├── send.h            # sendTo helper
│       └── log.h             # logging utilities
├── client/                   # chess-client binary (SFML GUI)
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp          # app entry
│       ├── app.h / app.cpp   # screen state machine, asset loading
│       ├── connection.h / connection.cpp    # TCP wrapper (non-blocking, queue)
│       ├── boardview.h / boardview.cpp      # pixel<->square mapping, draw board/pieces
│       └── screens/
│           ├── connect_screen.h / connect_screen.cpp  # host/port/name input
│           ├── game_screen.h / game_screen.cpp        # main game UI
│           ├── game_over_screen.h / game_over_screen.cpp
│           └── hud.h / hud.cpp                        # status, move history, chat
├── assets/
│   ├── pieces/               # 12 PNG sprites (cburnett)
│   └── fonts/                # Inter-Regular.ttf
├── tests/
│   ├── CMakeLists.txt
│   └── core/                 # perft, fen, san, rules tests
└── AGENTS.md                 # build commands, conventions
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

- [x] **1.1.3 Game-state struct**
  - [x] Side to move, castling rights (WK/WQ/BK/BQ flags), en-passant target square, halfmove clock, fullmove number
  - [x] Move history stack to support undo (position copies for repetition checks)
  - [x] `Board` class API: `pieceAt(Square)`, `sideToMove()`, `makeMove(Move)` — `undoMove()` deferred to 1.4.2

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

- [x] **1.3.1 Generate moves for fixed-step pieces**
  - [x] Knight: 8 offsets, `!offBoard(to) && !ownPiece(to)`
  - [x] King: 8 offsets (excluding castling for now)
  - [x] Pawns: single push, double push from starting rank, diagonal captures, promotions (4 types), en passant

- [x] **1.3.2 Generate sliding-piece moves**
  - [x] Rook: walk 4 directions until board edge or blocker
  - [x] Bishop: same with 4 diagonal directions
  - [x] Queen: rook + bishop rays combined
  - [x] Set `capture` flag when landing on enemy piece; stop at first blocker

- [x] **1.3.3 Castling moves**
  - [x] Only when castling-right flag set, squares between empty, king/rook unmoved
  - [x] Generate king-side and queen-side castling as moves (legality of "not in/through check" deferred to Task 1.4)

- [x] **1.3.4 Assemble `generateMoves(Board)`**
  - [x] Iterate 0x88 squares, dispatch on piece type, collect into `std::vector<Move>`
  - [x] Provide a `generateMoves<Filter>(board)` so callers can ask for captures-only later

### Task 1.4 — Make/unmake and legality

**Goal:** only legal moves remain; `makeMove`/`undoMove` maintain full state.

▶ [Move Making](https://www.chessprogramming.org/Move_Making) · [Check](https://www.chessprogramming.org/Check) · [King Attack](https://www.chessprogramming.org/King_Attack)

- [x] **1.4.1 Implement `makeMove`**
  - [x] Move piece, handle captures (remove piece, restore on undo)
  - [x] Handle promotion (replace pawn with chosen piece)
  - [x] Handle en passant capture (remove the captured pawn, not the target square's piece)
  - [x] Handle castling (also move the rook)
  - [x] Update castling rights (king/rook moved or square captured), en-passant target, clocks, side-to-move
  - [x] Push full previous state onto history stack for `undoMove`

- [x] **1.4.2 Implement `undoMove`**
  - [x] Pop history, restore pieces/state exactly (including promotion piece restored to pawn)
  - [x] Round-trip test: for every generated move, `makeMove` then `undoMove` returns original position

- [x] **1.4.3 Check detection**
  - [x] `isAttacked(sq, byColor)`: reuse movegen — is any enemy piece attacking `sq`?
  - [x] Simplify: scan from `sq` outward (knight offsets, king offsets, rook/bishop/queen rays, pawn attacks)
  - [x] `inCheck(board, color)` → king square attacked?

- [x] **1.4.4 Legal move filtering**
  - [x] Generate pseudo-legal moves, then simulate each with `makeMove` and keep only those where own king is not attacked
  - [x] King "moves into check", pinned pieces, castling out of/through check all handled for free by the filter
  - [x] Implement efficient-enough version first; optimization is a stretch goal

### Task 1.5 — Game-end detection and draw rules

**Goal:** know when the game is over and why.

▶ [Checkmate](https://www.chessprogramming.org/Checkmate) · [Stalemate](https://www.chessprogramming.org/Stalemate) · [Threefold Repetition](https://www.chessprogramming.org/Threefold_Repetition) · [Fifty-move Rule](https://www.chessprogramming.org/Fifty-move_Rule) · [Insufficient Material](https://www.chessprogramming.org/Insufficient_Material)

- [x] **1.5.1 Checkmate / stalemate**
  - [x] After filtering legal moves: if none and in check → checkmate; if none and not in check → stalemate
  - [x] Return a `GameState` enum: `Ongoing, Checkmate, Stalemate, Draw`

- [x] **1.5.2 Draw by repetition**
  - [x] Track position history (hashed or FEN key per position)
  - [x] `threefoldRepetition()` — same position 3 times (side to move matters)
  - [x] Include en-passant and castling-rights state in the "same position" test

- [x] **1.5.3 Fifty-move rule**
  - [x] Halfmove clock reaches 100 → draw
  - [x] Reset clock on pawn move or capture (already done in `makeMove`)

- [x] **1.5.4 Insufficient material**
  - [x] K vs K, K+B vs K, K+N vs K (and K+B vs K+B same-color bishops) → draw

- [x] **1.5.5 Public rules API**
  - [x] `GameStatus evaluate(Board)` combining all above
  - [x] `bool isLegalMove(Board, Move)` for server-side validation

### Task 1.6 — SAN (algebraic notation)

**Goal:** parse and format human-readable moves, both for the protocol and the GUI.

▶ [Algebraic Notation](https://www.chessprogramming.org/Algebraic_Notation)

- [x] **1.6.1 Format moves → SAN (`san::toSan`)**
  - [x] Disambiguate identical moves (file/rank/both, e.g. `Nbd2`, `R1e3`, `Qh4e1`)
  - [x] Capture marker `x`, promotion `=Q`, check `+`, checkmate `#`, castling `O-O` / `O-O-O`
  - [x] En passant in SAN (e.g. `exd6`)

- [x] **1.6.2 Parse SAN → move (`san::fromSan`)**
  - [x] Resolve notation against generated legal moves (find the unique matching move)
  - [x] Handle all the same cases as formatting (disambiguation, promotions, castles)
  - [x] Return error on ambiguous/illegal input

---

## Phase 2 — Core unit tests

**Goal:** prove the engine is correct before building anything on top of it.

**Exit version:** `v0.3.0` — engine verified by perft

▶ [Perft](https://www.chessprogramming.org/Perft) · [Engine Testing](https://www.chessprogramming.org/Engine_Testing)

- [x] **2.1 Perft (move-path counting)**
  - [x] Implement `perft(board, depth)` counting leaf nodes
  - [x] Verify starting position: `perft(1)=20, perft(2)=400, perft(3)=8902, perft(4)=197281, perft(5)=4865609`
  - [x] Verify the standard "Kiwipete" position (e.g. `r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1`) at depths 1–3
  - [x] Verify positions 3–6 from CPW (castling, en passant, promotions, pinned pieces, fianchetto symmetrical position)

- [x] **2.2 FEN tests**
  - [x] Round-trip start position and several midgames
  - [x] Reject malformed FEN strings gracefully

- [x] **2.3 Special-move tests**
  - [x] Castling rights granted/revoked correctly (king/rook move, rook captured)
  - [x] En passant captured pawn removed; ep target square correct timing
  - [x] All 4 promotions legal and revert on undo

- [x] **2.4 Endgame/draw tests**
  - [x] Fool's mate (checkmate in 2), scholar's mate, back-rank mate
  - [x] Stalemate position, threefold repetition, fifty-move counter, insufficient material
  - [x] Legal vs illegal: pinned-piece move, king-into-check, castling through check all rejected

- [x] **2.5 SAN tests**
  - [x] `toSan`↔`fromSan` round-trips across a game's move list
  - [x] Disambiguation correctness (`Nbd2` vs `Nfd2`), castling, promotion with check

---

## Phase 3 — Network protocol (`net/`)

**Goal:** a shared, well-tested protocol used identically by server and client.

**Exit version:** `v0.4.0` — protocol defined + tested

- [x] **3.1 Define message types (`messages.h`)**
  - [x] Client→Server: `JOIN {name}`, `MOVE {san}`, `DRAW_OFFER`, `DRAW_ACCEPT`, `DRAW_DECLINE`, `RESIGN`, `CHAT {text}`, `PING`
  - [x] Server→Client: `WELCOME {color} {opponent}`, `OPPONENT_JOINED {name}`, `OPPONENT_LEFT`, `MOVE {san}`, `DRAW_OFFER`, `GAME_OVER {result} {reason}`, `CHAT {name} {text}`, `PONG`, `ERROR {message}`
  - [x] Define a `Result` enum: `WHITE_WINS, BLACK_WINS, DRAW, RESIGNATION, ABORT`

- [x] **3.2 Wire format (`protocol.h`/`protocol.cpp`)**
  - [x] Encode messages as `sf::Packet` (size-prefixed, binary) OR newline-delimited text — pick one and document it
  - [x] `bool serialize(Packet&, Message&)` and `bool deserialize(Packet&, Message&)`
  - [x] Version/`HELLO` handshake field to reject mismatched clients
  - [x] Human-readable `debugString(Message)` for logs

- [x] **3.3 Protocol tests**
  - [x] Round-trip every message type through serialization
  - [x] Truncated/garbage packet → clean error, no crash
  - [x] Validate that `MOVE` payload is always valid SAN (deferred content check → server re-validates against engine)

---

## Phase 4 — Server (`server/`)

**Goal:** a headless server that runs any number of concurrent matches.

**Exit version:** `v0.5.0` — server hosts matches

- [x] **4.1 TCP listener (`main.cpp`)**
  - [x] Parse `--port` (default e.g. 5555) and `--host` from argv
  - [x] `sf::TcpListener`, bind, `listen()`, accept loop
  - [x] Non-blocking select loop across all connected sockets (or one thread per client — document the choice)
  - [x] Clean shutdown on SIGINT

- [x] **4.2 Matchmaker (`matchmaker.cpp`)**
  - [x] Player queue: when 2 players are waiting, pair them
  - [x] Assign colors (random or first-joined = white); send `WELCOME` to both
  - [x] Handle a player leaving the queue

- [x] **4.3 Match session (`match.cpp`)**
  - [x] Own a `Board` instance, start from the initial position
  - [x] Validate each `MOVE` with `core::isLegalMove`; reject with `ERROR` if illegal
  - [x] Forward valid SAN to both clients, update engine state
  - [x] Detect game end via `core::GameStatus`; broadcast `GAME_OVER`
  - [x] Handle `DRAW_OFFER`/`DRAW_ACCEPT`/`DRAW_DECLINE` and `RESIGN`
  - [x] Handle disconnects (opponent wins or game aborted), notify the other player
  - [x] `CHAT` relay between the two players

- [x] **4.4 Server robustness**
  - [x] Max message size guard, stale-connection timeout / keepalive
  - [x] One bad client must not crash the server
  - [x] Logging (`INFO/WARN/ERROR`) to stdout, optional `--log-file`

---

## Phase 5 — Client networking (`client/`)

**Goal:** a reliable, responsive connection wrapper for the GUI loop.

**Exit version:** `v0.6.0` — client connects

- [x] **5.1 Connection wrapper (`connection.cpp`)**
  - [x] Wrap `sf::TcpSocket` in non-blocking mode
  - [x] Outgoing message queue (drained each frame)
  - [x] Incoming message queue (polled each frame, parsed via `net/`)
  - [x] `connect(host, port)` with timeout and friendly error reporting

- [x] **5.2 Keepalive**
  - [x] Send `PING` every N seconds; if no `PONG` within timeout → mark disconnected
  - [x] Detect server closing the socket and surface "connection lost"

- [x] **5.3 Connection flow**
  - [x] Build `JOIN` on connect, wait for `WELCOME`, surface `OPPONENT_JOINED`
  - [x] Map incoming `MOVE`/`GAME_OVER`/`CHAT` events into the GUI's app state

---

## Phase 6 — SFML GUI (`client/`)

**Goal:** a clean, playable board UI.

**Exit version:** `v0.7.0` — playable GUI

### Task 6.1 — App skeleton

- [x] **6.1.1 Main loop (`main.cpp`/`app.cpp`)**
  - [x] SFML `RenderWindow` (e.g. 800×800 + side panel), `Event` polling
  - [x] Fixed-timestep update loop; render/draw each frame
  - [x] Screen state machine: `ConnectScreen → GameScreen → GameOverScreen`
  - [x] FPS-independent movement (for later animations)

- [x] **6.1.2 Connect screen**
  - [x] Text fields for host, port, player name (SFML `Text` + simple keyboard input)
  - [x] "Connect" button; show status ("connecting…", "waiting for opponent…")
  - [x] Handle connection failure without blocking the UI

### Task 6.2 — Board rendering

- [x] **6.2.1 Board geometry (`boardview.cpp`)**
  - [x] Define square size + board origin; map `Square` ↔ pixel rect
  - [x] Draw 64 alternating light/dark squares
  - [x] Draw rank/file labels (a–h, 1–8) in the margin
  - [x] Flip board 180° when playing black (configurable/optional)

- [x] **6.2.2 Piece sprites**
  - [x] Load the 12 PNGs into `sf::Texture` (from `assets/pieces/`)
  - [x] Draw each piece centered in its square (board + piece separation so pieces layer over highlights)
  - [x] Fallback: if textures fail to load, draw letters (K/Q/R/B/N/P) with `sf::Text`

- [x] **6.2.3 Highlights and state**
  - [x] Highlight the selected square and its legal-move targets (dot for quiet, ring for capture)
  - [x] Highlight last move's from/to squares
  - [x] Red highlight on own king when in check

### Task 6.3 — Input

- [x] **6.3.1 Click-click moves (`game_screen.cpp`)**
  - [x] Click a friendly piece → select + show legal moves
  - [x] Click a legal target (or same piece to reselect) → send `MOVE` (SAN via `core::san::toSan`)
  - [x] Ignore input when it's not your turn or game over
  - [x] Show "illegal move" feedback (e.g. status message) if server rejects

- [x] **6.3.2 Promotion picker**
  - [x] On promotion move, pause and show 4 piece choices (n/b/r/q)
  - [x] Complete the move with the chosen piece

- [x] **6.3.3 Other controls**
  - [x] Buttons: "Resign", "Offer draw", "Accept/Decline draw" (when offered)
  - [x] Chat: text input + send on Enter, event log with received messages

### Task 6.4 — HUD and game over

- [x] **6.4.1 Status panel (`hud.cpp`)**
  - [x] Whose turn, your color, opponent name
  - [x] Move history (SAN list, scrollable or last-N lines)
  - [x] Draw-offer / result / disconnection messages

- [x] **6.4.2 Game-over screen**
  - [x] Display result (e.g. "Checkmate — White wins"), reason, and a "Rematch" button (returns to connect screen)
  - [x] Return to connect screen or exit cleanly

---

## Phase 7 — Integration, polish, packaging

**Goal:** the game is fun, robust, and easy to run.

**Exit version:** `v1.0.0` — first release

- [x] **7.1 End-to-end test**
  - [x] Run server + two clients (second client via `-` window on same machine or second terminal)
  - [x] Play a full game to checkmate; verify both screens and move history match
  - [x] Test draw paths (repetition, stalemate, offer+accept), resign, disconnect-mid-game
  - [x] Test on loopback and on another machine (if available)

- [x] **7.2 Robustness pass**
  - [x] Rapid spam / malformed input handled without crashes (client and server)
  - [x] Server restart behavior; client reconnect path
  - [x] Window resize (scale board proportionally) or fix to constant size — decide and be consistent

- [x] **7.3 Readme + build polish**
  - [x] README: prerequisites, build steps, `server` and `client` usage, controls, screenshots
  - [x] `make install` / CMake install target or a `run.sh` convenience script
  - [x] Release build flags (`-O2`, `-DNDEBUG`) documented in CMake presets

- [x] **7.4 Final code-quality pass**
  - [x] Run a linter/formatter (e.g. `clang-format`) if available
  - [x] Compiler warnings clean (`-Wall -Wextra -Wpedantic`)
  - [x] Review includes/const-correctness; remove dead code

---

## Phase 8 — Engine integration via UCI

**Goal:** strong computer opponents and analysis powered by an external UCI engine
(e.g. Stockfish) instead of a hand-written search.

**Exit version:** `v1.1.0` — engine-backed play and analysis work end-to-end

▶ [UCI](https://www.chessprogramming.org/UCI)

- [x] **8.1 UCI process wrapper (`uci/`)**
  - [x] Spawn engine binary (`fork`/`exec` or `posix_spawn`) with stdin/stdout pipes
  - [x] Handshake: send `uci`, wait for `uciok`; `isready`/`readyok` sync before commands
  - [x] Background reader thread parsing engine output into a queue/callback
  - [x] Config: `setoption` (Skill Level, Threads, Hash), `ucinewgame`, `position fen ... moves ...`
  - [x] Request moves via `go depth N` / `go movetime MS`; parse `bestmove <coords>` → `core::Move`
  - [x] Timeout/crash handling; clean `quit` on shutdown

- [x] **8.2 Play vs computer (local)**
  - [x] Screen to pick side + strength preset (depth/movetime mapping)
  - [x] Reuse GameScreen/HUD against a local engine opponent (no server)
  - [x] Engine replies non-blocking (never freeze the render loop)
  - [ ] Local takeback/undo trivially supported vs CPU (deferred to 9.x)
  - [x] Temporary entry point from the connect screen until the 9.1 menu shell lands

- [ ] **8.3 Server-side bots**
  - [ ] Bot pseudo-player joins the matchmaking queue; paired like a human
  - [ ] Server hosts one UCI session per bot match, playing through the same MOVE path
  - [ ] Concurrency cap + queueing for engine sessions

- [ ] **8.4 PGN import/export (`core/`)**
  - [ ] Parse PGN headers + SAN movetext into a game (move list, result)
  - [ ] Emit canonical PGN from any played/saved game
  - [ ] Round-trip tests; skip comments/variations gracefully

- [ ] **8.5 Analysis board**
  - [ ] Load a finished game (FEN/PGN — uses 8.4) into read-only replay
  - [ ] Per-position eval bar; best-move arrow overlay
  - [ ] Eval graph across the game's plies

---

## Phase 9 — Client UI/UX overhaul

**Goal:** a real app shell and reusable hand-rolled widgets — no new GUI dependency.

**Exit version:** `v1.2.0` — menu-driven client with scalable layout

- [ ] **9.1 Menu shell & navigation**
  - [ ] New root screen: Play online / vs Computer / Puzzles / Archive / Settings
  - [ ] Unimplemented entries visible but disabled (greyed out)
  - [ ] Replace linear screen flow with a back-stack (Esc = go back)

- [ ] **9.2 Widget layer + responsive layout**
  - [ ] Factor reusable widgets from Phase 6 screens: `Button` (hover/pressed/disabled), `TextField` (focus/caret), `Panel`, `Label`
  - [ ] Layout helpers (margins, alignment, vertical/horizontal stacks)
  - [ ] Settle 7.2's resize question: window resize rescales board + side panel proportionally
  - [ ] View transform keeps pixel↔square mapping correct at any scale

- [ ] **9.3 Move-list navigator + captured material**
  - [ ] Clickable two-column SAN grid replaces the plain text list
  - [ ] Navigator controls `|< < > >|` + ←/→/Home/End keys; jump to any ply
  - [ ] Read-only replay of historical positions from move history
  - [ ] Captured-piece row + material diff per player card

- [ ] **9.4 Board interaction: drag-and-drop + annotations + promotion options**
  - [ ] Drag-and-drop piece movement (activation threshold; dropping off-board cancels)
  - [ ] Right-click drag draws arrows; plain right-click toggles a circle
  - [ ] Left-click clears user annotations; right-click also deselects
  - [ ] Auto-queen setting; promotion picker dialog vs inline choice

- [ ] **9.5 Settings screen + config file**
  - [ ] INI-style config persisted to `~/.config/chess/config.ini`
  - [ ] Sound volume/toggle (consumed by 9.8); animation toggle/duration (feeds 9.7)
  - [ ] Board colors + piece-set path (foundation for 11.6 themes)
  - [ ] Auto-queen, show-coordinates toggle
  - [ ] Load/save round-trip tested

- [ ] **9.6 Keyboard play**
  - [ ] Arrow-key square cursor; Enter/Space selects/moves; Esc cancels/deselects
  - [ ] `F` flips board; `?` shows shortcut help overlay

- [ ] **9.7 Move animation**
  - [ ] Pieces slide from source to destination using the fixed-timestep loop from 6.1
  - [ ] Duration/easing configurable via 9.5 settings

- [ ] **9.8 Sound effects**
  - [ ] Move/capture/check/checkmate clips via `sf::SoundBuffer`
  - [ ] Volume/mute controlled by 9.5 settings

---

## Phase 10 — Persistence & ratings

**Goal:** identity, ratings, and saved games survive server restarts.

**Exit version:** `v1.3.0` — accounts, rated games, browsable archive

▶ [Glicko-2 paper](http://www.glicko.net/glicko/glicko2.pdf)

- [ ] **10.1 Storage layer**
  - [ ] SQLite (system libsqlite3): players + games tables
  - [ ] Optional password auth (salted hash); guest play stays allowed and unrated

- [ ] **10.2 Rated games (Glicko-2)**
  - [ ] Implement Glicko-2 per Glickman's paper; unit-test against the worked example
  - [ ] Rating classes by time control once 11.1 clocks land (single class until then)
  - [ ] Extend profile messages with rating; update atomically on GAME_OVER

- [ ] **10.3 Game archive**
  - [ ] Persist every completed game (players, TC, SAN moves, result, date)
  - [ ] PGN generation from archive (reuses the 8.4 PGN writer)
  - [ ] Client "my games" list screen; open → replay viewer (reuses 9.3 navigator)

- [ ] **10.4 Leaderboard**
  - [ ] Server top-N query by rating
  - [ ] Client leaderboard screen

---

## Phase 11 — Multiplayer quality of life

**Goal:** smoother, friendlier online play.

**Exit version:** `v1.4.0`

- [ ] **11.1 Chess clocks**
  - [ ] Protocol: remaining-time synced with every MOVE broadcast
  - [ ] Server-enforced timeout = loss; Fischer increment supported
  - [ ] Time-control presets (bullet/blitz/rapid/classical) chosen before pairing
  - [ ] Ticking clock UI beside each player card

- [ ] **11.2 Reconnect & resume**
  - [ ] Server keeps match alive ~30 s after disconnect; token-based rejoin
  - [ ] Protocol: `RESUME {token}` → `STATE {fen, moves, clocks}` full resync
  - [ ] Opponent sees "opponent reconnecting…" status

- [ ] **11.3 Takeback**
  - [ ] Protocol `TAKEBACK_REQUEST/ACCEPT/DECLINE`; server validates via two-ply `undoMove`
  - [ ] Client request UI + accept/decline prompt

- [ ] **11.4 Premoves**
  - [ ] Queue next move while opponent thinks; ghost piece rendered
  - [ ] Auto-send when legal on position update; right-click/Esc cancels (pairs with 9.4)

- [ ] **11.5 Opening names (ECO)**
  - [ ] Small bundled ECO table asset; prefix-match on move list
  - [ ] Show opening name (+ ECO code) in HUD after each move

- [ ] **11.6 Themes**
  - [ ] Board color palettes + piece sets (`assets/pieces/<set>/`)
  - [ ] Selected via the 9.5 settings/config file

- [ ] **11.7 Rematch flow**
  - [ ] Post-game offer/accept starts a new game without reconnecting
  - [ ] Colors swapped on rematch

- [ ] **11.8 Chat polish**
  - [ ] Timestamps, name colors, per-player mute

---

## Phase 12 — Variants & community

**Goal:** more ways to play beyond standard 1v1 online.

**Exit version:** `v1.5.0`

▶ [Chess960](https://www.chessprogramming.org/Chess960)

- [ ] **12.1 Chess960**
  - [ ] Core: 960 start-position generator + flexible castling rules
  - [ ] Protocol: `VARIANT` field in JOIN/WELCOME; server-side option
  - [ ] Perft spot-checks against known FRC positions

- [ ] **12.2 Puzzle mode**
  - [ ] Offline mate-in-N set loaded from a bundled PGN file
  - [ ] Hints/solution reveal; streak counter (uses the 9.1 menu entry)

- [ ] **12.3 Arena tournaments**
  - [ ] Server periodically pairs all queued players; standings broadcast
  - [ ] Client tournament lobby + standings table

- [ ] **12.4 Spectators**
  - [ ] Server relays moves to observers watching a live match
  - [ ] Spectator list/count surfaced to players (optional)

---

## Phase 13 — Release engineering

**Goal:** others can build, run, and deploy the project easily.

**Exit version:** `v1.6.0` — CI green, installable, dockerized server

- [ ] **13.1 CI**
  - [ ] GitHub Actions: Ubuntu matrix (GCC/Clang), cmake configure/build + ctest
  - [ ] Install SFML via apt; cache FetchContent deps between runs

- [ ] **13.2 Packaging**
  - [ ] Headless server Dockerfile + docker-compose example
  - [ ] CMake install targets (binaries + assets); README install docs

- [ ] **13.3 Portability pass**
  - [ ] Audit fs paths, signal handling, compiler-specific code
  - [ ] Windows/macOS build notes (full support if cheap)

- [ ] **13.4 Performance pass (optional)**
  - [ ] Profile before touching anything; only optimize what's measurably slow
  - [ ] Candidate: bitboards or precomputed attack tables for movegen
  - [ ] Perft suite must stay green after any change

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
| UCI protocol | https://www.chessprogramming.org/UCI |
| Chess960 / Fischer Random | https://www.chessprogramming.org/Chess960 |
