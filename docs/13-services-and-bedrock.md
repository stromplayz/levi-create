# 13 — Services & Bedrock Access

Source: https://docs.levimc.org/LeviLamina/api_reference/service/ (canonical: `docs/main/contents/api_reference/service.md`).

Headers: `ll/api/service/Service.h`, `ll/api/service/ServiceManager.h`, `ll/api/service/Bedrock.h`.

The Service module provides a service registry for **dependency injection between mods** and access to Bedrock engine internals (`Minecraft`, `Level`, `ServerInstance`, `CommandRegistry`, ...).

## Bedrock engine accessors (all return `optional_ref<T>` — always null-check)

```cpp
namespace ll::service::inline bedrock {
optional_ref<Minecraft>              getMinecraft(bool isClientSide = false);
optional_ref<Level>                  getLevel();
optional_ref<ServerInstance>         getServerInstance();
optional_ref<ServerNetworkHandler>   getServerNetworkHandler(bool isClientSide = false);
optional_ref<CommandRegistry>        getCommandRegistry(bool isClientSide = false);
optional_ref<RakNet::RakPeer>        getRakPeer();
optional_ref<NetworkSystem>          getNetworkSystem(bool isClientSide = false);
optional_ref<DBStorage>              getDBStorage();
optional_ref<ResourcePackRepository> getResourcePackRepository(bool isClientSide = false);
}
```

```cpp
auto level = ll::service::getLevel();
if (level) {
    // use level
}
```

These are invalid outside the server's lifetime — null-check every time (e.g. inside `LevelTickEvent` or after `ServerStartedEvent`).

## ServiceManager (registry + change notifications)

```cpp
namespace ll::service {
class ServiceManager {
public:
    static ServiceManager& getInstance();

    template <IsService T>
    Expected<std::shared_ptr<T>> getService();

    template <IsService T>
    event::ListenerPtr subscribeService(std::function<void(std::shared_ptr<T> const&)> const& fn);

    bool registerService(std::shared_ptr<Service> const& service, std::shared_ptr<mod::Mod> const& mod);
    bool unregisterService(ServiceIdView const& id);
};
}
```

## Registering a service (provider side)

```cpp
class MyService : public ll::service::ServiceImpl<MyService, 1> {  // 1 = version
public:
    void doSomething() {}
    void invalidate() override {}
};

ll::service::ServiceManager::getInstance()
    .registerService(std::make_shared<MyService>(), ll::mod::NativeMod::current());
```

## Consuming a service

```cpp
auto service = ll::service::ServiceManager::getInstance().getService<MyService>();
if (service) {
    (*service)->doSomething();
}
```

Or subscribe to lifecycle changes: `subscribeService<T>(callback)` (fires with the shared service instance on registration); the corresponding events are `ServiceRegisterEvent` / `ServiceUnregisterEvent` (see `docs/04-events.md`).

## Design context (from the Architecture page)

The Service system is LeviLamina's dependency-injection pattern: `ServiceManager` = container, `Service` = injectable component, `subscribeService()` = dependency resolution. Related singletons: `EventBus::getInstance()`, `CommandRegistrar::getInstance(bool)`, `ModManagerRegistry::getInstance()`.
