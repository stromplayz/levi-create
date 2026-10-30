# 04 — Events

Sources: https://docs.levimc.org/LeviLamina/developer_guides/how_to_guides/event_guide/ + https://docs.levimc.org/LeviLamina/api_reference/event/ (canonical markdown: `docs/main/contents/developer_guides/how_to_guides/event_guide.md`, `docs/main/contents/api_reference/event.md`).

## Headers

`ll/api/event/EventBus.h`, `Event.h`, `EventId.h`, `Listener.h`, `ListenerBase.h`, `Cancellable.h`, `Emitter.h`, `DynamicListener.h`, `MultiListener.h`. Built-in events live in category subdirectories: `ll/api/event/{player,entity,world,server,command,service,input,...}/`.

## Listening

```cpp
using namespace ll::event;
EventBus& bus = EventBus::getInstance();

bus.emplaceListener<ServerStartedEvent>([](ServerStartedEvent& event) {
    // runs after the server started
});
```

- `emplaceListener<T>(callable [, EventPriority])` creates + registers in one call and returns a listener pointer (use it later with `bus.removeListener(ptr)` if needed).
- Priorities: `Highest = 0, High = 100, Normal = 200, Low = 300, Lowest = 400` — **lower value runs earlier**.
- LeviLamina automatically removes a mod's listeners when the mod is disabled.

## Cancelling

Cancellable events derive `Cancellable<T>`: call `event.cancel()` (or `setCancelled(bool)`).

```cpp
bus.emplaceListener<ll::event::player::PlayerChatEvent>(
    [](ll::event::player::PlayerChatEvent& event) { event.cancel(); }, ll::event::EventPriority::High);
```

## MultiListener (one callback, many events)

```cpp
auto listener = ll::event::MultiListener<
    ll::event::player::PlayerJoinEvent,
    ll::event::player::PlayerDisconnectEvent>::create([](auto&& event) { /* either event */ });
ll::event::EventBus::getInstance().addListener(listener);
```

## Built-in events (Common unless noted)

**Player** (`ll::event::player`): `PlayerJoinEvent`, `PlayerDisconnectEvent`, `PlayerChatEvent`*, `PlayerAttackEvent`*, `PlayerDestroyBlockEvent`*, `PlayerPlaceBlockEvent`*, `PlayerInteractBlockEvent`*, `PlayerUseItemEvent`*, `PlayerSwingEvent`, `PlayerJumpEvent`, `PlayerSneakEvent`, `PlayerSprintEvent`, `PlayerPickUpItemEvent`*, `PlayerRespawnEvent`, `PlayerDieEvent`, `PlayerChangePermEvent`, `PlayerAddExperienceEvent` (* = cancellable).

**Entity**: `ActorHurtEvent`*, `MobDieEvent`.
**World**: `LevelTickEvent`, `SpawnMobEvent`*, `BlockChangedEvent`, `FireSpreadEvent`*.
**Server**: `ServerStartedEvent`, `ServerStoppingEvent` (available on both builds but fire only server-side).
**Command**: `ExecuteCommandEvent`* (intercept any command execution).
**Service**: `ServiceRegisterEvent`, `ServiceUnregisterEvent`.
**Client-only** (`src-client/`): `ClientJoinLevelEvent`, `ClientExitLevelEvent`, `ClientStartJoinLevelEvent`, `ClientCancelJoinLevelEvent`, `KeyInputEvent`*, `MouseInputEvent`*, `BeforeUIRenderEvent`, `AfterUIRenderEvent`.

## Publishing custom events (mod-to-mod API)

An event class must be **`final`** and derive `ll::event::Event` (or `Cancellable<Event>` for cancellable ones). To publish, wire an emitter backed by a hook:

```cpp
// ServerStartedEvent.h
#pragma once
#include "ll/api/event/Event.h"
#include "mc/server/ServerInstance.h"

namespace ll::event::inline server {
class ServerStartedEvent final : public Event {
    ServerInstance& mInstance;

public:
    constexpr explicit ServerStartedEvent(ServerInstance& server) : mInstance(server) {}

    LLAPI void serialize(CompoundTag&) const override;
    LLNDAPI ServerInstance& server() const;
};
} // namespace ll::event::inline server
```

```cpp
// ServerStartedEvent.cpp
#include "ll/api/event/server/ServerStartedEvent.h"
#include "ll/api/event/Emitter.h"
#include "ll/api/event/EventRefObjSerializer.h"
#include "ll/api/memory/Hook.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/scripting/ServerScriptManager.h"

namespace ll::event::inline server {

void ServerStartedEvent::serialize(CompoundTag& nbt) const {
    Event::serialize(nbt);
    nbt["server"] = serializeRefObj(server());
}

ServerInstance& ServerStartedEvent::server() const { return mInstance; }

LL_TYPE_INSTANCE_HOOK(
    ServerStartedEventHook,
    ll::memory::HookPriority::Normal,
    ServerScriptManager,
    &ServerScriptManager::$onServerThreadStarted,
    EventResult,
    ::ServerInstance& ins
) {
    auto result = origin(ins);
    EventBus::getInstance().publish(ServerStartedEvent(ins));
    return result;
}

static std::unique_ptr<EmitterBase> emitterFactory();
class ServerStartedEventEmitter : public Emitter<emitterFactory, ServerStartedEvent> {
    memory::HookRegistrar<ServerStartedEventHook> hook;
};

static std::unique_ptr<EmitterBase> emitterFactory() { return std::make_unique<ServerStartedEventEmitter>(); }
} // namespace ll::event::inline server
```

Rules:
- `publish<T>` requires `is_final_v<T>`.
- Emitter pattern (mandatory for hook-backed events): forward-declare + define `static std::unique_ptr<EmitterBase> emitterFactory();`, subclass `Emitter<emitterFactory, YourEvent>` holding a `memory::HookRegistrar<YourHook>` — this ties hook activation to listener presence (lazy enable/disable).
- **Cancellable publishing**: check `ev.isCancelled()` after publishing inside the hook and short-circuit `origin(...)`:

```cpp
auto ev = PlayerAttackEvent(*this, ac, cause);
EventBus::getInstance().publish(ev);
if (ev.isCancelled()) return false;
return origin(ac, cause, doPredictiveSound);
```

## Exporting events across mods

`LLAPI`/`LLNDAPI` expand to `__declspec(dllexport)` while `LL_EXPORT` is defined (LeviLamina's own build) and `__declspec(dllimport)` for consumers. Mirror it per-mod:

```cpp
#ifdef MOD_EXPORT
#define MODAPI __declspec(dllexport)
#else
#define MODAPI __declspec(dllimport)
#endif
```

…with `add_defines("MOD_EXPORT")` in `xmake.lua` (rename the macros per-mod to avoid collisions). Make event accessors usable header-side so other mods don't need to link to yours; implement `serialize` to make events console-friendly.

## EventBus API surface

`getInstance()`, `publish(T&&)` / `publish(modName, T&&)`, `addListener`, `removeListener`, `emplaceListener<T, L, Args...>`, `getListenerCount<T>()`, `hasEvent(EventIdView)`, `hasListener(ListenerId)`, `setEventEmitter(factory, eventId, mod)`, plus `events()` generators.
