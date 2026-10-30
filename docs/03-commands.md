# 03 — Commands

Sources: https://docs.levimc.org/LeviLamina/developer_guides/how_to_guides/command_guide/ + https://docs.levimc.org/LeviLamina/api_reference/command/ (canonical markdown: `docs/main/contents/developer_guides/how_to_guides/command_guide.md`, `docs/main/contents/api_reference/command.md`).

## Headers

`ll/api/command/CommandRegistrar.h`, `ll/api/command/CommandHandle.h`, `ll/api/command/Overload.h`, `ll/api/command/Optional.h`, `ll/api/command/SoftEnum.h` (+ `Command.h`, `OverloadData.h`, `RuntimeOverload.h`, `ParamTraits.h`, `EnumName.h`).

## Registration model (compile-time registration)

1. Include the headers.
2. `CommandRegistrar::getInstance(bool isClientSide)` → shorthands `getServerInstance()` / `getClientInstance()`. **On BDS, mods can only register server commands** (`getInstance(false)`).
3. `getOrCreateCommand(name, description = {}, requirement = CommandPermissionLevel::Any, flag = CommandFlagValue::NotCheat, mod = NativeMod::current())` → `CommandHandle&`.
4. `command.overload<Params>()` — `Params` is a reflection-reflectable struct whose members become typed parameters; omit the template parameter for parameterless commands.
5. Chain `.text("sub")` (literal), `.required("member")`, `.optional("member")` — arguments are **member names of the struct** (compile-checked via `ParamName`). Other builder methods: `postfix`, `option`, `modify`.
6. `.execute(callback)` — callback signatures: `void(CommandOrigin const&, CommandOutput&, Params const&, ::Command const&)` (full), `void(CommandOrigin const&, CommandOutput&, Params const&)`, `void(CommandOrigin const&, CommandOutput&)`, or `void()`.

**Register commands inside a `ServerCommandRegisterEvent` listener** (see `docs/04-events.md`) — registration in `enable()` fails on the client because the client enables mods before any world loads. Example from the how-to guide:

```cpp
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"

struct NewCommand {
    enum class NewEnum { a = 0, b = 1, c = 2 } enum1;
    std::string str;
    int         number = 1;
};

void registerCommand() {
    using namespace ll::command;
    auto& command = CommandRegistrar::getInstance(false).getOrCreateCommand("new", "A example command");
    command.overload<NewCommand>().text("test").required("enum1").required("str").optional("number").execute(
        [](CommandOrigin const& origin, CommandOutput& output, NewCommand const& param) {
            output.success("test! {} {} {}", param.enum1, param.str, param.number);
        }
    );
}
```

In-game usage: `/new test <enum1> <str> [number]`. Optional members may be wrapped in `ll::command::Optional<T>` (check `.has_value()`):

```cpp
struct GreetParams {
    std::string name;
    ll::command::Optional<int> count;
};

cmd.overload<GreetParams>()
    .required("name")
    .optional("count")
    .execute([](CommandOrigin const&, CommandOutput& output, GreetParams const& params) {
        int n = params.count.has_value() ? params.count.value() : 1;
        for (int i = 0; i < n; ++i) output.success("Hello, {}!", params.name);
    });
```

## Enums

Plain `enum class` parameters must be registered first; the template overload uses magic_enum:

```cpp
enum class GameMode : int { Survival = 0, Creative = 1, Adventure = 2 };

struct ModeParams { GameMode mode; };

registrar.tryRegisterEnum<GameMode>();          // one-time registration
cmd.overload<ModeParams>().required("mode").execute([](CommandOrigin const&, CommandOutput& output, ModeParams const& p) {
    output.success("Mode set to {}", (int)p.mode);
});
```

## Soft enums (runtime-mutable value sets)

```cpp
registrar.tryRegisterSoftEnum("warps", {"spawn", "home", "shop"});
registrar.addSoftEnumValues("warps", {"arena"});
registrar.removeSoftEnumValues("warps", {"shop"});
registrar.setSoftEnumValues("warps", {"new"});
```

## Aliases and multiple overloads

```cpp
cmd.alias("tp");   // /teleport and /tp both work

struct TeleportCoords { int x, y, z; };
struct TeleportPlayer { std::string target; };

cmd.overload<TeleportCoords>().required("x").required("y").required("z").execute(...); // /warp <x> <y> <z>
cmd.overload<TeleportPlayer>().required("target").execute(...);                        // /warp <target>
```

## Permission levels

| Level | Meaning |
|---|---|
| `CommandPermissionLevel::Any` | All players |
| `GameDirectors` | Operators (OP) |
| `Admin` | Server admin |
| `Host` | Server host (console) |
| `Owner` | Server owner |
| `Internal` | Internal use |

## CommandRegistrar surface (verified against headers)

- `getOrCreateCommand(name, desc, requirement, flag, mod)`
- Enums: `hasEnum`, `tryRegisterEnum(name, values, type, parser)` / `tryRegisterEnum<T>()`, `addEnumValues`, `tryRegisterRuntimeEnum<T>()`
- Soft enums: `hasSoftEnum`, `tryRegisterSoftEnum`, `addSoftEnumValues`, `removeSoftEnumValues`, `setSoftEnumValues`
- Runtime/dynamic path: `CommandHandle::runtimeOverload(...)`, `registrar.compileCommand(...)`, `executeCommand(...)` (for building your own scripting engine — see LegacyScriptEngine `CommandAPI.cpp` and LeviLamina's `src-test/server/testCommand.cpp`)

## Server vs client commands (FAQ)

- Server commands: registered by the server, executable by the server and all connected players.
- Client commands: handled entirely by the client; only the registering client can run them.
- On **BDS** → only server commands. In a **local world** (client hosting) → both. In **multiplayer client** → only client commands.
- Commands registered directly in `enable()` work on BDS but fail on the client — always use `ServerCommandRegisterEvent` / `ClientCommandRegisterEvent`.

## Output

`output.success(fmt, args...)`, `output.error(fmt)` — fmt-style formatting; `CommandOutput` messages go to command sources (console, command blocks, players).
