# LeviCreate — Voxel Studio for LeviLauncher (Android)

**Chisel & Bits × Axiom style sub-block voxel modeling as a LeviLauncher Android native mod.**

LeviCreate ships as a `.levipack` — the official package format for
[LeviLauncher Android](https://docs.levimc.org/LeviLauncher/) native mods
(`preload-native`), built on the [Preloader SDK](https://github.com/LiteLDev/preloader-android)
(tag `0.2.2`). The package contains the manifest, the native `liblevi_create.so`
and the default config.

```
levi-create/                  <- runtime mod id (do not rename)
├── manifest.json             <- type: preload-native, entry: liblevi_create.so
├── liblevi_create.so         <- arm64-v8a native mod
├── icon.png
└── config/
    ├── config.json           <- defaults, editable
    └── config.schema.json
```

## Install

1. Install **LeviLauncher** on your Android device and add your Minecraft
   Bedrock version.
2. Open the `.levipack` file (from this folder) with LeviLauncher, or copy the
   unpacked `levi-create/` directory into the launcher's native mod directory.
3. Start the game. Open the LeviLauncher **Mod Menu** and you will find
   **"LeviCreate - Voxel Studio"**.

Choose the pack that matches your device:

| File | Device |
| --- | --- |
| `levi-create-1.0.0-arm64-v8a.levipack` | All modern 64-bit phones/tablets (recommended) |
| `levi-create-1.0.0-armeabi-v7a.levipack` | Older 32-bit devices |

## What you get

A floating **voxel studio canvas** (bottom-left) plus a **toolbar** of
on-screen buttons. Everything edits an N×N×N voxel grid ("bits") — the same
sub-block idea as Chisel & Bits:

- **1/16 sub-block precision** by default (grid size 16 = 16 bits per block
  edge; slider covers 4–48).
- **Tools** (cycled with the `Tool` button or the Mod Menu radio):
  `Build` (tap to place, drag to paint lines), `Erase`, `Paint` (recolor),
  `Box` (tap two corners → filled box), `Sphere` (tap center, then edge →
  filled ball), `Move` (drag to move the whole structure).
- **Any-degree rotation** — the `Rot` button applies the angle set in the Mod
  Menu slider (5°–180°), resampled onto the bit grid. `Tilt` rotates 90°
  around the horizontal axis, `Mir` mirrors.
- **Size control** — `S+` / `S-` scale the structure (nearest-neighbor,
  0.8×–1.25× per tap), grid resize from the Mod Menu.
- **Copy / Paste** — whole-structure clipboard; it survives restarts
  (`data/clipboard.json`).
- **Save / Load slots 1–9** — structures are JSON (RLE-compressed voxels)
  under `data/structures/slot_N.json`.
- **Undo** — 20 levels.
- **18-material palette** (stone, planks, concrete colors, gold, diamond…),
  cycled with `Mat` or the Mod Menu slider.
- **Interactive 3D preview** — isometric shaded render with depth sorting,
  face culling and automatic LOD so the overlay never exceeds the preloader's
  4096 draw-command cap. Drag inside the canvas to orbit the camera; the tap
  target follows the exact same math (DDA voxel raycast).

### Controls

| Gesture | Action |
| --- | --- |
| Tap in canvas | Apply current tool (place / erase / recolor / box & sphere anchors) |
| Drag in canvas | Orbit the preview (or move the structure with the Move tool) |
| `Tool` button | Cycle Build → Erase → Paint → Box → Sphere → Move |
| `Mat` button | Cycle material palette |
| `Rot` button | Rotate by the configured angle around the vertical axis |
| `Tilt` / `Mir` | 90° tilt / mirror |
| `S+` / `S-` | Scale up / down |
| `Copy` / `Paste` | Clipboard |
| `Save` / `Load` | Slot save / load (slot from Mod Menu) |
| `Clear` | Empty the grid |

Every floating button can be moved or hidden with the LeviLauncher HUD editor;
the whole panel can be toggled from the Mod Menu (`Show Studio Panel`).

## Configuration (Mod Menu)

`Panel Scale`, `Preview Zoom`, `Grid Size`, `Tool`, `Material`, `Rotate
Angle`, `Save Slot`, `Max Draw Commands`, `Accent Color`, plus touch-capture
and status-line toggles. Values persist to `config/config.json`; the packaged
defaults are generated from the typed schema at build time.

If the canvas sits wrong for your screen, adjust `Panel Scale`; screen bounds
are also auto-detected from the largest touches seen (`autoDetectScreen`).

## World application (advanced)

Placing voxels into the live Minecraft world requires per-version byte
signatures for `libminecraftpe.so`. LeviCreate ships an honest, safe extension
point: `config/game_bridge.json` (created on first run). Set `"enabled": true`
and add signatures; the bridge resolves them via `pl::memory::resolveSignatures`
at startup and reports results in logcat. The actual game-function calls live
in `src/mod/game/GameBridge.cpp` and are intentionally inert until signatures
for your game version are supplied — no invented addresses, no crashes.
Structures are portable JSON, so they can also be converted into server-side
imports (e.g. a LeviLamina `llplugin` that replays them into worlds).

## Build from source

Requirements: CMake 3.22+, Ninja, Android NDK r28b (28.2.13676358), a checkout
of `LiteLDev/preloader-android` at tag `0.2.2`.

```bash
export ANDROID_NDK_HOME=/path/to/android-ndk-r28b
export PRELOADER_ANDROID_ROOT=/path/to/preloader-android
./scripts/build.sh arm64-v8a        # or: all
# -> dist/levi-create-1.0.0-arm64-v8a.levipack
```

The build mirrors the official template: host build generates
`config.json` + `config.schema.json` from the typed config, NDK build compiles
`liblevi_create.so` (stripped, 16 KB page-size aligned) and the `levi_package`
target zips the `.levipack`. A GitHub Actions workflow (`.github/workflows/`)
reproduces this on push.

## Project layout

```
src/
├── main.cpp                     # PL_REGISTER_MOD entry
├── config_generator.cpp         # host config/schema generator
└── mod/
    ├── Config.h/.cpp            # typed config + Mod Menu schema
    ├── LeviCreateMod.h/.cpp     # lifecycle, module + config wiring
    ├── VoxelStudio.h/.cpp       # shared state: tools, undo, clipboard, picking
    ├── voxel/                   # VoxelStructure (RLE JSON), transforms, slots
    ├── ui/                      # projection math, HUD renderer, touch, toolbar
    └── game/GameBridge.cpp      # signature-driven world-application hook point
```

## License

MIT
