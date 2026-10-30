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
| 10 — Persistence & ratings | `v1.3.0` | Accounts, Glicko-2, game archive, leaderboard |
| 11 — Multiplayer QoL | `v1.4.0` | Clocks, rematch, reconnect/resume, premove, themes |
| 12 — Variants & community | `v1.5.0` | Chess960, puzzles, tournaments, spectators |
| 13 — Release engineering | `v1.6.0` | CI, packaging, portability |
| 14 — Design system v2 | `v1.7.0` | Token-driven UI, icons, avatars, restyled screens |
| 15 — Game experience rebuild | `v1.8.0` | Board-first game layout, player cards, quick chat |
| 16 — Engine intelligence | `v1.9.0` | Move classification, accuracy, game report |
| 17 — Puzzles, personas & play hub | `v2.0.0` | Puzzle Rush, daily puzzle, persona bots, play hub |

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
  - [x] Local takeback/undo trivially supported vs CPU (Land with 13.0 bookkeeping: LocalGameScreen Takeback button beside Back to Menu — only while it's the player's turn, retracting engine reply + own move via `MoveNavigator::setGame` two-ply rebuild; engine search never straddles the rollback, so no stale-bestmove race)
  - [x] Temporary entry point from the connect screen until the 9.1 menu shell lands

- [x] **8.3 Server-side bots**
  - [x] Bot pseudo-player joins the matchmaking queue; paired like a human
  - [x] Server hosts one UCI session per bot match, playing through the same MOVE path
  - [x] Concurrency cap + queueing for engine sessions

- [x] **8.4 PGN import/export (`core/`)**
  - [x] Parse PGN headers + SAN movetext into a game (move list, result)
  - [x] Emit canonical PGN from any played/saved game
  - [x] Round-trip tests; skip comments/variations gracefully

- [x] **8.5 Analysis board**
  - [x] Load a finished game (FEN/PGN — uses 8.4) into read-only replay
  - [x] Per-position eval bar; best-move arrow overlay
  - [x] Eval graph across the game's plies

---

## Phase 9 — Client UI/UX overhaul

**Goal:** a real app shell and reusable hand-rolled widgets — no new GUI dependency.

**Exit version:** `v1.2.0` — menu-driven client with scalable layout

- [x] **9.1 Menu shell & navigation**
  - [x] New root screen: Play online / vs Computer / Puzzles / Archive / Settings
  - [x] Unimplemented entries visible but disabled (greyed out)
  - [x] Replace linear screen flow with a back-stack (Esc = go back)
  - [~] Residual: menu entry model + `App` back-stack transitions are untested (App owns a RenderWindow; a headless harness is deferred)

- [x] **9.2 Widget layer + responsive layout**
  - [x] Factor reusable widgets from Phase 6 screens: `Button` (hover/pressed/disabled), `TextField` (focus/caret), `Panel`, `Label`
  - [x] Layout helpers (margins, alignment, vertical/horizontal stacks)
  - [x] Settle 7.2's resize question: window resize rescales board + side panel proportionally
  - [x] View transform keeps pixel↔square mapping correct at any scale

- [x] **9.3 Move-list navigator + captured material**
  - [x] Clickable two-column SAN grid replaces the plain text list
  - [x] Navigator controls `|< < > >|` + ←/→/Home/End keys; jump to any ply
  - [x] Read-only replay of historical positions from move history
  - [x] Captured-piece row + material diff per player card
  - [~] Residual: player-card/HUD composition has no widget-level tests (needs a screen harness)

- [x] **9.4 Board interaction: drag-and-drop + annotations + promotion options**
  - [x] Drag-and-drop piece movement (activation threshold; dropping off-board cancels)
  - [x] Right-click drag draws arrows; plain right-click toggles a circle
  - [x] Left-click clears user annotations; right-click also deselects
  - [x] Auto-queen setting; promotion picker dialog vs inline choice
  - [~] Residual: drag→send/promo glue inside the game screens is untested (needs a screen harness)

- [x] **9.5 Settings screen + config file**
  - [x] INI-style config persisted to `~/.config/chess/config.ini`
  - [x] Sound volume/toggle (consumed by 9.8); animation toggle/duration (feeds 9.7)
  - [x] Board colors + piece-set path (foundation for 11.6 themes)
  - [x] Auto-queen, show-coordinates toggle
  - [x] Load/save round-trip tested

- [x] **9.6 Keyboard play**
  - [x] Arrow-key square cursor; Enter/Space selects/moves; Esc cancels/deselects
  - [x] `F` flips board; `?` shows shortcut help overlay

- [x] **9.7 Move animation**
  - [x] Pieces slide from source to destination using the fixed-timestep loop from 6.1
  - [x] Duration/easing configurable via 9.5 settings

- [x] **9.8 Sound effects**
  - [x] Move/capture/check/checkmate clips via `sf::SoundBuffer`
  - [x] Volume/mute controlled by 9.5 settings

