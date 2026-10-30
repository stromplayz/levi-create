# Sources — every mined page and the URL corrections

All content in `docs/` was extracted in **October 2026** from the official LeviLamina documentation and cross-verified against the canonical markdown in the `LiteLDev/LeviLamina` repository (`docs/main/contents/…`). This file records every source URL and the URL corrections discovered by the 20-agent mining fleet, so future agents never chase dead links.

## URL structure (important)

| Content | Live URL pattern | Canonical markdown (raw) |
|---|---|---|
| Developer guides | `https://docs.levimc.org/LeviLamina/developer_guides/...` | `https://raw.githubusercontent.com/LiteLDev/LeviLamina/main/docs/main/contents/developer_guides/...` |
| API Reference | `https://docs.levimc.org/LeviLamina/api_reference/<name>/` | `https://raw.githubusercontent.com/LiteLDev/LeviLamina/main/docs/main/contents/api_reference/<name>.md` |
| Android guides | `.../developer_guides/android/` (+ `android/api/<name>/`) | `.../developer_guides/android/<name>.md`, `.../android/api/<name>.md` |

**Corrections found while mining** (these were verified 404 → corrected):

- How-to guides use `how_to_guides` (underscores), **not** `how-to_guides`.
- The API Reference section is **top-level** `/LeviLamina/api_reference/<name>/` — there is no `developer_guides/api_guides/` folder; the "Overview" page is `api_reference/index.md`.
- The Android section has been restructured: old `android_native_mods/*` URLs are dead; the real paths are `developer_guides/android/` (quick start = `android/index.md`) and `developer_guides/android/api/{mod,mod_menu,config,input,hook,signature,patch,memory_hook_macros,types_and_macros}.md`.

## Pages mined (41 total)

### Tutorials (1)

- Create Your First Mod — `developer_guides/tutorials/create_your_first_mod/` → `docs/01`

### How-to Guides (11) — `developer_guides/how_to_guides/<name>/`

| Page | Doc file |
|---|---|
| build_guide | docs/02 |
| command_guide | docs/03 |
| event_guide | docs/04 |
| form_guide | docs/05 |
| data_driven_ui_guide | docs/06 |
| coro_guide | docs/10 |
| hook_guide | docs/09 |
| find_function_guide | docs/09 |
| interface_export_guide | docs/09 |
| item_guide | docs/01 (+ notes below) |
| i18n_guide | docs/11 |

Item guide key facts (folded into docs/01): `ItemStack : ItemStackBase` (per-slot data: count, enchants, durability) vs `Item` (type info); creation via `ItemStack::reinit(name, count, aux)` or `ItemStack::fromTag(tag)`; **LeviLamina currently has no API for registering custom items**; after modifying a server-side ItemStack, sync to the client (`Mob::refreshInventory()`); header reference `src/mc/world/item/ItemStack.h`.

### Developer Guides root (2)

- FAQ — `developer_guides/faq/` → docs/16
- Architecture — `developer_guides/architecture/` → docs/16

### API Reference (21) — `api_reference/<name>/`

| Page | Doc file |
|---|---|
| index (Overview) | docs/README.md scope table |
| base | docs/17 |
| chrono | docs/10 |
| command | docs/03 |
| config | docs/07 |
| coro | docs/10 (guide covers it) |
| data | docs/08 |
| data_driven_ui | docs/06 |
| event | docs/04 |
| expected | docs/17 |
| form | docs/05 |
| i18n | docs/11 |
| input (Client) | docs/15 |
| io (IO & Logger) | docs/11 |
| memory (Memory & Hook) | docs/09 |
| mod | docs/14 |
| network | docs/12 |
| reflection | docs/07 |
| reflection_advanced | docs/07 |
| service | docs/13 |
| thread | docs/10 |
| utils | docs/17 |

### Android Native Mods (10) — `developer_guides/android/**`

| Page | Doc file |
|---|---|
| index (Native Mod Quick Start) | docs/15 |
| api/mod.md | docs/15 |
| api/mod_menu.md | docs/15 |
| api/config.md | docs/15 |
| api/input.md | docs/15 |
| api/hook.md | docs/15 |
| api/signature.md | docs/15 |
| api/patch.md | docs/15 |
| api/memory_hook_macros.md | docs/15 |
| api/types_and_macros.md | docs/15 |

## External references referenced by the docs

- Mod template: https://github.com/LiteLDev/levilamina-mod-template
- Tutorial reference implementation: https://github.com/ShrBox/ExampleMod
- xmake repo (packages): https://github.com/LiteLDev/xmake-repo
- Android Preloader SDK: https://github.com/LiteLDev/preloader-android — Android mod template: https://github.com/QYCottage/levilauncher-android-mod-template
- Keep a Changelog: https://keepachangelog.com/en/1.1.0/ — SemVer: https://semver.org/
- lip package docs: https://docs.levimc.org/lip/guides/creating_packages.html, .../migrating_packages.html
- Microsoft DDUI docs: https://learn.microsoft.com/en-us/minecraft/creator/documents/scripting/intro-to-ddui?view=minecraft-bedrock-stable

## Verification method

Each page was fetched twice: once from the rendered docs site, once from the raw markdown on `LiteLDev/LeviLamina@main`, with headings and code-block counts compared. Where the site and the repository disagreed (e.g. restructured Android section), the repository markdown was treated as canonical. API signatures were additionally spot-checked against LeviLamina headers (`src/ll/api/...`).
