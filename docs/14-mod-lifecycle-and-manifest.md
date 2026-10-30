# 14 — Mod Lifecycle & Manifest

Source: https://docs.levimc.org/LeviLamina/api_reference/mod/ (canonical: `docs/main/contents/api_reference/mod.md`).

Headers: `ll/api/mod/Mod.h`, `Manifest.h`, `ModManager.h`, `ModManagerRegistry.h`, `NativeMod.h`.

## Mod class

```cpp
namespace ll::mod {
class Mod {
public:
    enum class State { Enabled, Disabled };

    State getState() const;
    Manifest const& getManifest() const;
    std::string const& getName() const;
    std::string const& getType() const;

    std::filesystem::path const& getModDir() const;
    std::filesystem::path const& getDataDir() const;
    std::filesystem::path const& getConfigDir() const;
    std::optional<std::filesystem::path> getWorldDataDir() const;
    std::optional<std::filesystem::path> getWorldConfigDir() const;
    std::filesystem::path const& getLangDir() const;
    std::filesystem::path const& getResourceDir() const;
    std::filesystem::path const& getBehaviorDir() const;

    io::Logger& getLogger() const;

    bool isEnabled() const;
    bool isDisabled() const;

    void onLoad(CallbackFn func) noexcept;    // CallbackFn: bool(Mod&)
    void onUnload(CallbackFn func) noexcept;
    void onEnable(CallbackFn func) noexcept;
    void onDisable(CallbackFn func) noexcept;
};

std::filesystem::path const& getModsRoot();
}
```

- Current instance: `ll::mod::NativeMod::current()` (this repo stores it as `mSelf` in the `LeviCreate` constructor).
- `getWorldConfigDir()` / `getWorldDataDir()` return `std::nullopt` while no world is loaded; on a client connected to a remote server they are `ll_config/<host>_<port>/<Mod>/` / `ll_data/<host>_<port>/<Mod>/` in the game directory.

## manifest.json fields

```cpp
struct Manifest {
    std::string entry, name, type;
    std::optional<std::string> platform;
    std::optional<bool> passive;
    std::optional<data::Version> version;
    std::optional<std::string> author, description;
    std::optional<SmallStringMap<std::string>> extraInfo;
    std::optional<SmallDenseSet<Dependency>> dependencies, optionalDependencies, conflicts, loadBefore;
};
```

| `type` | Loaded by | When |
|---|---|---|
| `native` | LeviLamina's native mod manager | After LeviLamina starts, with the other mods — use for ordinary C++ mods |
| `preload-native` | PreLoader | Before the game starts, before LeviLamina's own mods (LeviLamina itself uses this) |
| anything else | the mod manager registered for that type in `ModManagerRegistry` (e.g. a script engine); if none → "Unrecognized mod type" | – |

Dependency semantics:

| Field | Load ordering | Load failure | Disable/unload protection |
|---|---|---|---|
| `dependencies` | dependency first | blocks the dependent | yes |
| `optionalDependencies` | matching available dep first | does not block | yes, when bound at load time |
| `loadBefore` | declaring mod first | no dependency | no |

- Version ranges: omit `version` = any; use `^1.2.3`, `>=2.0.0 <3.0.0`, exact `=1.2.3` (bare `1.2.3` is legacy for `>=1.2.3 <2.0.0` and warns). Dependency JSON: `{"name": "RequiredMod", "version": "^1.2.3"}`.
- `passive: true` → loaded only when another mod depends on it.
- Dynamic loading never reorders already-loaded mods; "load before an already loaded target" logs a warning and continues.

## Runtime directory contract

```
plugins/                       # mods/ on the client
└── MyMod/
    ├── manifest.json          # mod metadata
    ├── MyMod.dll              # mod binary
    ├── config/                # getConfigDir()
    ├── data/                  # getDataDir()
    ├── lang/                  # getLangDir()
    ├── resource_packs/        # getResourceDir()
    └── behavior_packs/        # getBehaviorDir()

worlds/Bedrock level/
├── ll_config/MyMod/           # getWorldConfigDir()
└── ll_data/MyMod/             # getWorldDataDir()
```

`getModsRoot()` = `plugins/` on the server, `mods/` on the client (PreLoader uses `plugins/` when it exists, else `mods/`; `modsPath` in `PreLoaderConfig.json` overrides).

## Lifecycle order

`load` → `enable` → (runtime) → `disable` → `unload`. Each callback returns `bool` (`false` = failure). This repo implements the pattern with override-style methods + `LL_REGISTER_MOD(levi_create::LeviCreate, levi_create::LeviCreate::getInstance())` from `ll/api/mod/RegisterHelper.h`. Alternative registration style (from the docs): `mod.onLoad([](ll::mod::Mod& self) -> bool { ...; return true; });`.