---

## Phase 10 — Persistence & ratings

**Goal:** identity, ratings, and saved games survive server restarts.

**Exit version:** `v1.3.0` — accounts, rated games, browsable archive, leaderboard

▶ [Glicko-2 paper](http://www.glicko.net/glicko/glicko2.pdf)

- [x] **10.1 Storage layer**
  - [x] SQLite (system libsqlite3): players + games tables (`db/` → `chessdb`; guests are stored by name only inside `games`, never in `players`)
  - [x] Optional password auth (salted hash); guest play stays allowed and unrated (store-level `authenticate`/`registerPlayer` with PBKDF2-HMAC-SHA-256; the LOGIN/profile wire flow and client UI land with 10.2's profile messages, `--db` currently opt-in on the server)

- [x] **10.2 Rated games (Glicko-2)**
  - [x] Implement Glicko-2 per Glickman's paper; unit-test against the worked example (`db` → `applyGames`, worked example 1464.06/151.52 reproduced; idle periods grow RD only, capped at 350)
  - [x] Rating classes by time control (`db/rating_class.h` classification: bullet/blitz/rapid/classical; single "unlimited" class active until 11.1 clocks give games a TC, then wire per-class ratings)
  - [x] Idle-period deviation growth (Glicko-2 no-game periods) (`PlayerStore::applyIdleGrowth`; server applies at login, daily periods since `players.last_rated_at` — schema rev 2, capped at RD 350)
  - [x] Extend profile messages with rating; update atomically on GAME_OVER (protocol v2: `LoginMsg`/`ProfileMsg` + rated `WelcomeMsg`; one transaction covers both players incl. W/L/D counters; aborted results are never rated; clients receive re-sent profiles)

- [x] **10.3 Game archive**
  - [x] Persist every completed game (players, TC, SAN moves, result, date) (`Match::sanHistory_` + game-over hook; any game with ≥1 move, guests keep NULL id columns)
  - [x] PGN generation from archive (reuses the 8.4 PGN writer) (`client/src/archive_pgn.h` → toPgn; Copy PGN via clipboard)
  - [x] Client "my games" list screen; open → replay viewer (reuses 9.3 navigator) (`HistoryScreen` over `Archive{Request,Get/List,Game}` messages; mine filter for logged-in accounts; `AnalysisScreen` as viewer)

- [x] **10.4 Leaderboard**
  - [x] Server top-N query by rating (`PlayerStore::topPlayers(limit, minGames = 1)`: rating DESC, name ASC tie-break; only accounts with ≥1 finished rated game rank, so factory-1500 rows stay hidden. Wire: `LeaderboardRequestMsg{limit}` → `LeaderboardListMsg{players}`, protocol v2 append-only tags; answered in every connection state beside the archive handlers; default 50 / clamped to the 100-row wire guard; no `--db` → `ErrorMsg` "This server has no leaderboard")
  - [x] Client leaderboard screen (`client/src/screens/leaderboard_screen.{h,cpp}`: read-only list panel, scroll + Esc/OK, own-row "(you)" highlight for logged-in accounts; entered from the ConnectScreen "Leaderboard" button — the connect flow's `archiveBrowse_` flag became `BrowseTarget {None, Archive, Leaderboard}` sharing the renamed `ConnectingBrowse` phase. Drive-by: archive server clamp 200→100 so replies never exceed the `ArchiveListMsg` wire guard)

---

## Phase 11 — Multiplayer quality of life

**Goal:** smoother, friendlier online play.

**Exit version:** `v1.4.0` — clocks, reconnect/resume, premove, ECO names,
takeback, rematch, chat polish, per-class ratings, login throttling

- [x] **11.1 Chess clocks**
  - [x] Protocol: remaining-time synced with every MOVE broadcast (`JoinMsg.timeControl` + `WelcomeMsg.timeControl` carry the "m+s" strings, empty = unlimited; `ServerMoveMsg.remainingMs` carries the on-move player's fresh remaining; `GameOverReason::Timeout` appended last — parser promoted to `chess::net::parseTimeControl` in messages.h, malformed = unlimited)
  - [x] Server-enforced timeout = loss; Fischer increment supported (`server/src/clock.{h,cpp}` — pure `MatchClock` over injected timestamps; White's clock starts at match construction, the mover is charged elapsed + increment per move and the broadcast carries their new remaining; `Match::pollClock` polls every loop pass, select wait shrinks to 100 ms while a clock runs; bots mirror the waiting human's TC and time out like humans)
  - [x] Time-control presets (bullet/blitz/rapid/classical) chosen before pairing (ConnectScreen cycle button `1+0/3+2/5+3/10+0/15+10/30+0/unlimited`, persisted as `general.time_control`; matchmaker pairs only equal TC strings — mismatched players stay queued)
  - [x] Ticking clock UI beside each player card (`Hud::syncClocks` re-syncs at every `ServerMoveMsg`, locally ticks between syncs in `Hud::update`; `m:ss` style, tenths under a minute, amber below 30 s, red below 10 s; no display for unlimited games)

- [x] **11.1b Per-class ratings**
  - [x] Schema rev 3: `player_ratings` table keyed `(player_id, class)` with Glicko-2 triples + W/L/D counters + `last_rated_at`; class strings come from `classifyTimeControl` ("unlimited"/bullet/blitz/rapid/classical). The rev-2→3 migration seeds every existing account's "unlimited" row verbatim from their flat `players` columns; rows are sparse — a first game in a class materializes it, unread classes read as factory defaults (1500/350)
  - [x] `PlayerStore::applyRatedGame` gains the time-control string: each class rates independently (both sides' pre-period class values feed Glicko-2, upsert via `ON CONFLICT`; unlimited games additionally keep mirroring the legacy `players` columns so the flat wire — ProfileMsg/WelcomeMsg/leaderboard top-N — keeps its single-number meaning without protocol churn). `PlayerStore::classRatingFor(id, tc)` reads a class row; `applyIdleGrowth` idles every owned class row independently (its own `last_rated_at`) and the login profile displays the newest-rated class; `Match::timeControl()` keys the server's rated hook and the WelcomeMsg opponent-rating read
  - [x] Tests: class isolation ("5+3" touches only blitz; flat mirror untouched), unlimited parity, migration seeding both directions, per-class idle growth (flat display follows the newest class), e2e `RatedE2eTest.TimedGameRatesItsOwnClass` (timed rated game shifts class figures; fresh login shows them)

- [x] **11.2 Reconnect & resume**
  - [x] Server keeps match alive ~30 s after disconnect; token-based rejoin (`Match` refactored to two `Seat`s — join-persistent name/playerId/rating snapshot plus a live `Client*` presence and a per-seat resume token. `ServerConfig.reconnectGraceMs = 30000` (overridable via `--grace-ms`); a solo drop opens the window and notifies the survivor, both absent = immediate unrated `Abort`, lapsed window = opponent wins by `Disconnection` (rated), and the absent seat's clock keeps ticking throughout — a flag fall during grace still loses on time)
  - [x] Protocol: `RESUME {token}` → `STATE {fen, moves, clocks}` full resync (protocol v2 append-only tags: client `Resume = 12`; server `ResumeState = 14`, `OpponentReconnecting = 15`, `OpponentResumed = 16`; `WelcomeMsg` gained `resumeToken`; `ResumeStateMsg` carries color/opponent/rated/TC/`fen`/SAN list (512-entry wire guard)/both remaining-ms. Finished matches linger in retention for the grace window so a late token still replays final state + `GameOverMsg`)
  - [x] Opponent sees "opponent reconnecting…" status (plus "opponent reconnected" on the rebind; the dropped client auto-rejoins: HUD "Connection lost — reconnecting…", 1 s-retry loop against `App::last{Host,Port}`, `ResumeStateMsg` rebuilds the screen in place via SAN replay with both clock figures re-synced — no move animation; chat history is not restored (accepted v1 scope); give-up after ~30 s or a rejected token falls back to the pre-11.2 game-over screen)
  - [x] Tests (shared `tests/e2e/harness.h` extracted from the fixtures; `tests/e2e/reconnect_test.cpp` covers leave→resume→finish rated+archived, grace expiry, double-disconnect abort, wrong token, and resume-after-game-over)

- [x] **11.3 Takeback**
  - [x] Protocol `TAKEBACK_REQUEST/ACCEPT/DECLINE`; server validates the classic two-ply frame (on the move, ≥2 plies, opponent seated) and rolls back by replaying the truncated SAN chain from the start position — the same reconstruction as a token resume; `Board::undoMove` stays an internal legality-detection helper. Bots auto-accept with no round trip. Clocks are not refunded — the retracted plies' spent time stays spent, and the rebuilt turn keeps running (no `MatchClock` surgery needed: after two charges, `onMove` equals the post-rollback `sideToMove` again). A pending request dissolves on the next accepted move, a decline (or the asker's self-cancel via their own decline), a disconnect, or the game ending; requesting while the opponent is mid-grace fails fast with an error
  - [x] Client request UI (Takeback button beside Resign/Offer Draw; prompt rows share the draw-offer accept/decline buttons) + `applyTakeback` rebuild via `MoveNavigator::setGame` SAN replay (`goEnd` immediately, no animation or move sound; `MoveAnimator::cancel` added; same `goEnd` fix applied to `applyResume`, which used to show ply 0 until the first click). Resume after a takeback replays the trimmed history (`TakebackE2eTest.ResumeAfterTakebackReplaysTrimmedHistory`)
  - [x] Tests (protocol round-trip/garbage; `MatchTest` validation + rollback + lifecycle; `BotMatchTest` auto-accept; `tests/e2e/takeback_test.cpp` agreed/declined/resume-after/clocked flows)

- [x] **11.4 Premoves**
  - [x] Queue next move while opponent thinks; ghost piece rendered (single `Premove` slot in `GameScreen`; moves trapped while `!myTurn_` are queued instead of sent; `BoardView::drawGhostPiece` shows the origin piece translucent on its landing square, plus cyan from/to tint via `HighlightState::premoveFrom/To`)
  - [x] Auto-send when legal on position update; right-click/Esc cancels (pairs with 9.4) (`flushPremove` runs right after the `ServerMoveMsg` echo in `GameScreen::update` — the echo stays the single history-append point, so a flushed premove never double-records; pure `client/src/premove.h` `resolvePremove` matches the queued from/to against `generateLegalMoves` of the live position: empty "capture" squares, own-piece landings and pinned/blocked paths just drop with a "Premove illegal" status, castling and en passant frames resolve naturally, promotion premoves auto-queen. Online `GameScreen` only; cleared on game over / takeback / resume)

- [x] **11.5 Opening names (ECO)**
  - [x] Small bundled ECO table asset; prefix-match on move list (compiled-in `client/src/eco.h` — no I/O, no install rules: ~130 `{sanPrefix, ECO code, name}` literals matched by **longest-prefix-wins** over the SAN history, so e4 → e4 c6 → e4 c6 d4 d5 e5 refines B00 King's Pawn → B10 Caro-Kann → B12 Advance; transposed move orders are covered by listing each spelling; '+'/'#' suffixes are stripped from both sides, so check-diversions still name the family; deliberately shallow — entries only where the ECO code is unambiguous)
  - [x] Show opening name (+ ECO code) in HUD after each move (`Hud::refreshOpening` re-runs `ecoLookup(navigator_.sans())` at both SAN-mutation points — `setGame` and `appendMove` — never `setStatus` (that's the timed message channel); rendered as "B01 - Scandinavian Defence", right-aligned on the "Moves" row; local/engine/analysis games get names for free via the shared `Hud`)

- [x] **11.6 Themes**
  - [x] Board color palettes + piece sets (`assets/pieces/<set>/`) (palettes enum extended with **Wood** (warm walnut) and **Moss** (gray-green), appended after `Dark` so persisted `board.colors` names stay valid; piece sets restructured into `assets/pieces/{cburnett,merida,chessnut}/` — merida (GPLv2+, Armando Hernandez Marroquin) and chessnut (Apache 2.0, Alexis Luengas) rasterized 160×160 from lichess-org/lila `public/piece/<set>/` per lila COPYING.md, each dir carries a README.md credit)
  - [x] Selected via the 9.5 settings/config file (new config key **`pieces.set`** (default `cburnett`, values = set dir names); Settings screen gains a "Piece set" cycle row right below the theme row (`pieceSetPrev_/Next_`, status text warns when a custom `pieces.path` overrides the named set); loading order in `loadPieceTextures`: custom path → `assets/pieces/<set>/` (all three relative roots) → legacy flat `assets/pieces/` root)

- [x] **11.7 Rematch flow**
  - [x] Post-game offer/accept starts a new game without reconnecting (protocol v2 append-only: client tags `RematchRequest = 16`/`RematchAccept = 17`/`RematchDecline = 18`, server tags `ServerRematchRequest = 20`/`ServerRematchDecline = 21` (carries `withdrawn` 0/1 for the self-cancel path); there is NO `RematchAccepted` — the fresh `WelcomeMsg` of the new match is the acceptance signal for both seats; `GameOverScreen` pumps/drains its own connection now, rebuilds a `GameScreen` from the rematch `WelcomeMsg`, hosts the offer prompt (Accept/Decline replace the Rematch row) and maps every `ErrorMsg` into its status line; the old fake-"Rematch" button became a real `Menu` button)
  - [x] Colors swapped on rematch (a finished match keeps both game-over seats **attached** for exactly its retention window (= reconnect grace, 11.2) via new `ClientState::GameOver`; the rematch callback fires from `Match::handleRematchAccept` with both clients referenced through the seats, the server dissolves the old match's seats (`detachAllSeats`) and pairs the same two clients again with colors swapped; bots auto-accept inside the request handler and are `startBot`-'d against their swapped color in the server's `onRematch`)

- [x] **11.8 Chat polish**
  - [x] Timestamps, name colors, per-player mute (client-local only: pure `client/src/chat_log.h` — `chatTimestamp` renders local "HH:MM" via `strftime`; `chatNameColor` picks a light-on-dark palette entry by FNV-1a over the name (own line fixed gold, system lines gray); mute list persists as `chat.muted` CSV in Config, managed from the chat input with `/mute <name>` and `/unmute <name>` slash commands (feedback lands as system lines; unknown commands get a usage line), and incoming `ServerChatMsg` from a muted name never enters the log — zero protocol/server changes)

- [x] **11.9 Login rate limiting (review follow-up R2)**
  - [x] Per-connection exponential backoff on consecutive `LoginMsg` failures (`server/src/client.h` gains `failedLogins`/`loginLockedUntil`; `handleLogin` refuses attempts inside the lock with "Too many failed logins — try again in N seconds"; wrong passwords and rejected registrations count, success resets; the lock arms from the SECOND failure so a single typo + immediate correct retry stays instant — 1 s doubling, 60 s cap; per connection, so reconnecting sidesteps it by design). E2e `RatedE2eTest.LoginThrottleBlocksRapidFailures` (throttled correct password, expiry + reset, fresh connection unthrottled)

---

## Phase 12 — Variants & community

**Goal:** more ways to play beyond standard 1v1 online.

**Exit version:** `v1.5.0`

▶ [Chess960](https://www.chessprogramming.org/Chess960)

- [x] **12.1 Chess960**
  - [x] Core: 960 start-position generator + flexible castling rules
  - [x] Protocol: `VARIANT` field in JOIN/WELCOME; server-side option
  - [x] Perft spot-checks against known FRC positions

- [x] **12.2 Puzzle mode**
  - [x] Offline mate-in-N set loaded from a bundled PGN file
  - [x] Hints/solution reveal; streak counter (uses the 9.1 menu entry)

- [x] **12.3 Arena tournaments**
  - [x] Server periodically pairs all queued players; standings broadcast
  - [x] Client tournament lobby + standings table

- [x] **12.4 Spectators**
  - [x] Server relays moves to observers watching a live match
  - [x] Spectator list/count surfaced to players (optional)

---

## Phase 13 — Release engineering

**Goal:** others can build, run, and deploy the project easily.

**Exit version:** `v1.6.0` — CI green, installable, dockerized server

- [x] **13.1 CI**
  - [x] GitHub Actions: Ubuntu matrix (GCC/Clang), cmake configure/build + ctest (`.github/workflows/ci.yml`: `actions/checkout@v4` → cache `build/_deps` keyed on the SFML/GTest pins → Release configure → `-j4` build → `ctest --output-on-failure`; both matrix COMPILE paths verified locally — GCC and Clang, zero warnings, 763/763 green; the suite is fully headless so no xvfb/audio steps needed)
  - [x] Install SFML build deps via apt (Ubuntu's `libsfml-dev` ships 2.x, so the job installs the SFML **dependency** headers — freetype/X11/udev/OpenAL/vorbis/flac — and lets the documented FetchContent 3.1.0 fallback build SFML itself; source in `.github/workflows/ci.yml`)

- [x] **13.2 Packaging**
  - [x] Headless server Dockerfile + docker-compose example (`Dockerfile` multi-stage: builder = ubuntu:24.04 + toolchain + SFML-dep headers + FetchContent SFML (only `sfml-network` compiles for the server target), runtime = ubuntu:24.04 + libsqlite3-0 + the fetched `libsfml*.so` copied from `_deps/sfml-build/lib` + `ldconfig`; `docker-compose.example.yml` mounts a `/data` volume for `--db`, example port 5555 + `chess-data` named volume; image build NOT verified locally — docker daemon unavailable, YAML parse-checked only)
  - [x] CMake install targets (binaries + assets); README install docs (top-level `GNUInstallDirs`: both binaries → bindir, whole `assets/` tree → `<prefix>/share/chess/assets`; runtime lookup extended with the `CHESS_ASSETS` env override as the FIRST root — `client/src/asset_paths.h` `assetRoots()` now feeds font loading, piece textures, and the puzzle loader's dirs, replacing the three hand-copied relative-root lists; README gained Installing + Docker sections)

- [x] **13.3 Portability pass**
  - [x] Audit fs paths, signal handling, compiler-specific code (inventory: `uci/src/engine.cpp` fork/exec/dup2/waitpid/kill + SIGPIPE = the only real "portability wall"; server `std::signal(SIGINT)` compiles everywhere; config default path gained `APPDATA`/`USERPROFILE` fallbacks; `App` font fallback now probes Linux dejavu → Windows arial/consola → macOS Helvetica; Clang build of the full suite verified zero-warning + 763/763 green)
  - [x] Windows/macOS build notes (full support if cheap) — checked "if cheap": NOT cheap for Windows (engine-bridge rewrite), documented honestly in README's "Platform support" table; macOS in principle supported, not CI-covered. Considered-and-dropped headless build options (server itself links the engine bridge for bots, so an engine-off toggle has no real target) — recorded in the top-level CMakeLists comment

- [ ] **13.4 Performance pass (optional)** — DELIBERATELY SKIPPED for v1.6.0 (AGENTS.md release-engineering bullet is the record): engine perft/gameplay is not measurably slow, and a movegen rewrite is high-risk-low-value heading into the 14–17 client overhaul arc. Revisit only after profiling shows a real need.
  - [ ] Profile before touching anything; only optimize what's measurably slow
  - [ ] Candidate: bitboards or precomputed attack tables for movegen
  - [ ] Perft suite must stay green after any change

---

## Phase 14 — Design system v2 (`client/`)

**Goal:** a token-driven visual foundation, chess.com-style, rebuilt on our own
assets: one theme module instead of the scattered hardcoded colors, a drawn
icon + avatar system, and every screen restyled through it. Recreation is
"in spirit" — same layout vocabulary, zero proprietary chess.com artwork,
fonts, or branding (our own palette values, our own drawings; README gains an
"inspired by" note in 14.5).

**Exit version:** `v1.7.0` — all screens restyled on tokens; zero behavior
change; 763-test suite stays green (CI discipline from 13.x continues)

- [x] **14.1 Design tokens (`client/src/ui/theme.h`)** — pure refactor: every token value mirrors the pre-14.1 hardcoded palette verbatim, so the phase's visual refresh (14.4/14.5) changes values HERE, not at the call sites
  - [x] Central module: spacing scale, type scale, color palette (dark background family, panel/card/button/accent tokens), component style structs; the single source every later phase paints from (header-only like `themes.h`/`chat_log.h`; `pal` namespace = shared families, `overlay` = promo-picker/dialog chrome shared by GameScreen/LocalGame/Puzzle, `type`/`space` scales)
  - [x] Migrate the widget layer first: `Button`/`TextField`/`Panel`/`Label`/`Checkbox` take styles from tokens instead of per-screen literal colors (`Button::setColors` 9-arg DELETED — `setStyle(const ui::ButtonStyle&)`; 15 `constexpr` variant factories `buttonDanger/Success/ConnectBlue/…` each copy one screens' exact trio; MoveNavigator paints from `pal` too)
  - [x] Sweep screens to remove magic colors/sizes connect/menu/settings/game screens draw directly — screens pass composition, tokens pass paint (~300 `sf::Color` literals down to 10, each of which is a screened-unique accent tone with a trailing comment naming it: ECO bronze, arena slate family, puzzle header sage, chat stub grays, eval-bar track; `boardview.cpp` + `chat_log.h` deliberately untouched — board semantics, 14.4/15.5 territory)
  - [x] Unit-testable helpers pure (color derivations, contrast pair checks) so theme lookups have tests without a window (`tests/client/theme_test.cpp`, +11 tests: mix/shade clamping + alpha preservation, contrast symmetry + 21:1 white/black, readable-text flip, panel contrast floors ≥ 4.0, type/spacing monotonicity, variant distinctness/statefulness; suite 774 green)
  - Note: today's legacy text-on-card contrast actually measures ~3.9–4.3:1 — the test floor is set at truth and 14.5's restyle should RAISE it (update the asserts when values change)

- [x] **14.2 Icon system (`client/src/ui/icons.h` + SFML-drawn registry)** — 8-icon procedural set, ours outright; the open-source-asset alternative was evaluated and declined (SFML 3 can't rasterize SVG; PNG atlases fight type-scale sizing; zero licensing overhead stays zero)
  - [x] Small procedural icon set drawn in SFML (clock, flag, draw-hand, flip, hint bulb, medal, watcher eye, back arrow) — no external sprite dependencies, sized by the type scale (`drawIcon` compositions of Rectangle/Circle/Convex shapes; sizing ONLY via `iconBox(textSize)`, never hand-picked pixels)
  - [x] Replace text-only button labels/headers where icons read better (11.6 theme cycler, HUD actions) (`Button::setIcon` leading slot; Resign→flag, Draw→hand, Leave/Menu→arrow, Hint→bulb, GameOver Menu→arrow, HUD clock glyphs, spectator eye+count, tournament banner medal; **labels always keep rendering beside icons** — an icon never replaces its text; two labels shortened to make slots fit at 96/140px: "Offer Draw"→"Draw", "Back to Menu"→"Menu"; Flip registered only, affordance lands with 15.x player cards)
  - [x] Keep letter fallbacks working everywhere (the 6.2.2 letter-drawing discipline extends to icons) (button draw drops the icon when the measured label cannot fit the remaining space — text wins over icons, hierarchy enforced in `Button::draw`; HUD count re-sync: the watcher figure left the name string for an eye-icon unit drawn by `drawPlayerCard`)
  - Tests: `tests/client/icons_test.cpp` (+5, headless layout math only — the draw path needs a target) — registry count, box monotonicity, slot containment, text-rect contract, narrow-control drop; suite 779

- [ ] **14.3 Avatar system (`client/src/avatars.{h,cpp}`)**
  - [ ] Procedural avatars: deterministic pattern + color derived from a seed (config `profile.avatar` for self, FNV over opponent name for the other seat); geometric shapes via `sf::Shape`, initial-letter fallback
  - [ ] Optional CC0 bundled avatar pack under `assets/avatars/` loaded through `assetRoots()` (13.2's env var applies) — missing pack degrades silently to procedural
  - [ ] Rendered on player cards (15.2 consumes it), leaderboard rows, spectator lobby rows

- [ ] **14.4 Board theme family refresh (`themes.h` palettes)**
  - [ ] Add chess.com-iconography-inspired named boards — Green/Brown/Blue/Purple — OUR color values, appended after the existing names (the 11.6 persistence ordering rule: never reorder existing entries)
  - [ ] Coordinates style toggle (extending `board.show_coordinates` beyond on/off: margin style = current, inside-squares style like chess.com), persisted like the theme
  - [ ] Settings screen theme row cycles the new family; preview swatches in the settings status text

- [ ] **14.5 Runtime chrome + settings restyle**
  - [ ] Window background, title rendering, version badge; rounded panels + pill buttons + underline textfields as component defaults from tokens
  - [ ] Settings screen restyled on the token layer (larger touch rows, sectioned panels); "inspired by chess.com" note lands in README

---

## Phase 15 — Game experience rebuild (`client/`)

**Goal:** the game experience rebuilt board-first — the in-game right panel
becomes a full stack (player card / clock / move list / actions) shared by
every board-bearing screen instead of each screen hand-drawing a panel.

**Exit version:** `v1.8.0` — GameScreen, LocalGameScreen, AnalysisScreen,
SpectateScreen (12.4) and PuzzleScreen render through the shared composer;
input parity tests green; zero protocol/server changes

- [ ] **15.1 Shared board composer (`client/src/screens/board_layout.{h,cpp}`)**
  - [ ] One layout module: top player card band, board rect, bottom player card band, right action stack — parameterized per screen (PuzzleScreen has no cards; Analysis has eval bar slot; Spectate is read-only)
  - [ ] Migrate the five board-bearing screens onto it; screens keep their event handling, composer owns geometry only (single writer rule for rect math)

- [ ] **15.2 Player card widget (`client/src/widgets/player_card.{h,cpp}`, 9.3's residual closure)**
  - [ ] Avatar (14.3) + name + rating suffix + captured-material row + material diff in one card; "thinking…" dot, connection icon, premove ghost badge states
  - [ ] Clock pill styling for 11.1's figures (m:ss / tenths / amber-red thresholds preserved; no display for unlimited)
  - [ ] Widget-level unit test: extend the client/widget test file — the 9.3 residual "player-card composition untested" closes here (headless composition via `sf::RenderTarget`-free logic slice)

- [ ] **15.3 Move-list panel restyle**
  - [ ] Alternating row shading + current-ply highlight + move-number gutter in the 9.3 navigator; scroll auto-follows the live ply (only when the user is at the tail — never yank during browsing)
  - [ ] ECO opening (11.5) stays the header; draw-offer/takeback prompt rows restyled as banners

- [ ] **15.4 Quick-message strip + chat restyle**
  - [ ] Three canned chips ("Good luck", "Good game", "Thanx for the game") above the chat panel, online GameScreen only; zero protocol change (she sends ordinary `ChatMsg`)
  - [ ] Chat panel restyle on 11.8's three-part line renderer (tokens for width/truncation math)

- [ ] **15.5 Input & motion polish**
  - [ ] Square-hover highlight under cursor, drag shadow fidelity, annotation arrow width/opacity from tokens, animated "waiting" spinner for engine/server think states
  - [ ] Board entrance animation optional (config `animation.entrance`, default off)

- [ ] **15.6 Consistency pass**
  - [ ] GameOverScreen / TournamentLobbyScreen / LeaderboardScreen / HistoryScreen restyled via the composer + tokens; promotion picker restyled (bigger cells, token stroke)

---

## Phase 16 — Engine intelligence: accuracy & game report (`client/`)

**Goal:** chess.com's most recognizable feature system on our own terms:
per-move classification, accuracy percentages, and a Game Report screen.
The engine stays OPTIONAL through the whole phase — features hide cleanly
when no engine is found (e2e/CI has none), exactly like the AGENTS "mock
engine" discipline.

**Exit version:** `v1.9.0` — finished local games and archived online games
classify; Report screen reachable from GameOverScreen; classification unit
tests abstain from engines (canned eval streams only)

- [ ] **16.1 Eval harness (`client/src/eval_harness.{h,cpp}`, engine required only at runtime here)**
  - [ ] Background-thread batch evaluation of every position of a finished game via `uci::UciEngine` (budgeted movetime per node, cancelable, no UI freeze — `tryGetBestMove`-style polling discipline from 8.2 never blocks the render loop)
  - [ ] Results cached in-process per `App` run; game identity = archive id or SAN-chain hash; re-opening the same game skips completed nodes
  - [ ] Engine-absent contract: `hasEngine()` false → callers hide all 16.* UI; harness never spuriously spawns engines from CI tests

- [ ] **16.2 Classification + accuracy (pure `client/src/move_quality.{h,cpp}`, the carefully-tested core)**
  - [ ] Win% conversion from cp/mate scores (sigmoid-equivalent mapping), per-move delta = win% given up from the mover's seat; thresholds → Brilliant/Best/Good/Inaccuracy/Mistake/Blunder/Book (book = 11.5's ECO prefix hit)
  - [ ] Accuracy% per side = a normalized win%-loss curve over `N` plies (the existing Glicko-2/norm discipline: clamp to 0-100)
  - [ ] Unit tests: a canned `MoveEvalStream` fixture vector drives the entire classifier; thresholds asserted stable (tuning changes REQUIRE test updates — same discipline as the 2.x perft constants); no engine binary in test envs

- [ ] **16.3 Game Report screen (`client/src/screens/report_screen.{h,cpp}`)**
  - [ ] Post-game panel: accuracy bars per side, class counts per piece legend, opening name (11.5 lookup), "worst moves" list jumping the navigator; reuses 9.3 navigator for board replay
  - [ ] Stored vs computed: reports are only computed on demand (button "View report" when engine present + game finished), never silently in the background during play
  - [ ] GameOverScreen integration: report button appears next to Rematch where 16.1 contract is satisfied

- [ ] **16.4 Move-list coloring + analysis upgrades**
  - [ ] Classified move list: class abbreviation chips/tints on each SAN cell (16.2's labels), rendered via 15.3's restyled rows; archive-replay and spectate views honor the same chip layer when a report was computed
  - [ ] AnalysisScreen: multi-line best PV arrows toggle, node markers on the 8.5 eval graph at classified points, per-position annotation arrows/circles persisted client-local via the 9.5 config file, keyed by game identity

---

## Phase 17 — Puzzles, personas & play hub (`client/` + tiny `server/` config)

**Goal:** the remaining chess.com hub surfaces: Puzzle Rush timed marathons,
daily puzzles/streaks, persona bots, and a full-screen "Play" hub that
retires the old ConnectScreen-first flow.

**Exit version:** `v2.0.0` — the hub is the client's entry surface; ConnectScreen
fields survive as a collapsible drawer; suite green; protocol untouched
(bot personas on the server are config-only, Guests still pair like humans)

- [ ] **17.1 Puzzle Rush & Survival (`client/src/screens/rush_screen.{h,cpp}`, extends 12.2)**
  - [ ] 3-minute and 5-minute rush + endless Survival mode: fail-or-skip advances, streak-and-score counter, time pill (11.1's clock styling)
  - [ ] Scores persisted per mode as `puzzle.rush_{3m,5m,survival}_best` config keys; "beat your best" status banner; solution-reveal still zeroes the streak (12.2 discipline carries over verbatim)
  - [ ] Feed shuffling deterministic per session (seed stored in config; shuffle source = the 12.2 loader order)
  - [ ] Input discipline: the 12.2 `solverTurn()` guard extends to rush timer expiry (a stall mid-animation can never double-apply)

- [ ] **17.2 Daily puzzle + streaks**
  - [ ] Date-seeded deterministic selection from the bundled set (year+day hash → shard into the 12.2 list); "Daily" card entry opens PuzzleScreen in daily mode; solving marks `puzzle.daily_last`/`daily_streak` (config) and days skipped reset the streak
  - [ ] All-time solves + daily stats panel inside PuzzleScreen header

- [ ] **17.3 Persona bots (`client/src/bot_personas.{h,cpp}` + server config table)**
  - [ ] Named difficulty personas for LOCAL play — fresh, neutral names of our own (NOT chess.com's bot cast): depth presets + style caps (an engine-option set like Skill Level / move-time budgets; zero protocol)
  - [ ] Server-side configurable persona table (`--bot-personas` config) mapping persona names to depth/preset pairs so spawned server bots (8.3's `botSpawnQueue` path, unchanged) play at matching strength — still zero wire changes
  - [ ] Persona cards from 14.2/14.3 render like player cards on the 15.2 layout

- [ ] **17.4 Play hub (`client/src/screens/play_hub_screen.{h,cpp}`)**
  - [ ] Chess.com-style home: quick-queue TC cards (bullet/blitz/rapid/classical from 11.1's presets), variant cards (standard/Chess960 via 12.1, arena from 12.3), vs-Bot persona section, Archive/Leaderboard/Puzzles/Spectate/Settings entries (9.1 menu entries all move here, none lost)
  - [ ] ConnectScreen survives as a collapsible "connect" drawer inside the hub (last_host/port/name pre-filled from 10.2's persistence, time-control cycler replaces the 11.1 connect-screen cycle button)
  - [ ] App entry point swaps to the hub (config `general.entry` opt-out keeps the old menu for one version; menu screen stays functional)

- [ ] **17.5 Final consistency + release pass**
  - [ ] Full sweep: every screen paints from tokens (no stragglers flagged by a debug-only "token violation" overlay build flag); sounds/animation settings apply everywhere
  - [ ] README screenshots + "inspired by chess.com" note + version-table close-out; tag v2.0.0

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
