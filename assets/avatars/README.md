# avatars (bundled avatar pack)

Small square persona tiles — animals and a maple leaf — used by the
14.3 avatar system. The loader (`client/src/avatars.cpp`) picks one
deterministically per player seed; delete any file (or the whole
directory) and every seat degrades to the procedural identicon/initial
emblem instead.

- Source: **Twemoji**, the open-source emoji artwork released by
  Twitter, Inc. (maintained fork: `jdecked/twemoji`), assets served as
  72×72 transparent PNGs; local copies are unmodified.
  <https://github.com/jdecked/twemoji>
- License: graphics under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
  (the project code in that repository is MIT). Attribution — this file —
  fulfills the license's credit requirement; no endorsement is implied.
- Tile provenance: each `*.png` keeps its Unicode codepoint name
  (e.g. `1f431.png` = U+1F431 cat face), so a future refresh from the
  upstream repository is unambiguous.

Adding your own tiles: any square PNG recognized is first-root-sorted
into the pool (`assets/avatars/` under the first `assetRoots()` hit, see
13.2's `CHESS_ASSETS` override). Filenames other than the `*.png`
tiles are ignored by the loader.
