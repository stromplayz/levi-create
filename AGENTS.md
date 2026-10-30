# AGENTS.md — notes for AI coding agents working on LeviCreate

LeviCreate is a **LeviLauncher Android native mod** (`.levipack`, type
`preload-native`), built on the Preloader SDK (`LiteLDev/preloader-android`,
tag `0.2.2`). It is **not** a LeviLamina server plugin and does not use the
`ll::` server API, xmake, or `tooth.json`.

## Authoritative references

- Docs: <https://docs.levimc.org/LeviLamina/developer_guides/android/>
- SDK headers (ground truth): `include/pl/*.hpp` in the preloader-android repo
- Official template: <https://github.com/QYCottage/levilauncher-android-mod-template>
- Reference example: `examples/full-cpp-mod` in `LiteLDev/LeviLauncher-Android`
- Launcher import validation: `app/src/main/java/org/levimc/launcher/core/mods/FileHandler.java`

## Build

```bash
export ANDROID_NDK_HOME=/path/to/android-ndk-r28b      # 28.2.13676358
export PRELOADER_ANDROID_ROOT=/path/to/preloader-android   # tag 0.2.2
./scripts/build.sh arm64-v8a        # or: armeabi-v7a | all
# -> dist/levi-create-1.0.0-<abi>.levipack
```

Host prerequisite: CMake 3.22+, Ninja. The host branch compiles
`levi_config_generator` (typed config -> `config.json` + `config.schema.json`),
then the NDK branch builds and packages. CI replicates this in
`.github/workflows/build.yml`.

## Package contract (do not break)

- Zip contains ONE folder `levi-create/` (the runtime mod id — never rename it
  after release; it owns persisted paths and Mod Menu ownership).
- Inside: `manifest.json` (`type: preload-native`, `entry: liblevi_create.so`),
  `liblevi_create.so`, `config/config.json`, `config/config.schema.json`,
  optional `icon.png`.
- The launcher validates: exactly one `manifest.json` root and **exactly one
  `.so`** entry. Any extra `.so` in the pack breaks import.

## Hard constraints learned the hard way

1. **Draw-command cap**: the preloader caps overlay commands per module at
   4096 (`kMaxDrawCommandCount` in `ModMenuBridge.cpp`). The HUD renderer LODs
   down structures to stay under `config.maxDrawCommands` (default 3800 + 96
   reserve). Never submit unbounded command lists.
2. **DrawCommand colors are `0xAARRGGBB`** (see `ButtonInfo::styleColors`
   usage in the official example).
3. **Radio configs** take an index string plus a comma-separated options
   constant (see `kToolMenuOptions`).
4. **`PL_REGISTER_MOD`** requires a long-lived mod object; store
   `*ll::mod::NativeMod::current()` in the constructor (available during
   registration only).
5. **Unregister everything** in `disable()` (buttons, module, overlay via
   empty `submitDrawCommands`), and neutralize the process-wide touch callback
   with a no-op re-registration — there is no unregister API for input.
6. **clang/NDK brace-elision**: aggregates with 2-D array members must be
   initialized with explicit nesting — `{ { {a,b,c},... }, nx, ny, nz, s }`.
7. **VoxelStructure::resize** historically crashed when called on a freshly
   constructed object (empty buffer); the `hasData` guard is mandatory. Keep
   `tests/engine_selftest.cpp` green — it exercises exactly this class of bug:

   ```bash
   g++ -std=c++20 -O1 -fsanitize=address \
     -Isrc -Ibuild/host/_deps/nlohmann_json-src/include \
     tests/engine_selftest.cpp src/mod/voxel/*.cpp && ./a.out
   ```

## World application (game hooks)

Placing voxels into the live world needs per-version byte signatures for
`libminecraftpe.so`. `src/mod/game/GameBridge.cpp` resolves signatures from
`config/game_bridge.json` via `pl::memory::resolveSignatures` but is inert by
default — do NOT invent signatures or addresses. See README "World
application (advanced)".
