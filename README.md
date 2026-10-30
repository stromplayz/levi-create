# levi-create

**A voxel building toolkit for Minecraft Bedrock servers, powered by [LeviLamina](https://github.com/LiteLDev/LeviLamina).**
Chisel & Bits + Axiom style building — custom voxel models, any shape, any degree, any scale — 100% server-side, works with vanilla clients.

## ✨ Features

- 🧱 **Voxel models** — capture any region of the world as a reusable voxel model with a block palette (RLE-compressed JSON on disk).
- 📐 **Transformations** — move, copy, paste, cut, rotate, scale, flip/mirror.
- 🔄 **Any degree rotation** — exact voxel rotation at 90°/180°/270°, rasterized rotation at *any* other angle (try `/vc rotate 45 y` or `137.5`!).
- 🐜 **Tiny builds / miniatures** (the Chisel & Bits workflow) — shrink any build to 1/2 … 1/16 scale and place the miniature beside it, then `expand` it back to full size. True proportional replicas, not hand-scaled copies.
- 🖱️ **Interactive UI** — full form-based menus (`/vc menu`): selection, transforms, paste options with sliders, model library browser. Works on every vanilla client (phone, console, PC).
- 🪓 **Wand tool** — iron axe: sneak+right-click sets pos #1, right-click sets pos #2 (classic WorldEdit feel).
- 💾 **Model library** — save/load/delete named models in the mod data folder; share them between worlds by copying files.
- ↩️ **Undo** — every destructive operation (cut, move, rotate, scale, flip, clear, replace) is undoable, 8 levels deep.
- 🔍 **Fill / replace / count** — region editing and block statistics.
- 🛡️ **Safety rails** — 2,000,000 block volume cap and 512-per-axis side cap on all operations; unknown blocks are reported, never crash the server.

## 📦 What is the "levipack"?

The **levipack** is the ready-to-use mod package this repo ships on every release:

```
levi-create-server-windows-x64.zip
└── levi-create/
    ├── manifest.json        ← LeviLamina mod manifest (type: native, platform: server)
    ├── levi-create.dll      ← the compiled mod library (the ".so equivalent" for Windows)
    └── levi-create.pdb      ← debug symbols (optional, can be deleted)
```

### ❓ "Where is the .so?" (read this!)

Older LeviLamina-era builds were sometimes compiled for Linux (`.so`). **Current official LeviLamina releases (v26.x) run mods on Windows servers and clients**, and native mods are shipped as **`<mod>.dll`** — that *is* the native library file of this platform; there is no Linux loader published by LeviLamina right now, so a `.so` would not load anywhere.

- Running BDS on **Windows** → use the levipack as-is. ✅
- Running BDS on a **Linux host** → run the BDS + LeviLamina stack in a Windows VM, or ask your host for a Windows plan. If LeviLamina publishes a Linux loader again, adding a Linux CI target here is a one-line change (`.github/workflows/build.yml` already uses the platform-aware build matrix).

Earlier broken distributions of this mod missed files inside the pack — the release pipeline now zips the **entire packed output** (manifest + binary + symbols) exactly as LeviLamina's `modpacker` produces it, so nothing can be missing.

## 🚀 Installation

1. Install [LeviLamina](https://docs.levimc.org/) on your BDS server (Windows).
2. Download `levi-create-server-windows-x64.zip` from the [releases page](../../releases).
3. Unzip it into your server root, so you get:

   ```
   <server>/mods/levi-create/manifest.json
   <server>/mods/levi-create/levi-create.dll
   ```

   (If your `mods` folder is elsewhere, put the `levi-create` folder there.)
4. Restart the server. You should see `levi-create enabled. Use /vc help or /vc menu in game.`
5. In game (op level ≥ GameDirectors): `/vc menu` 🎉

Saved models land in `<server>/mods/levi-create/data/models/*.json`? No — LeviLamina stores mod data per data root; use `/vc list` in game to see what the server sees. Copy those JSON files between servers to share builds.

## 🎮 Usage

### Quick start (30 seconds)

```
1. Give yourself an iron axe:      /give @s iron_axe
2. Sneak + right-click one corner   → pos #1
3. Right-click the opposite corner  → pos #2
4. /vc copy                         → region is in your clipboard
5. Fly somewhere else and /vc paste → your build appears (centered on you)
6. /vc mini 4                       → a 1/4-scale miniature appears beside the original
```

### Commands

| Command | Description |
| --- | --- |
| `/vc help` | full reference |
| `/vc menu` · `/vc ui` | open the interactive UI |
| `/vc wand` | wand instructions |
| `/vc pos1` · `/vc pos2` | set selection corners at your feet |
| `/vc copy` · `/vc cut` | clipboard operations |
| `/vc paste [scale] [yaw] [pitch]` | paste around you, e.g. `/vc paste 0.5 90` |
| `/vc move <dx> <dy> <dz>` | move the selection |
| `/vc rotate <deg> [axis]` | **any degree** rotation (x/y/z) |
| `/vc scale <factor>` | scale in place (0.0625 … 16) |
| `/vc flip <x\|y\|z>` | mirror |
| `/vc mini [factor]` · `/vc expand [factor]` | tiny-build workflow (2…16) |
| `/vc fill <block>` · `/vc replace <from> <to>` · `/vc clear` | region editing |
| `/vc undo` | undo the last destructive op |
| `/vc info` · `/vc count` | selection statistics |
| `/vc save <name>` · `/vc load <name> [paste]` · `/vc delete <name>` · `/vc list` | model library |

All commands require **GameDirectors** permission (op). Command alias: `/levicreate`.

### The tiny-build workflow (Chisel & Bits style)

```
/vc mini 16   → 1/16-scale replica beside your build (a "bits" version)
/vc paste 0.125 y 45   → paste a model at 1/8 scale, rotated 45°
/vc expand 16 → take a tiny build and rebuild it x16 full-size
```

Internally every model is a 1/16-precision voxel grid, so miniatures keep true proportions. Block-level detail is downsampled with nearest-neighbor sampling (state-of-the-art for server-side vanilla clients — real per-voxel sub-block rendering would require a client-side addon).

### UI

`/vc menu` opens the form menu: **Selection**, **Transform** (dropdowns + sliders + any-angle input), **Clipboard & Paste** (scale/yaw/pitch sliders, "also paste air"), **Miniature/Bits** (one-tap ×2…×16), **Model Library** (browse, load, load+paste, delete), **Help**.

## 🏗️ Building from source

Requirements: [xmake](https://xmake.io/), Visual Studio 2022 LLVM/clang-cl toolset (LLVM 17+).

```bash
xmake repo -u
xmake f -a x64 -m release -p windows --target_type=server -y
xmake -y
```

The packed mod is generated to `bin/levi-create/` by LeviBuildScript's `modpacker` rule.

CI (`.github/workflows/build.yml`) builds the levipack on every push and attaches the zip to every `vX.Y.Z` tag release. The voxel core (`src/core/VoxelModel.cpp`) has zero engine dependencies and is covered by `scripts/test_core.cpp`.

## 📁 Project layout

```
manifest.json            LeviLamina mod manifest
xmake.lua                build definition
src/mod/                 mod lifecycle (load/enable/disable), wand event
src/core/VoxelModel.*    voxel data + transforms + JSON (pure C++, unit-tested)
src/core/PlayerState.*   per-player selection, clipboard, undo stack
src/core/WorldOps.*      world reads/writes (capture/place/fill/replace/count)
src/core/PlayerOps.*     high-level player operations shared by commands & UI
src/cmds/Commands.cpp    /vc command registration (20+ subcommands)
src/ui/Forms.cpp         interactive form UI
```

## ⚠️ Known limitations (v1)

- Block **states** (log axis, slab type, facing…) are not captured — blocks restore with their default state. Colors/variants that are separate block types (wool, concrete, wood types) are fully preserved.
- Arbitrary-angle rotation uses nearest-voxel sampling (lossy but deterministic).
- Very large pastes (hundreds of thousands of blocks) run synchronously — expect a short tick hiccup.
- Windows only (see the `.so` note above).

## 📄 License

[MIT](LICENSE)
