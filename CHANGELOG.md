# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] — 2026-10-31

### Fixed

- **The mod actually works now.** The v0.1.0 release shipped a scaffold-grade binary; v0.2.0 is a complete rewrite verified line-by-line against the LeviLamina **v26.51.9** SDK headers, with the pure voxel engine covered by a unit test suite (JSON round-trip, rotations on all axes, scaling, mirroring, corruption rejection).
- **"The .so is not there":** LeviLamina v26.x publishes loaders for Windows only, and native mods ship as `levi-create.dll` (the platform's native library). The levipack now contains the packed mod folder produced by LeviLamina's own `modpacker` (`manifest.json` + `levi-create.dll` + pdb), so no file can go missing. A Linux `.so` is impossible to load on current LeviLamina — see the README section "Where is the .so?".
- Commands are now registered both on `ServerCommandRegisterEvent` (survives command-registry rebuilds) and on enable (hot-enable safe).
- Command parameter structs moved to external linkage (anonymous-namespace structs silently break `boost::pfr` reflection used by the command system).
- Form button callbacks capture by value (v0.1.0 pattern risked dangling references).
- Fixed an off-by-half sampling bug in scaling that dropped edge voxels and broke 1/16 miniatures of small models.

### Added

- `/vc` command (alias `/levicreate`) with 20+ subcommands: `help, menu/ui, wand, pos1, pos2, copy, cut, paste [scale] [yaw] [pitch], move, rotate (any degree), scale, flip, mini, expand, fill, replace, clear, undo, info, count, save, load, delete, list`.
- Interactive form UI (`/vc menu`): selection panel, transform studio (dropdowns/sliders/any-angle input), clipboard & paste panel, miniature/bits panel, model library browser with load/paste/delete, help.
- Wand tool: iron axe; sneak+right-click = pos #1, right-click = pos #2 (cancels the interaction).
- Voxel engine (`src/core/VoxelModel.cpp`, dependency-free, unit-tested): palette grid, RLE JSON model format, exact rotations at 90° multiples on X/Y/Z, rasterized rotations at any other angle, scale 1/16…16 with true-proportion miniatures, mirroring.
- Undo support (8 levels) for all destructive operations.
- Volume safety rails: 2,000,000 blocks / 512 per axis, with clear in-game error messages.
- CI: builds the levipack on every push and attaches `levi-create-server-windows-x64.zip` to every `vX.Y.Z` tag release.

### Changed

- Complete rewrite of the mod layer on top of the v0.1.0 knowledge base; `docs/` (LeviLamina developer-guide digests) retained and kept current; `AGENTS.md` rewritten for the new layout.

## [0.1.0] — 2026-10-30

### Added

- Initial mod skeleton generated from the official LeviLamina "Create Your First Mod" tutorial.
- `docs/` knowledge base digesting all LeviLamina developer guide pages with source URLs.
- `AGENTS.md` operating instructions for AI coding agents.
- First voxel engine prototype (`src/levi_create/`) — superseded by the v0.2.0 rewrite.
