> **ℹ️ v0.2.0 status note:** this document describes the v0.1.0 engine design.
> The v0.2.0 implementation lives in `src/core/` (`VoxelModel`, `PlayerState`, `WorldOps`, `PlayerOps`)
> with the command layer in `src/cmds/` and UI in `src/ui/`. The LeviLamina API knowledge below
> remains accurate and current.

# Feature: Voxel Build Engine

This is the heart of LeviCreate: an unlimited voxel building system for
Minecraft Bedrock (LeviLamina) inspired by Java's Chisel & Bits + Axiom.
This document explains how it works so any agent can extend it safely.

## Module map

```
src/levi_create/
├── core/                       Pure C++20, ZERO Minecraft dependencies
│   ├── VoxelModel.h/.cpp       3D palette-compressed voxel grid + .lcmodel JSON
│   ├── VoxelShape.h/.cpp       Procedural generators (box/sphere/cylinder/cone/
│   │                           pyramid/torus/diamond/line, hollow shells, gradients)
│   ├── VoxelTransform.h/.cpp   Scale (1/16..16), any-degree 3-axis rotation,
│   │                           mirror — nearest-neighbor voxel resampling
│   ├── Session.h/.cpp          Per-player state: corners, clipboard, undo/redo,
│   │                           brush, material (SessionManager, keyed by UUID)
│   └── ModelStore.h/.cpp       models/*.lcmodel library (save/load/list/delete)
├── mc/                         The ONLY file that touches Bedrock internals
│   └── BlockAccess.h/.cpp      currentLevel, blockSource, tryGetBlock, blockAt,
│                               setBlock, playerFeet, message  ← [ADAPT] here
├── ops/
│   └── WorldOps.h/.cpp         capture/fill/clear/copy/cut/paste/stamp/move/
│                               stack/paintSphere/undo/redo + async stamp queue
├── ui/Forms.h/.cpp             SimpleForm/CustomForm panels (main menu, shape
│                               wizard, region tools, transform studio, brush,
│                               palette, model library, help)
├── commands/Commands.h/.cpp    /lcreate (alias /voxel) command tree
└── LeviCreate.cpp              Lifecycle: config, event subscriptions, wiring
```

## Data flow of one build operation

```
Command / Form
   │  (server thread)
   ▼
Session (corners / clipboard / material)
   │
   ▼
core::VoxelModel ── applyTransform (scale → rotate → mirror, in memory)
   │
   ▼
WorldOps::planStamp   reads world state, builds HistoryEntry list
   │                   (pos, oldBlock, newBlock) — undo record
   ▼
PendingJob queue ──── tickQueue() on LevelTickEvent applies
   │                  ≤ config.maxChangesPerTick writes per tick
   ▼
mc::setBlock (BlockSource)  → world updated smoothly, no lag spike
```

Key invariants:

* One operation = one `HistoryBatch`; `undo`/`redo` replay it in reverse.
  Starting a new op (or undoing) cancels the player's pending writes first,
  so history and world state can never diverge.
* When a player disconnects, their session is dropped and their pending
  writes are discarded *with* their history — always consistent.
* `VoxelModel` values: `0` = air, `1..palette.size()` = palette block,
  `0xFFFF` = "keep existing world block" (reserved for overlay stamps).

## Shape generation

`generateShape(ShapeParams, maxVolume)` evaluates every voxel of the bounding
box against a predicate (`Generator::inside`). The same predicate is used for
the hollow rule: a voxel is on the shell when any voxel within Chebyshev
distance `shellThickness` fails the predicate — this keeps every hollow shape
closed and works uniformly for all 8 shape types.

`blocks` supports vertical gradients: the palette is cycled per Y layer.

## Transforms (any degree + tiny builds)

`applyTransform` runs three passes:

1. **Scale** — nearest-neighbor resample to `round(dim * scale)` per axis
   (clamped 1/16..16). Scale < 1 is the "tiny build" feature: a 64-block
   statue becomes a 4-block miniature at 1/16.
2. **Rotate** — output bbox is the axis-aligned hull of the 8 rotated
   corners (with a `1e-4` epsilon so `ceil` fp noise cannot inflate dims);
   each output voxel inverse-rotates (Rz⁻¹·Ry⁻¹·Rx⁻¹) and samples the source
   nearest-neighbor. Arbitrary-degree rotations produce the classic voxel-art
   resampling (a 3-block line at 45° legitimately needs ~5 blocks to stay gap-free).
3. **Mirror** — full remap (never in-place swapping; that double-applies).

## Interactive UI

* `/lcreate ui` (or sneak+interact with a **stick**) opens the main panel.
* **wooden_axe** = selection wand: interact → corner 2, sneak-interact → corner 1.
* **carrot_on_a_stick** = brush: interact paints a sphere (sneak = hollow).
* All panels reopen themselves after an action (Axiom-style flow), and the
  CustomForm callbacks parse `CustomFormElementResult`
  (`std::variant<monostate, uint64, double, string>`) through shared
  `u64/dbl/str` helpers in `ui/Forms.cpp`.

## Minecraft API isolation ([ADAPT] convention)

Every Bedrock API call lives in `mc/BlockAccess.cpp`, marked with `// [ADAPT]`.
If a newer LeviLamina renames a method (`getDimensionBlockSource`,
`Block::tryGetFromRegistry`, `BlockSource::setBlock(pos, block, 3, nullptr)`,
`Block::getTypeName`, `Level::getPlayer(ActorUniqueID)`), fix it there and
nowhere else — the rest of the mod compiles untouched.

Event/registry facts this feature relies on (verified against the digests in
`docs/`):

* Commands register inside `ServerCommandRegisterEvent`
  (`CommandRegistrar::getInstance(false).getOrCreateCommand(...)`).
* `PlayerInteractBlockEvent` is cancellable — tool uses call `event.cancel()`.
* The disconnect event is `PlayerDisconnectEvent` (not "PlayerLeftEvent").
* Tick work happens in a `LevelTickEvent` listener; `ll::service::getLevel()`
  and `Level::getPlayer(ActorUniqueID)` resolve players at apply time.
* Forms run their callbacks on the server thread — safe to touch the world.

## Verifying changes without Windows

The whole `core/` layer is pure C++20 and compiles on Linux with only
nlohmann/json:

```bash
g++ -std=c++20 -O2 -Isrc -I<vendor>/nlohmann \
    test_engine.cpp src/levi_create/core/*.cpp -o test && ./test
```

Run the engine test-suite before every push (shape volumes, 90°/45° rotation
dim math, scale 1/16 tiny builds, mirror remap, JSON round-trip, store
sanitize rules). The `mc/`, `ops/`, `ui/`, `commands/` layers are
syntax-checked by the Windows CI build.

## Roadmap (suggested next steps for agents)

1. **Block-state support** — resolve `"wool[color=red]"` via
   `Block::tryGetFromRegistry` state overloads; extend the palette UI.
2. **Particle preview** — ghost-render the stamp target before applying.
3. **Import/export** — .schem / .mcstructure converters on top of VoxelModel.
4. **True 1/16 bits** — custom blocks with sub-block geometry (behavior +
   resource pack generator); `kKeep` voxels are reserved for overlay layers.
5. **Per-dimension async stamping** — currently jobs resolve the player for
   their BlockSource; a dimension-id path removes that dependency.
