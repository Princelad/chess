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

When adding a new `.cpp` test file, register it in `tests/CMakeLists.txt` in the `add_executable(chess-tests ...)` list.

When adding a new `.cpp` source, register it in `core/CMakeLists.txt` (`add_library(chesscore STATIC ...)`).

## Project structure

```
chess/
├── core/          # chess rules engine — pure C++, no I/O
│   ├── include/chess/   # public headers
│   └── src/             # implementations
├── net/           # shared network protocol (sf::Packet)
├── server/        # headless TCP match server
├── client/        # SFML 3 GUI client
├── tests/         # GoogleTest suite (ctest)
├── assets/pieces/ # cburnett PNG sprites (CC BY-SA 3.0)
├── TODO.md        # full task list with phase/task numbering
└── AGENTS.md      # this file
```

`core/` is the only library target (`chesscore`). All test files go in `tests/core/`.

## Git workflow

- **Branch naming:** `v{major}.{minor}.{patch}-task-{task_id}` (e.g. `v0.2.0-task-1.3.3`)
- **One branch per task**, forked from the phase base (e.g. `v0.2.0`)
- **Commit message format:** `core: short description (1.3.3)` — terse, no body for small tasks
- **PR flow:**
  ```sh
  git push -u origin <branch>
  gh pr create --base <phase-branch> --head <branch> --title "..." --body "..."
  gh pr merge <N> --merge --delete-branch   # fast-forward, deletes local + remote branch
  ```
  After merge, check out the phase base (`git checkout v0.2.0`) and `git pull`.
- Tags (`v0.1.0`, `v0.2.0`, ...) are applied at phase completion.

## C++17 gotchas (already bitten)

- **`constexpr operator==`:** `explicit constexpr operator==` is valid; do NOT `= default` on `==`/`!=` in C++17 (no defaulted comparison operators).
- **`Piece::None()`:** use a static member function, not a static data member (incomplete-type issue at point of declaration).
- **`std::optional<Board>` dereference:** `Board::fromFen()` returns `std::optional<Board>`. Dereference with `*board` or `board->`, never `board.`.

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
| 7 — Integration & polish | `v1.0.0` | planned |
| 8 — Engine integration (UCI) | `v1.1.0` | planned |
| 9 — Client UI/UX overhaul | `v1.2.0` | planned |
| 10 — Persistence & ratings | `v1.3.0` | planned |
| 11 — Multiplayer QoL | `v1.4.0` | planned |
| 12 — Variants & community | `v1.5.0` | planned |
| 13 — Release engineering | `v1.6.0` | planned |
