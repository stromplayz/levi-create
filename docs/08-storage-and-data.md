# 08 — Storage & Data

Source: https://docs.levimc.org/LeviLamina/api_reference/data/ (canonical: `docs/main/contents/api_reference/data.md`).

Headers: `ll/api/data/Observable.h`, `KeyValueDB.h`, `DependencyGraph.h`, `Version.h`, `VersionRequirement.h`, `AnyFunction.h`, `CancellableCallback.h`, `ConcurrentPriorityQueue.h`, `TightPair.h`, `IndirectValue.h`.

## KeyValueDB (persistent storage — the standard mod database)

LevelDB-backed key-value store; **strings in, strings out**.

```cpp
namespace ll::data {
class KeyValueDB {
public:
    class WriteBatch {
    public:
        WriteBatch& set(std::string_view key, std::string_view val);
        WriteBatch& del(std::string_view key);
    };

    explicit KeyValueDB(std::filesystem::path const& path);
    KeyValueDB(std::filesystem::path const& path, bool createIfMiss, bool fixIfError, int bloomFilterBit);

    std::optional<std::string> get(std::string_view key) const;
    bool has(std::string_view key) const;
    bool empty() const;
    bool set(std::string_view key, std::string_view val);
    bool del(std::string_view key);
    bool write(WriteBatch const& batch);

    coro::Generator<std::pair<std::string_view, std::string_view>> iter() const;
};
}
```

Usage (from the API page):

```cpp
ll::data::KeyValueDB db(mod.getDataDir() / "mydata");

db.set("player_score", "100");
auto score = db.get("player_score");   // std::optional<std::string> -> check before use
if (db.has("player_score")) db.del("player_score");

// atomic multi-update
ll::data::KeyValueDB::WriteBatch batch;
batch.set("player_score", "125").set("last_reward", "daily").del("pending_reward");
db.write(batch);

for (auto [key, value] : db.iter()) { /* scan all entries */ }
```

Lifecycle rule (from the tutorial): the DB is move-only → hold it in `std::unique_ptr<ll::data::KeyValueDB>` (as `src/levi_create/LeviCreate.cpp` does), initialize it in `load()`, never a raw pointer. Location: `getSelf().getDataDir() / "players"` style subpaths. Keys: player UUID strings (`uuid.asString()`), never XUID.

## Observable<T> (shared reactive value — general purpose, not just UI)

```cpp
template <class T>
class Observable {
public:
    using Callback       = brstd::move_only_function<void(T const&)>;
    using SubscriptionId = std::uint32_t;

    explicit Observable(T initial);
    T     getData() const;
    void  setData(T value);
    SubscriptionId subscribe(Callback callback);
    bool  unsubscribe(SubscriptionId id);
};
```

- Copies **share** value + subscribers + id sequence; a separate construction creates independent state.
- `setData()` ignores equal values; notifications run synchronously before `setData()` returns; callback exceptions propagate (never swallowed).
- Subscription ids are **not RAII** — discarding the id does not unsubscribe; empty callback returns id `0`, `unsubscribe(0)` is `false`.
- **Warning (verbatim intent)**: no internal locking — synchronize externally; do not add/remove subscriptions on the same observable from one of its callbacks; recursive `setData()` executes immediately.
- `T` must be copyable (`getData`) and equality-comparable (`setData`). UI variants: `ObservableBoolean/Number/String/UIRawMessage` (see `06-data-driven-ui.md`).

## Version & VersionRequirement

```cpp
ll::data::Version current{"1.3.0-beta.1+windows.5"};
bool newer = ll::data::Version{"1.3.0"} > current;                    // prerelease affects precedence
bool same  = ll::data::Version{"1.3.0+first"} == ll::data::Version{"1.3.0+second"}; // build metadata ignored
bool ident = a.isIdenticalTo(b);                                      // build metadata matters here

ll::data::VersionRequirement supported{"^1.2.3 || =2.0.0"};
supported.matches(ll::data::Version{"1.8.0"});                        // true
```

Range syntax: `=1.2.3` exact, `>=1.2.0 <2.0.0` AND-group, `^1.2.3` compatible, `~1.2.3` patch-level, `1`/`1.2`/`1.2.x` partial, `*` any, `1.2.3 - 2.0.0` hyphen, `||` alternatives. ⚠️ A bare `1.2.3` currently means `>=1.2.3 <2.0.0` (legacy) and emits a migration warning in manifests — use `=1.2.3` for exact.

## Where used in this repo

- `src/levi_create/LeviCreate.cpp` — `playerDb` under `getDataDir()/"players"` keyed by UUID (first-join tracking).
