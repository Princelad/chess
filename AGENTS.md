# AGENTS.md — C++ Multiplayer Chess

## Build & test commands

```sh
cmake -S . -B build          # configure (fetches SFML 3.1.0 + GoogleTest v1.17.0 if needed)
cmake --build build          # build everything
ctest --test-dir build       # run all tests
```

Single test:
```sh
ctest --test-dir build -R "MoveGen.KingCastling" --output-on-failure
```

When adding a new `.cpp` file, register it in the relevant `CMakeLists.txt`:
- `tests/CMakeLists.txt` → `add_executable(chess-tests ...)` for test files
- `core/CMakeLists.txt` → `add_library(chesscore STATIC ...)` for rules engine sources
- `client/CMakeLists.txt` → `chessclient` for GUI sources, including `src/widgets/`

## Project structure

```
chess/
├── core/          # chess rules engine — pure C++, no I/O
│   ├── include/chess/   # public headers
│   └── src/             # implementations
├── net/           # shared network protocol (sf::Packet)
├── db/            # SQLite persistence: schema, auth hashing, player/game stores, Glicko-2
├── server/        # headless TCP match server
├── client/        # SFML 3 GUI client
│   ├── src/screens/     # screens + shared HUD
│   ├── src/widgets/     # reusable widgets: Button, TextField, Panel, Label, MoveNavigator, Checkbox
│   └── src/             # BoardView, Connection, board_interaction (Annotations + DragTracker),
│                        # themes.h (BoardTheme/colors), sfx.h (procedural SoundManager),
│                        # config.{h,cpp} (INI persistence)
├── tests/         # GoogleTest suite (ctest)
│   ├── core/            # core engine tests
│   └── client/          # client/widget tests
├── assets/pieces/ # cburnett PNG sprites (CC BY-SA 3.0)
├── TODO.md        # full task list with phase/task numbering
└── AGENTS.md      # this file
```

Library targets: `chesscore` (rules), `chessnet` (protocol), `chessdb` (SQLite persistence, system libsqlite3), `chessuci` (UCI engine), `chessclient` (GUI). Widgets live in `chessclient`; tests link it directly.

## C++17 gotchas (already bitten)

