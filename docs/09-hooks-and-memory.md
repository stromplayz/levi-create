# 09 — Hooks & Memory

Sources: https://docs.levimc.org/LeviLamina/developer_guides/how_to_guides/hook_guide/ + https://docs.levimc.org/LeviLamina/api_reference/memory/ + https://docs.levimc.org/LeviLamina/developer_guides/how_to_guides/find_function_guide/ + https://docs.levimc.org/LeviLamina/developer_guides/how_to_guides/interface_export_guide/ (canonical: `docs/main/contents/developer_guides/how_to_guides/{hook_guide,find_function_guide,interface_export_guide}.md`, `docs/main/contents/api_reference/memory.md`).

## Hook macros (`ll/api/memory/Hook.h`)

Eight macros — the flags compose: **INSTANCE** (member functions; the compiler-generated `this` param is dropped) vs **STATIC**; **TYPE** (DEF_TYPE inherits the target class so you can call its members) vs plain; **AUTO** (registers automatically at mod load) vs manual `hook()`/`unhook()`.

```cpp
#define LL_TYPE_STATIC_HOOK(DEF_TYPE, PRIORITY, TYPE, IDENTIFIER, RET_TYPE, ...)
#define LL_STATIC_HOOK(DEF_TYPE, PRIORITY, IDENTIFIER, RET_TYPE, ...)
#define LL_TYPE_INSTANCE_HOOK(DEF_TYPE, PRIORITY, TYPE, IDENTIFIER, RET_TYPE, ...)
#define LL_INSTANCE_HOOK(DEF_TYPE, PRIORITY, IDENTIFIER, RET_TYPE, ...)
#define LL_AUTO_TYPE_STATIC_HOOK(DEF_TYPE, PRIORITY, TYPE, IDENTIFIER, RET_TYPE, ...)
#define LL_AUTO_STATIC_HOOK(DEF_TYPE, PRIORITY, IDENTIFIER, RET_TYPE, ...)
#define LL_AUTO_TYPE_INSTANCE_HOOK(DEF_TYPE, PRIORITY, TYPE, IDENTIFIER, RET_TYPE, ...)
#define LL_AUTO_INSTANCE_HOOK(DEF_TYPE, PRIORITY, IDENTIFIER, RET_TYPE, ...)
```

Parameter meanings: `DEF_TYPE` = your hook type name; `PRIORITY` = `ll::memory::HookPriority` (`Highest=0, High=100, Normal=200, Low=300, Lowest=400` — lower runs earlier; **docs recommend staying at `Normal`**); `TYPE` = class being hooked; `IDENTIFIER` = decorated MSVC name, function bytecode, or function definition pointer (`&DedicatedServer::$ctor`); `RET_TYPE`; then the parameter list. Inside the body, `origin(args...)` calls the original.

Canonical example (hooking the DedicatedServer constructor):

```cpp
#include "ll/api/memory/Hook.h"
#include "ll/api/io/LoggerRegistry.h"
#include "mc/server/DedicatedServer.h"

auto dedicatedServerLogger = ll::io::LoggerRegistry::getInstance().getOrCreate("DedicatedServer");

LL_AUTO_TYPE_INSTANCE_HOOK(
    DedicatedServerCtorHook,
    ll::memory::HookPriority::Normal,
    DedicatedServer,
    &DedicatedServer::$ctor,
    void*
) {
    dedicatedServerLogger->info("DedicatedServer::DedicatedServer");
    return origin();
}
```

- **Registration**: non-AUTO hooks call `hook()` when needed; **unloading**: all hooks unload automatically when Minecraft unloads, or call `unhook()`.
- `HookRegistrar<T...>` (RAII, ref-counted) is used inside event emitters (see `04-events.md`).
- **Warning (verbatim intent)**: hooks are a double-edged sword — misuse can crash Minecraft's core or mods and, in severe cases, damage saves. Check code carefully.
- Platform note: use function pointers (`&ClassName::method`) instead of symbol-name strings.

## Memory utilities (`ll/api/memory/Memory.h`)

```cpp
ll::memory::modify(void* ptr, size_t len, callback);          // make writable, run callback, restore protection
ll::memory::addressCall<R>(void const* address, args...);      // call function at raw address
ll::memory::virtualCall<R>(void const* self, ptrdiff_t vIndex, args...); // call virtual at vtable index
ll::memory::dAccess<T>(void* ptr, ptrdiff_t off);              // access member at offset
```

Low-level: `int hook(target, detour, originalFunc*, priority, suspendThreads=true)` / `bool unhook(target, detour, ...)`.

## Finding functions (reverse engineering BDS)

Tools: **IDA Pro** (or IDA Free — but it cannot decompile to C). Method (from the guide's worked examples):

1. Start from LeviLamina's **fake headers** (`src/mc/...`) and walk the inheritance chain (e.g. `Player : Mob : Actor` — `getPosition()` lives on `Actor`).
2. Confirm a candidate by reading IDA assembly/pseudocode. Example: `Actor::getPosition` is literally `return *(Vec3**)(this+0x2A0);` (symbol `?getPosition@Actor@@QEBAAEBVVec3@@XZ`).
3. To modify behavior (not just call), trace data flow: for the chat-forwarding example, `std::string` constructor sites inside `ServerNetworkHandler::handle` + the `ServerNetworkEventCoordinator::sendEvent` → `_displayGameMessage` chain identified `v17` as the chat message.
4. **Danger (verbatim intent)**: LeviLamina's structure info is unofficial — structures may change between BDS versions without timely updates. Be careful when replacing offsets with structures; submit PRs for fixes.
5. Multiple-inheritance `this` adjustments (`(char*)this - 16`) are handled by LeviLamina's hook helper `thisFor<T>()`; prefer official struct fields (`textPacket.mMessage` at `this+0x58`) over raw offsets.

## Cross-mod interface export

```cpp
#ifdef MOD_EXPORT
#define MODAPI __declspec(dllexport)
#else
#define MODAPI __declspec(dllimport)
#endif
```

- Add `add_defines("MOD_EXPORT")` to the exporting mod's `xmake.lua`, mark exported functions/methods with `MODAPI`; consumers compile the same header without the define (dllimport).
- **Rename `MOD_EXPORT`/`MODAPI` per mod** to avoid macro collisions with other mods.
- Related macros: `LLAPI` (export), `LLNDAPI` (`[[nodiscard]]` export), `LLCAPI` (`extern "C"` export) — switched by `LL_EXPORT`.

## API surface index

`ll/api/memory/Hook.h` (macros + HookPriority + hook/unhook + HookRegistrar), `Memory.h` (modify/addressCall/virtualCall/dAccess), `Signature.h` (binary pattern matching — no doc-page examples; see the Android signature API in `15-client-input-and-android.md` for pattern syntax), `Symbol.h` (symbol resolution).
