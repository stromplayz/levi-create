# AGENTS.md — Operating Instructions for AI Coding Agents

Read this file completely before touching any code. It is written in first person: when you (the agent) work in this repository, these sentences are yours.

---

## 1. Who I am and what I am building

I am an autonomous coding agent working inside **levi-create**, a native C++ mod for **[LeviLamina](https://github.com/LiteLDev/LeviLamina)** — the mod loader for Minecraft Bedrock Dedicated Server (BDS). My product is a Windows x64 shared library packed by xmake (`@levibuildscript/modpacker`), loaded by LeviLamina at runtime, plus the knowledge base that lets me (or a future agent) keep developing it correctly.

levi-create is a **voxel building toolkit** (Chisel & Bits + Axiom style, server-side): custom voxel models with block palettes, any-degree rotation, scaling down to 1/16 (tiny builds / miniatures), move/copy/paste, region fill/replace, undo, an interactive form UI and a file-backed model library.

- Mod name / target name: `levi-create` (manifest name `levi-create`, command `/vc`, alias `/levicreate`)
- Namespace: `lc`, mod class: `LeviCreate` (singleton via `LeviCreate::getInstance()`, registered with `LL_REGISTER_MOD`)
- Language standard: **C++20** (C++23 features enabled via `_HAS_CXX23=1`), compiled with **clang-cl** against the MSVC STL
- Package ecosystems: `xmake` (build) + `lip` (distribution via `tooth.json`)

## 2. Where everything lives

| Location | What it is | When I read it |
|---|---|---|
| `src/core/VoxelModel.*` | **Voxel engine** — pure C++20, no Minecraft deps: `VoxelModel` (palette grid), scale/rotate/flip transforms, JSON (de)serialization | Before touching any engine math |
| `src/core/PlayerState.*` | Per-player selection, clipboard, undo stack | When touching player session state |
| `src/core/WorldOps.*` | The ONLY code calling Bedrock world internals (capture/place/fill/replace/count) | When a LeviLamina update breaks a build |
| `src/core/PlayerOps.*` | High-level operations shared by commands and UI (requireSelection, pushUndo, clear, paste, mini/expand) | When adding a user-facing feature |
| `src/cmds/Commands.cpp` | `/vc` command tree (reflection-based overloads) | When adding commands |
| `src/ui/Forms.cpp` | Form studio panels (SimpleForm/CustomForm/ModalForm) | When touching player-facing UI |
| `src/mod/LeviCreate.*` | Mod lifecycle, wand event, command registration wiring | When touching lifecycle |
| `docs/01…17-*.md` | Full digest of every official developer-guide page (tutorial, build, commands, events, forms, DDUI, config/reflection, storage, hooks/memory, coroutines/threads, i18n/logging, network, services, lifecycle, input/Android, architecture/FAQ, core utilities) | Before using ANY API I have not used yet in this repo |
| `docs/feature-voxel-engine.md` | Architecture, data flow, invariants, roadmap of the voxel engine | Before ANY feature work |
| `docs/model-format.md` | Model format specification | When touching serialization |
| `docs/sources.md` | Canonical source URLs + URL corrections | When I need the upstream page |
| `xmake.lua` | Build config (target name, `target_type`) | When changing build or mod name |
| `tooth.json` | `lip` package manifest (server variant, release zip) | When releasing |
| `manifest.json` | Runtime mod manifest — `${placeholders}` are filled by the `@levibuildscript/modpacker` xmake rule; I must **not** hand-edit the placeholders | Rarely |
| `.github/workflows/build.yml` | CI: builds the levipack on every push, attaches the zip to every `vX.Y.Z` tag release | When touching CI |
| `scripts/test_core.cpp` (in the agent workspace) | Linux unit tests for the pure engine (`VoxelModel.cpp` compiles standalone with g++) | After ANY engine math change |

## 3. Build and verify (I always do this after code changes)

This repo builds on **Windows** (clang-cl + MSVC SDK). On a Linux agent box I cannot fully compile; in that case I still run every static check I can and let CI (`.github/workflows/build.yml`) do the Windows build — then I read the run logs via the GitHub API and fix the exact lines before pushing again.

```powershell
# configure (server mod — the default)
xmake f -y -p windows -a x64 -m release --target_type=server

# build (packed mod lands in bin/levi-create/)
xmake
```

- Build output: `bin/levi-create/` (manifest.json + levi-create.dll + pdb) — that directory **is** the levipack; CI zips it for releases.
- If dependency downloads fail, configure a GitHub mirror proxy (`xmake g --proxy_pac=github_mirror.lua`) or an HTTP proxy for xmake.

### 3b. Verifying the engine on Linux (no Windows needed)

`src/core/VoxelModel.cpp` has zero Minecraft dependencies:

```bash
g++ -std=c++20 -Wall -Wextra -Isrc src/core/VoxelModel.cpp test_core.cpp -o test && ./test
```

The suite covers JSON round-trips and corruption rejection, exact 90/180/270/360 rotations on all three axes, 45°/137.5° rasterized rotation, scale ×0.5/×2/×16 and 1/16 minis, mirror remap and transform composition. If I change engine math, I extend this suite in the same commit. Everything else is syntax-checked by Windows CI.

## 4. Hard runtime rules (breaking any of these = crash or broken mod)

1. **Lifecycle discipline.** `load()` → directories/config only. `enable()` → subscribe events, register commands. `disable()` → undo both. **Never** touch game objects or register anything in `load()` or the constructor — that crashes the server.
2. **Commands are registered (a) inside a `ll::event::ServerCommandRegisterEvent` listener** — fired whenever the game builds its command registry — and (b) once directly in `enable()` so hot-enabling works. `getOrCreateCommand` is idempotent, so both are safe. Registration *only* in `enable()` breaks on the client and after registry rebuilds.
3. **Server thread only.** Most MCAPI calls (anything touching `Player`, `Level`, `BlockSource`, forms) must run on the server thread, otherwise: ACCESS_VIOLATION. Commands, form callbacks and events already arrive on it; do not hop threads with world state.
4. **No `dynamic_cast`** — MCBE has no RTTI. After `entity->isPlayer()`, `static_cast<Player*>(entity)` is the accepted pattern.
5. **Identify players by name via `player.getRealName()`** (display names are mutable, `getName()` is worse; this mod's session map keys on `getRealName()`). For tick-time player lookup prefer `Level::getPlayer(ActorUniqueID)`.
6. **Items are `ItemStack`**, not `Item`. Check item identity with `itemStack.getTypeName()` (e.g. `"minecraft:iron_axe"`), never with display name.
7. **Event listeners are auto-removed** when the mod is disabled — I keep `shared_ptr` references only when I want explicit `bus.removeListener(...)`.
8. **Errors flow through `ll::Expected<T>`** in LL APIs; this mod keeps it simple with `std::string& err` out-params in `WorldOps` so both commands and forms can surface messages.
9. **Command param structs must have external linkage** (named namespace — `boost::pfr` reflection rejects anonymous-namespace types) and **no `std::optional<T>` members** — optional overload params keep their default member values instead. They live directly in `namespace lc::cmd`.
10. **The disconnect event is `PlayerDisconnectEvent`** — there is no `PlayerLeftEvent` in LeviLamina. `PlayerInteractBlockEvent` is cancellable (`event.cancel()`) and exposes **`blockPos()`** (not `pos()`), plus `item()`, `face()`, `clickPos()`. The wand cancels it so the axe never uses the block.
11. **Namespaced `class Player&` bug**: a `class X&` parameter written INSIDE `namespace lc` declares `lc::X`, not global `::X`. Forward-declare Minecraft types at global scope (`class Player;`) and use bare `Player&` everywhere.
12. **World writes** go only through `WorldOps::setBlockAt` (`BlockSource::setBlock(pos, block, flags=3, nullptr, BlockChangeContext{})`, 5-arg virtual overload). Volume caps: 2,000,000 blocks / 512 per axis — never lift them without also changing the undo snapshot budget.

## 5. API facts verified against the v26.51.9 headers (re-check on version bumps)

- `Block::tryGetFromRegistry(HashedString const&)` → `optional_ref<Block const>`; the registry is global, no `Level&` argument. `HashedString` is at `mc/deps/core/string/HashedString.h`.
- `BlockChangeContext{}` is an aggregate — include `mc/world/level/block/BlockChangeContext.h`.
- `Vec3` is at `mc/deps/core/math/Vec3.h`; `BlockPos` at `mc/world/level/BlockPos.h` (intN3 ctors: `BlockPos{x,y,z}`).
- `Player::getRealName()`, `Actor::isPlayer()`, `Actor::isSneaking()`, `Actor::getPosition()`, `Actor::getDimensionBlockSource()`, `Actor::getDimensionId()` (returns `DimensionType`, has `operator int`), `Player::sendMessage(std::string_view)`.
- `EventBus::emplaceListener<T>(lambda)` returns `shared_ptr<Listener<T>>`; T must be `final` (`PlayerInteractBlockEvent`, `ServerCommandRegisterEvent` are).
- Command executor lambda signatures, in order of preference: `(CommandOrigin const&, CommandOutput&, Params const&)`. `CommandHandle::alias(std::string_view)` adds aliases. `CommandPermissionLevel::GameDirectors` = op.
- Form element methods: `appendInput(name, text, placeholder, default)`, `appendToggle(name, text, default)`, `appendSlider(name, text, min, max, step, default)`, `appendDropdown(name, text, options, defaultIndex)`; results: dropdowns/toggles `uint64`, sliders `double`, inputs `std::string`. `SimpleForm::appendButton(text, imageData, imageType, callback)` — if `sendTo` gets a callback, per-button callbacks are ignored.
- Form button callbacks must capture by **value** anything from the enclosing scope (they run long after the opener returns).
- The server tick event is **`ll::event::world::ServerLevelTickEvent`** — there is no `LevelTickEvent` on the server.
- `Player::add(ItemStack&)` (virtual `bool`) and `Mob::refreshInventory()` (LLAPI) give items.

## 6. Coding conventions

- `clang-format`: LLVM-based, 120 columns, 4-space indent, pointer left (`Type* name`). Run `clang-format -i src/**` before committing.
- Namespace style: `namespace lc { ... } // namespace lc`. Anonymous namespaces for TU-local helpers — but **never** for command param structs (see §4.9).
- The mod logger: `auto& logger = getSelf().getLogger();` — fmt-style `{}` formatting everywhere.
- Engine math (rotation/scale/flip/serialization) lives ONLY in `src/core/VoxelModel.cpp`; world writes ONLY in `src/core/WorldOps.cpp`.

## 7. Recipes (proven patterns)

- **Add a command**: params struct in `namespace lc::cmd` + `cmd.overload<Params>().text("sub").required("member").optional("opt").execute(...)` inside `registerCommands()`; resolve the player with `playerOf(origin, output)`.
- **Show UI**: static `ll::form::ModalForm/SimpleForm/CustomForm`; forms are show-once and server-thread-bound. Parse results through the `getUint/getDouble/getString/getBool/getNumber` helpers in `ui/Forms.cpp`.
- **React to gameplay**: `bus.emplaceListener<ll::event::PlayerInteractBlockEvent>(...)`; cancellable events get `event.cancel()`.
- **World edits**: snapshot with `ops::pushUndoSnapshot` before destroying; then `WorldOps::fillRegion`/`placeModel`.
- **Persist data**: model JSON files under `LeviCreate::modelsDir()` (`getSelf().getDataDir() / "models"`).
- **Ship a release**: add a `CHANGELOG.md` entry → tag `vX.Y.Z` and push the tag → CI builds the levipack and attaches `levi-create-server-windows-x64.zip` to the release.

## 8. Pre-commit checklist

- [ ] No game operations outside `enable()`/event callbacks.
- [ ] Engine math changes come with an extended Linux test-suite run (§3b).
- [ ] Every new Bedrock call sits in `src/core/WorldOps.cpp`.
- [ ] Every new API I used is documented in `docs/` (or I added its digest) and §5 if verified.
- [ ] Command param structs still have external linkage; no `std::optional` members.
- [ ] `CHANGELOG.md` updated for user-visible changes.
- [ ] Commit message in imperative mood.

## 9. Source of truth

The mined knowledge in `docs/` was extracted (October 2026) from docs.levimc.org and cross-verified against the LeviLamina repository sources (v26.51.9). When docs and reality conflict at build time, the compiler wins — then I fix the digest and note it in `docs/sources.md`.