- **`constexpr operator==`:** `explicit constexpr operator==` is valid; do NOT `= default` on `==`/`!=` in C++17 (no defaulted comparison operators).
- **`Piece::None()`:** use a static member function, not a static data member (incomplete-type issue at point of declaration).
- **`std::optional<Board>` dereference:** `Board::fromFen()` returns `std::optional<Board>`. Dereference with `*board` or `board->`, never `board.`.
- **`find_if_not` + inverted predicate:** `find_if_not(begin, end, isNotSpace)` finds the first *space*, not the first non-space. For trimming use `isSpace` with `find_if_not` (or `isNotSpace` with `find_if`).
- **Chat enter-send:** in `GameScreen`, `chatInput_` is focused by click; while focused, `handleEvent` forwards `KeyPressed` into `chatInput_.handleEvent` (Enter fires its `onCommit` → `sendChat()`). Escape unfocuses first. Text is typed via `TextEntered`, so backspace arrives as `TextEntered('\b')`, not a key press.
- **Keyboard play (9.6):** in `GameScreen`/`LocalGameScreen`, arrows move a board cursor (`keyCursor_`, wraps at edges, syncs from mouse presses); Enter selects own piece then commits to the cursor square; `[`/`]` step the move list (replacing the navigator's Left/Right in those screens — Analysis still uses arrows); `F` calls `BoardView::toggleFlipped()`; `?`/`Slash` toggles the help overlay (`showingHelp_`, closes first on Esc). Pure mapping lives in `client/src/keyboard.h` (`keyToAction`, `moveCursorStep`). Cursor rendering rides on `HighlightState::cursorSquare`.
- **INI config:** `Config` flattens keys as `section.key`, sees `[section]`/`key = value`, strips `#`/`;` comments, and persists to `~/.config/chess/config.ini` (created on save). Settings are applied and re-saved immediately from the Settings screen; `sound.{volume,muted}`/`animation.{enabled,duration}` are consumed by the SFX manager and move animator (9.7/9.8). Chess-clock presets live in `general.time_control` (11.1, cycled on the ConnectScreen) and join messages carry them.
- **Chess clocks (11.1):** protocol is append-only v2 — `JoinMsg`/`WelcomeMsg` carry the "m+s" time-control string (empty = unlimited; `net::parseTimeControl` in messages.h refuses malformed strings to unlimited, and the server canonicalizes the payload before pairing/storage). The matchmaker pairs only equal TC strings. `server/src/clock.{h,cpp}` is a pure `MatchClock` fed injected timestamps (unit-testable without sockets): White's clock starts at `Match` construction, each accepted move charges the mover's elapsed time plus the Fischer increment, and the broadcast `ServerMoveMsg.remainingMs` (0 = unlimited) tells both clients the on-move player's fresh remaining. `Match::pollClock` runs every server-loop pass and the select wait shrinks to 100 ms whenever a clock is live, so `GameOverReason::Timeout` flags fire promptly; rated-flow treats it like any decisive result. Client side: `Hud::syncClocks` re-syncs at each `ServerMoveMsg` and the side on the move ticks locally between syncs; unlimited games show nothing. Per-class ratings (bullet/blitz/rapid/classical keyed by `classifyTimeControl`) remain a pending follow-up — the single "unlimited" class still updates today.
- **Reconnect & resume (11.2):** `Match` holds two private `Seat`s (`present` `Client*` + join-persistent name/playerId/rating snapshot/deviation + `resumeToken`); all sends go through `sendToPresent()` (absent seats receive nothing), and every observer reads seat data via `Match::seat(Color)` (`SeatView`) rather than the still-present `white()/black()` pointers — those are null whenever a seat is away. Protocol v2 append-only: client tag `Resume = 12`, server tags `ResumeState = 14`/`OpponentReconnecting = 15`/`OpponentResumed = 16`; `WelcomeMsg` grew `resumeToken` (trailing fields must be serialized AND deserialized — legacy-shaped packets fail as intended). Grace flow: solo disconnect = seat absent + `OpponentReconnectingMsg{ceil(graceSec)}` to the survivor + server-side deadline (`ServerConfig.reconnectGraceMs`, `--grace-ms` e2e knob); absent pieces still clock-run, so `pollClock` can flag during grace — wildcard `pollReconnect(nowMs)` ends the game `opponent wins / Disconnection` when the deadline lapses, and both seats absent in one pass = instant unrated `Abort`+`Disconnection`. `ResumeMsg{token}` routing lives in the `Connected`-state branch (`handleResume`): `Match::resume` rebinds the seat (or, for a finished match kept in retention for exactly the grace window, returns `ReboundOver` and the server replays ResumeState + `GameOverMsg` + fresh `ProfileMsg`; server.cpp erases matches only past `retentionEnded`). `endGame()` notifies present seats, runs the gameOver callback while seats are still attached (rated/archive hook uses seat snapshots, so results rate even after someone left), and only then detaches clients — reorder that and profile re-sends break. Client: GameScreen owns `resumeToken_` (from WelcomeMsg via ConnectScreen); a mid-game drop arms a Backoff→Connecting→AwaitingResume loop (1 s retries, ~30 s window, instant give-up on an `ErrorMsg` while waiting) and `applyResume()` rebuilds the screen in place by replaying the SAN chain (`hud_.setGame`) with `Hud::syncClocks(whiteMs, blackMs, sideToMove)` — everything after `switchScreen` must `return` immediately, so the rebuild never switches screens. Never touch a seat's stale `Client*` after erase: presence, not the erased object, is the truth.
- **Takeback (11.3):** classic two-ply frame — a request is legal only when the asker is on the move with ≥2 plies and the opponent is seated; acceptance retracts the asker's last ply plus the opponent's reply, so the asker is again on the move in the prior position. The server rolls back by truncating `Match::sanHistory_` by 2 and re-chaining `san::fromSan` from the start position — the same reconstruction as a token resume; `Board::undoMove` is NOT used (it restores movegen-internal state, not match-level scope). Protocol append-only: client `TakebackRequest = 13`/`TakebackAccept = 14`/`TakebackDecline = 15`, server `ServerTakebackRequest = 17`/`TakebackAccepted = 18` (carries the full remaining SAN list guarded by `MaxResumeMoves = 512`)/`ServerTakebackDecline = 19`. Lifecycle: one pending per match (`takebackPending_` = asker seat index), dissolved by the next accepted move, a decline (the asker's own decline doubles as a self-cancel — its notice always lands on the asker's seat), a disconnect (either seat), or `endGame`; a request while the opponent is mid-grace fails fast (`ErrorMsg` "The opponent is away"), never queues. Bots auto-accept inside `Match::handleTakebackRequest` (no round trip; safe because a bot never thinks while the asker is on move). Clocks are NOT refunded — after the two `completedMove` charges, `MatchClock.onMove` already equals the post-rollback `sideToMove`, so the rebuilt turn just keeps running; no clock surgery anywhere. Client: GameScreen holds `takebackOfferPending_` (opponent's prompt shares the draw offer's accept/decline buttons; draw wins visibility while both pending); `applyTakeback(vec<string>)` rebuilds via `hud_.setGame` + `goEnd` (11.2's `applyResume` got the same `goEnd` — it used to sit at ply 0 until the first click), calls `anim_.cancel()` (new `MoveAnimator` API) and drops stale selection/drag/promo state. History-append rule unchanged: rollbacks replay via `setGame`, never `appendMove`.
- **Leaderboard (10.4):** `PlayerStore::topPlayers(limit, minGames = 1)` ranks accounts rating-DESC / name-ASC, only those with ≥1 finished rated game. Wire: `LeaderboardRequestMsg{limit}` → `LeaderboardListMsg{players}` (protocol v2, append-only tags; `MaxLeaderboardEntries = 100` guard on deserialize). The server answers in every connection state beside the archive handlers (server clamp 100 == wire guard; without `--db` it replies `ErrorMsg` "This server has no leaderboard"). Client: `LeaderboardScreen` (read-only list panel, "(you)" own-row highlight) entered via the ConnectScreen "Leaderboard" button; the connect flow's `archiveBrowse_` became `BrowseTarget {None, Archive, Leaderboard}` sharing the renamed `ConnectingBrowse` phase. Drive-by: the archive server clamp was also aligned from 200 to 100 so replies can never exceed the `ArchiveListMsg` wire guard.
- **Game archive (10.3):** the server answers `ArchiveRequestMsg{limit, mineOnly}` / `ArchiveGetMsg{id}` in any connection state with `ArchiveListMsg` / `ArchiveGameMsg` (or `ErrorMsg`); `mineOnly` for guests degrades to the global list. `Match::sanHistory_` feeds the row insert in the game-over hook — only games with ≥1 move are archived (`Abort` games with zero moves aren't). Replay rebuilds moves by chaining `san::fromSan` over the record's SAN list (honouring `startFen`); PGN export lives in pure `client/src/archive_pgn.h` (`gameToPgn`), copy via `sf::Clipboard`. Entry points: ConnectScreen side button (connects without queueing; with a password it logs in first, `ConnectingArchive` phase) and GameOverScreen button (pushed so Esc returns; disabled without a live connection).
- **Move history records ONCE:** the online server echoes your own move back via `MOVE`, so `GameScreen` must NOT append a local move — the echo during `update()` is the single append point via `Hud`/`MoveNavigator::appendMove`. `LocalGameScreen` (no server echo) records its own moves.
- **Accounts & ratings (10.2):** protocol is **v2**. `LoginMsg{name, password}` (unknown name + non-empty password = transparent auto-register; empty password = guest, joins via plain `JoinMsg` as before) gets a `ProfileMsg{name, rating, deviation, isGuest}` reply — also re-sent after rated updates. `WelcomeMsg` carries `{rating, deviation, rated}` (opponent's). Booleans on the wire travel as validated 0/1 bytes (`writeFlag`/`readFlag` in protocol.cpp). Rated updates happen in `PlayerStore::applyRatedGame` (one transaction over both rows + W/L/D counters, pre-period Glicko-2 inputs), wired via `Match::setGameOverCallback` in `runServer` (v1.3.1 audit follow-up); only `WhiteWins/BlackWins/Draw` rate — aborts (both disconnects) and guests/bots are never rated, while a single disconnect mid-game loses and still rates. Guest `JoinMsg` names are rejected when they collide with an account (NOCASE) — password-gated accounts must not be spoofable in chat/archive (`RatedE2eTest.GuestJoinCannotTakeAnAccountName`); logged-in clients skip the check since `playerId` bypasses the payload name. `ConnectScreen` phase `AwaitingProfile` keeps a failed login's socket alive for retries; `App::profile()` holds the latest `ProfileMsg` and `App::setLastConnection` persists `general.last_host/port/name` (never the password). `TextField::setMasked` renders '*' for the password field. Idle-period RD growth (10.2 follow-up): `handleLogin` runs `PlayerStore::applyIdleGrowth(id, now)` before `ProfileMsg` — one Glicko-2 no-game period per full idle day since `players.last_rated_at` (schema rev 2, `ALTER`-migrated from rev 1), capped at 5000 periods/RD 350; never-rated rows no-op. Rating classes are taxonomized in `db/rating_class.h` (`classifyTimeControl`, "unlimited" until 11.1 clocks give games a TC).
- **Premoves (11.4):** online `GameScreen` only. When `!myTurn_`, a committed drag/click/Enter move becomes a `Premove{fromFile/Rank, toFile/Rank}` (single slot in `GameScreen::premove_`, latest replaces) instead of a send; `trySendMove` is the queue entrance, so keyboard-arrow premoves work too. Flush is exactly one point: the `ServerMoveMsg` handler calls `flushPremove()` right after `myTurn_ = true` — the echo remains the single history-append point (a flushed premove must never also `appendMove` locally). Resolution is pure `client/src/premove.h` `resolvePremove(board, premove)`: the legal move matching from/to at flush time; capture-frames onto empty squares, own-piece landings and pin breaks drop with "Premove illegal"; king-by-two resolves to the castle; promotion frames send the queen move directly (no picker can exist for a hypothetical position). Ghost rendering is `BoardView::drawGhostPiece` (110-alpha piece at the landing square) + `HighlightState::premoveFrom/premoveTo` tints, drawn only when `!myTurn_ && navigator().atEnd()`. Cancel = right-click/Esc/Space (`Deselect`); cleared on `GameOverMsg`, `applyTakeback`, and `applyResume`. No protocol changes — a flushed premove is a plain `MoveMsg`.
- **Move animation (9.7):** pieces slide via `MoveAnimator` (in `client/src/move_animator.h`, header-only) — capture the pre-move state, advance `update(dt)` on the fixed-timestep, read eased `progress()` in `draw()` (frame-independent). Triggers are exactly the append points: the online `ServerMoveMsg` handler and `LocalGameScreen::applyMove`/`applyEngineMove`. `BoardView::drawPieces` takes a `const MoveAnimator*` (Analysis passes `nullptr`) and skips destination squares while animating. Duration/enable come from config keys `animation.enabled`/`animation.duration` (0/false = instant snap); no animation while replay-stepping. On the deciding move, the game-over screen switch is delayed via `GameOverTransition` (`client/src/game_over_transition.h`) so the final animation and mate/capture sound play out (`gameOverDelaySec` floors all delays at 0.55s).
- **Sound (9.8):** `client/src/sfx.h` ships `SoundManager` (App-owned, `app_.sounds()`) with procedurally synthesized 16-bit/44.1 kHz clips (no asset files) — move bump, capture thock, two-tone check, descending mate. Pure `classifyMove(before, move)` picks the clip with precedence Checkmate > Check > Capture (incl. en passant) > Move, and is fired at the same append points as 7 animation. Requires the SFML `Audio` component — root `CMakeLists.txt` requests it (`SFML::Audio`/`sfml-audio`), and the FetchContent fallback builds audio too (needs OpenAL on Linux). SFML 3.1 nuances: `sf::Sound` has no default ctor (initialize with a buffer), `SoundBuffer::loadFromSamples` takes a 5th channel-map arg (use `{sf::SoundChannel::Mono}`), samples are `std::int16_t`.
- **Board input is press→release:** left press starts a `DragTracker` + selects own piece; the move is committed on release (drag-resolution or click-target). Left press clears arrows/circles; right press deselects, right drag draws arrows, right tap toggles circles. Annotations live in `BoardAnnotations` (per-square, `BoardView`-rendered) and are cleared when the board leaves the live position.
- **Auto-queen defaults ON** (config key `general.auto_queen`, toggled via the picker's `Checkbox` and the Settings screen); when enabled a promotion commits the Queen move directly and skips the picker.

## 0x88 board reference

- Square encoding: `squareOf(file, rank) = rank * 16 + file`
- `offBoard(sq)`: `sq & 0x88` is nonzero
- Knight offsets: `{+33, +18, +14, +31, -14, -31, -18, -33}` (rank\*16 + file delta)
- King offsets: `{1, 17, 16, 15, -1, -17, -16, -15}`
- Rook directions: `{16, -16, 1, -1}`
- Bishop directions: `{17, 15, -15, -17}`
- FEN rank parsing: `rankIndex=0` is rank 8 (top of FEN string); each rank must sum to 8 squares.

## Versioning

SemVer-style, pre-1.0: MINOR bump per phase, PATCH for bugfixes within a phase. See `TODO.md` version table.

| Phase | Version | Status |
|---|---|---|
| 0 — Project setup | `v0.1.0` | done |
| 1 — Core rules engine | `v0.2.0` | done |
| 2 — Core unit tests | `v0.3.0` | done |
| 3 — Network protocol | `v0.4.0` | done |
| 4 — Server | `v0.5.0` | done |
| 5 — Client networking | `v0.6.0` | done |
| 6 — SFML GUI | `v0.7.0` | done |
| 7 — Integration & polish | `v1.0.0` | in progress |
| 8 — Engine integration (UCI) | `v1.1.0` | planned |
| 9 — Client UI/UX overhaul | `v1.2.0` | done |
| 10 — Persistence & ratings | `v1.3.0` | done |
| 11 — Multiplayer QoL | `v1.4.0` | planned |
| 12 — Variants & community | `v1.5.0` | planned |
| 13 — Release engineering | `v1.6.0` | planned |
