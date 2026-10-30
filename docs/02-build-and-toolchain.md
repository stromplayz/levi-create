# 02 — Build & Toolchain

Sources: https://docs.levimc.org/LeviLamina/developer_guides/how_to_guides/build_guide/ + https://docs.levimc.org/LeviLamina/developer_guides/how_to_guides/faq/ (troubleshooting portions live in `16-architecture-and-faq.md`). Canonical markdown: `docs/main/contents/developer_guides/how_to_guides/{build_guide,faq}.md`.

## Required tools

| Tool | Notes |
|---|---|
| **XMake 3.0.0+** | Build system. `Invoke-Expression (Invoke-Webrequest 'https://xmake.io/psget.text' -UseBasicParsing).Content` or `scoop install xmake` |
| **LLVM/Clang** | Windows builds use the `clang-cl` toolchain; `clang-cl` must be on `PATH` (`scoop install llvm` or releases.llvm.org) |
| **MSVC 2022+** | Provides the Windows SDK + STL that clang-cl builds against (VS 2022 "Desktop development with C++" or Build Tools) |
| **Git** | Version control / version generation |
| **Windows 10/11 x64** | Main platform. Server builds can also target Linux x86_64 (`-p linux`, `clang` toolchain); **client builds are Windows-only** |

## Build commands

```bash
xmake                                   # build for server (default)
xmake f --target_type=server            # or explicitly
xmake f --target_type=client            # client build
xmake f --tests=y                       # GTest-based tests
xmake f --publish=y                     # mark as published release (affects version string)
xmake c                                 # clean build
xmake c -a                              # clean ALL
xmake f -c                              # reconfigure
xmake require --list                    # show resolved dependency versions
xmake g --proxy_pac=github_mirror.lua   # mirror on download failures
```

## Build options

| Option | Values | Default | Description |
|---|---|---|---|
| `--target_type` | `server`, `client` | `server` | Target type (picks `bedrockdata:-server/-client`) |
| `--tests` | `y`, `n` | `n` | Enable GTest testing |
| `--publish` | `y`, `n` | `n` | Published-release version string |
| `--levimc_repo` | URL | – | Custom xmake repo URL |

## Dependencies (25+, all via `add_requires` — `xmake.lua` is the single source of truth)

- **Core**: entt (ECS), fmt, nlohmann_json, rapidjson, leveldb (the KVDB backend), gsl
- **Performance**: mimalloc, parallel-hashmap, concurrentqueue
- **Utilities**: ctre, magic_enum, type_safe, expected-lite, glm, cpr, stb
- **LeviLamina-specific** (from `levimc-repo` = `LiteLDev/xmake-repo`): pcg_cpp, pfr (reflection), demangler, levibuildscript, preloader, symbolprovider, trampoline
- **Platform**: libhat (Windows), bedrockdata (version-locked to the MC version; `-server`/`-client` per `--target_type`), gtest (only with `--tests=y`)

Mod-relevant subset: `levilamina` + `levibuildscript` packages and the `levimc-repo` repository (see `xmake.lua`).

## Compiler settings (Windows)

- C++20 required; C++23 features via `_HAS_CXX23=1`.
- Runtime `/MD`; exceptions `/EHa` + `/EHs` (xmake-level `set_exceptions("none")` to avoid conflict); warnings `/W4` plus `/w44265 /w44289 /w44296 /w45263 /w44738 /w45204`; clang-cl-specific `-Wno-*` suppressions (all visible in this repo's `xmake.lua`).

## Version number generation (LeviLamina itself — same pattern is good practice for mods)

- Format: `v{major}.{minor}.{patch}{-prerelease}+{commit_hash}`; source priority: git tag → `tooth.json` `version`.
- Injection target: `src/ll/core/Version.h.in` (`LL_VERSION_MAJOR/MINOR/PATCH/PRERELEASE/BUILD`).
- Major/minor track the targeted Minecraft version.

## Build output

```
LeviLamina/
├── build/windows/x64/release/LeviLamina.dll
└── bin/LeviLamina/
    ├── LeviLamina.dll
    └── manifest.json
```

For mods: `bin/<mod-name>/` — copy the whole directory into the server's `mods/`.

## CI/CD

- `build.yml`: on every push/PR/manual; `windows-latest`; matrix = server + client; xmake cache; LLVM 22 via `ZhongRuoyu/setup-llvm@v0`; uploads `bin/` artifact.
- `release.yml`: on release `published`; builds both variants, injects `CHANGELOG.md` section into the release body (`ffurrer2/extract-release-notes`), zips `bin/ + CHANGELOG.md + LICENSE + README.md` as `<repo>-{server,client}-windows-x64.zip` and attaches to the release.
- Release flow: `git tag v26.51.8` → push tag → CI builds with `--publish=y` → artifacts to GitHub Releases.

## Troubleshooting

- XMake cache weirdness: `xmake c -a` then `xmake f -c`.
- Dependency download failures: GitHub mirror pac (`xmake g --proxy_pac=github_mirror.lua`) or HTTP proxy.
- Toolchain not found: install VS 2022 / Build Tools (Windows SDK), LLVM (clang-cl on PATH), run from "Developer Command Prompt for VS 2022".
