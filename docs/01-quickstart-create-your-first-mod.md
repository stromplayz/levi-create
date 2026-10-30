# 01 — Quickstart: Create Your First Mod

Source: https://docs.levimc.org/LeviLamina/developer_guides/tutorials/create_your_first_mod/ (canonical markdown: `LiteLDev/LeviLamina@main → docs/main/contents/developer_guides/tutorials/create_your_first_mod.md`). Reference implementation: https://github.com/ShrBox/ExampleMod

This is the end-to-end official tutorial; the mod in `src/levi_create/` IS this tutorial, renamed. Below is the complete digest.

## Prerequisites

- Basic C++ knowledge.
- Toolchain: [xmake](https://xmake.io), [VS Code](https://code.visualstudio.com) (+ extensions: `ms-vscode.cpptools` C/C++, `tboox.xmake-vscode` XMake, `llvm-vs-code-extensions.vscode-clangd` clangd), Git, **Visual Studio 2022 with "Desktop development with C++"**, and LLVM/clang-cl (via the VS installer's "C++ Clang tools for Windows" optional component, or from llvm-project releases).

## Creating the mod repository

Start from the official template: https://github.com/LiteLDev/levilamina-mod-template ("Use this template"). Then personalize:

1. **Mod name** in `xmake.lua`: `target("LeviCreate")` — letters/numbers/hyphens only (`example-mod` or `ExampleMod` style).
2. **Pin the LeviLamina version**: `add_requires("levilamina 26.51.*", {configs = {target_type = get_config("target_type")}})` (a commit hash also works for pre-release adaptation).
3. **`tooth.json`** — lip package info: set `tooth` to the repo path (`github.com/OWNER/REPO`), fill `info`, set the LeviLamina version in each variant's `dependencies`, and match `assets[].urls` + `placements` (`src`/`dest`) to the mod name. Empty-label variant = **server** (installs to `plugins/<mod>/`); `label: "client"` variant = **client** (installs to `mods/<mod>/` via LeviLauncher). Server install: `lip install github.com/OWNER/REPO`.
   - Note: `tooth.json` is the legacy format; lip's current format is `manifest.json` (format 4) — `lip migrate` converts.
4. **`LICENSE`** — template is CC0; pick anything (this repo uses MIT).
5. **`README.md`** — features, usage, config, commands.
6. **Namespace/class** — rename `my_mod`/`MyMod` in `MyMod.cpp/.h` (lowercase_underscore namespace, consistent everywhere; update `#include`s).

## Building

- VS Code: XMake sidebar → build mode `Debug` → set **XMake: Additional Config Arguments** to `--target_type=server` or `--target_type=client` → "Build the given target".
- CLI: `xmake f -y -p windows -a x64 -m release --target_type=server` then `xmake`.
- Download failures → configure a GitHub mirror proxy or HTTP proxy for xmake.

## Registering the /suicide command

Commands **cannot** be registered while the mod loads — only after the game has started. On the server, `enable()` is late enough; on the client (since 26.10, mods enable when the game startup completes, not when a world loads) it is not. So register via **`ServerCommandRegisterEvent`**:

```cpp
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/command/ServerCommandRegisterEvent.h"

bool ExampleMod::enable() {
    auto& logger = getSelf().getLogger();
    using namespace ll::event;
    auto& bus = EventBus::getInstance();

    bus.emplaceListener<command::ServerCommandRegisterEvent>([&logger](command::ServerCommandRegisterEvent&) {
        auto& command = ll::command::CommandRegistrar::getInstance(false)
                            .getOrCreateCommand("suicide", "Commits suicide.", CommandPermissionLevel::Any);
        command.overload().execute([&logger](CommandOrigin const& origin, CommandOutput& output) {
            auto* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer()) {
                output.error("Only players can commit suicide");
                return;
            }
            auto* player = static_cast<Player*>(entity);
            player->kill();
            logger.info("{} killed themselves", player->getRealName());
        });
    });
    return true;
}
```

Key facts:
- `CommandRegistrar::getInstance(false)` = server side, `true` = client side.
- Permission levels: `Any` (everyone), `GameDirectors` (OP), `Host` (console).
- A **command overload** = one argument pattern (`ll <unload|reload|reactivate> <mod>` vs `ll list`). Overloads are typed by a reflection struct; `.text("sub")` adds literal subcommands (see `docs/03-commands.md`).
- **Warning**: never do game operations (event subscription, command registration) in the loading method — it will very likely crash.
- **Tip**: the mod constructor should only do non-game initialization (logging, config, database).
- **Warning**: MCBE lacks RTTI → no `dynamic_cast`. **Tip**: prefer `getRealName()` over `getName()` (display name is mutable).

## Reading configuration files

LeviLamina implements reflection for configs: define a struct, LeviLamina reads/writes the JSON automatically.

```cpp
// Config.h
namespace example_mod {
struct Config {
    int  version                = 1;
    bool doGiveClockOnFirstJoin = true;
    bool enableClockMenu        = true;
};
} // namespace example_mod
```

```cpp
bool ExampleMod::load() {
    auto& logger = getSelf().getLogger();
    const auto& configFilePath = getSelf().getConfigDir() / "config.json";
    if (!ll::config::loadConfig(config, configFilePath)) {
        logger.warn("Cannot load configurations from {}", configFilePath);
        logger.info("Saving default configurations");
        if (!ll::config::saveConfig(config, configFilePath)) {
            logger.error("Cannot save default configurations to {}", configFilePath);
        }
    }
    return true;
}
```

Config is loaded in `load()`, so later phases can rely on it. See `docs/07-config-and-reflection.md`.

## Persisting player join information in a database

`ll::data::KeyValueDB` is a LevelDB-backed KV store. It is move-only → store in `std::unique_ptr`, **never** a raw pointer ("you are writing C++, not C").

```cpp
namespace {
Config                                config;
std::unique_ptr<ll::data::KeyValueDB> playerDb;
} // namespace

// in load():
const auto& playerDbPath = getSelf().getDataDir() / "players";
playerDb                 = std::make_unique<ll::data::KeyValueDB>(playerDbPath); // auto-creates the dir
```

## Giving players a clock on first join

```cpp
bus.emplaceListener<ll::event::player::PlayerJoinEvent>(
    [&doGiveClockOnFirstJoin = config.doGiveClockOnFirstJoin, &logger, &playerDb = playerDb](
        ll::event::player::PlayerJoinEvent& event) {
        if (doGiveClockOnFirstJoin) {
            auto& player = event.self();
            const auto& uuid = player.getUuid();
            if (!playerDb->get(uuid.asString())) {
                ItemStack itemStack;
                itemStack.reinit("minecraft:clock", 1, 0);
                player.add(itemStack);
                player.refreshInventory();
                if (!playerDb->set(uuid.asString(), "true")) {
                    logger.error("Cannot mark {} as joined in database", player.getRealName());
                }
                logger.info("First join of {}! Giving them a clock", player.getRealName());
            }
        }
    });
```

- `event.self()` = the player; `player.getUuid()` returns `mce::UUID` (efficient; convert with `asString()` only when needed).
- **Danger**: do NOT use XUID as a player identifier (it is an Xbox Live id; unpredictable in offline mode / with fake players). Use UUID.
- Use `ItemStack` (count/enchantments/durability wrapper), not `Item` (type only). Refresh the inventory after edits.

## Suicide confirmation prompt when using the clock

```cpp
bus.emplaceListener<ll::event::PlayerUseItemEvent>(
    [enableClockMenu = config.enableClockMenu, &logger](ll::event::PlayerUseItemEvent& event) {
        if (enableClockMenu) {
            auto& player    = event.self();
            auto& itemStack = event.item();
            if (itemStack.getRawNameId() == "clock") {
                using namespace ll::form;
                ModalForm form("Warning", "Are you sure you want to kill yourself?", "Yes", "No");
                form.sendTo(player, [&logger](Player& player, ModalFormResult res, FormCancelReason) {
                    if (res.has_value() && res.value() == ModalFormSelectedButton::Upper) {
                        player.kill();
                        logger.info("{} killed themselves", player.getRealName());
                    }
                });
            }
        }
    });
```

- **Warning**: do not use `itemStack.getName()` — it returns the *display* name ("Clock", "Iron Sword").
- `ModalForm(title, content, upperBtn, lowerBtn)`; callback gets `(Player&, ModalFormResult, FormCancelReason)`. `ModalFormResult` is `std::optional<ModalFormSelectedButton>` (`Upper`/`Lower`).

## Running the mod

After a successful build, `bin/` contains a directory named after the mod. Copy it into the LeviLamina server's (or client's) `mods/` directory (on lip-managed servers `plugins/` links to `mods/`), creating it if needed, then run `bedrock_server_mod.exe` (or the LeviLamina client).

## Publishing the mod

1. Bump `version` in `tooth.json` (e.g. `0.1.0`).
2. Add a Keep-a-Changelog entry to `CHANGELOG.md` (optionally format with `npm install keep-a-changelog -g` + `changelog --format markdownlint`).
3. Create a GitHub release tagged `v0.1.0`.

GitHub Actions writes the changelog into the release body and auto-builds/uploads the zips.

**Warning**: the tag MUST start with `v` and follow SemVer, otherwise the mod will not be listed by LeviMC Mods / LeviLauncher.
