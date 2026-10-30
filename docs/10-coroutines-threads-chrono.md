# 10 — Coroutines, Threads & Chrono

Sources: https://docs.levimc.org/LeviLamina/developer_guides/how_to_guides/coro_guide/ + https://docs.levimc.org/LeviLamina/api_reference/thread/ + https://docs.levimc.org/LeviLamina/api_reference/chrono/ (canonical: `docs/main/contents/developer_guides/how_to_guides/coro_guide.md`, `docs/main/contents/api_reference/{thread,chrono}.md`).

## Coroutines (since LeviLamina 1.0.0 they replace the Scheduler)

1. Include `ll/api/chrono/GameChrono.h` + `ll/api/coro/CoroTask.h`.
2. Wrap a lambda returning `ll::coro::CoroTask` in `ll::coro::keepThis`.
3. `.launch(executor)` or `.syncLaunch(executor)` (the latter **blocks the server thread**), passing an executor.

**Available executors (tip, verbatim intent)**: `ll::thread::ServerThreadExecutor` (server thread, `ll/api/thread/ServerThreadExecutor.h`) and `ll::thread::ThreadPoolExecutor` (thread pool, `ll/api/thread/ThreadPoolExecutor.h`); more under `ll/api/thread/`.

```cpp
// every 20 game ticks, 20 times, on the server thread
void createServerThreadCoro() {
    using namespace ll::chrono_literals;
    ll::coro::keepThis([]() -> ll::coro::CoroTask<> {
        for (int i = 0; i < 20; ++i) {
            co_await 20_tick;
            std::cout << "This is coro in server thread\n";
        }
        co_return;
    }).launch(ll::thread::ServerThreadExecutor::getDefault());
}

// every real second, 20 times, on the thread pool
void createThreadPoolCoro() {
    using namespace ll::chrono_literals;
    ll::coro::keepThis([]() -> ll::coro::CoroTask<> {
        for (int i = 0; i < 20; ++i) {
            co_await 1s;
            std::cout << "This is coro in thread pool\n";
        }
        co_return;
    }).launch(ll::thread::ThreadPoolExecutor::getDefault());
}
```

Awaitables include game-tick literals (`20_tick`) and `std::chrono` durations (`1s`); `coro::Generator<...>` powers `KeyValueDB::iter()`; `coro::InterruptableSleep` powers wait-queues (see the SimpleTunnel example in `12-networking.md`).

## Thread executors (`ll/api/thread/`)

```cpp
class ThreadPoolExecutor : public coro::Executor {
public:
    explicit ThreadPoolExecutor(std::string name, size_t nThreads = 1);
    void resize(size_t nThreads);
    void destroy();
    void execute(std::function<void()>) const override;
    std::shared_ptr<data::CancellableCallback> executeAfter(std::function<void()>, Duration) const override;
    static ThreadPoolExecutor const& getDefault();
};

class ServerThreadExecutor : public coro::Executor {  // same execute/executeAfter/getDefault shape
};
```

- **Background work**: `ll::thread::ThreadPoolExecutor::getDefault().execute([] { ... });`
- **Game-state work** (safe to touch `Player`/`Level`): `ll::thread::ServerThreadExecutor::getDefault().execute([] { ... });`
- **Delayed execution** (cancellable):

```cpp
auto callback = executor.executeAfter([] { /* runs after 5s */ }, std::chrono::seconds(5));
// callback->cancel();
```

- Custom pools: `ll::thread::ThreadPoolExecutor myPool("MyPool", 4); myPool.resize(8);`
- Other headers: `GlobalThreadPauser.h` (pause all game threads), `ThreadName.h`.
- ⚠️ The Thread page does not document `ClientThreadExecutor` / `TickSyncSleep` — they exist in headers/ecosystem code (e.g. Network examples use `ClientThreadExecutor`) but are not part of this page's documented surface.

## Chrono (`ll/api/chrono/GameChrono.h`)

Game time = **ticks** (20 ticks = 1 real second). Durations in `ll::chrono::game`:

| Type | Definition | Equivalent |
|---|---|---|
| `game::ticks` | `duration<int64, ratio<1, 20>>` | 1 tick = 50ms |
| `game::days` | 24000 ticks | 1 Minecraft day |
| `game::hours` | 1000 ticks | 1 Minecraft hour |
| `game::minutes` | ~16.67 ticks | 1 Minecraft minute |
| `game::weeks` / `months` / `years` | multiples of game days | (year = 365.2425 game days in code) |

```cpp
using namespace ll::chrono_literals;
auto tenTicks = 10_tick;
auto halfDay  = 12000_tick;

auto now = ll::chrono::GameTickClock::now();        // advances only while the game ticks
auto tp  = ll::chrono::GameTickClock::fromTick(100);
auto serverNow = ll::chrono::ServerClock::now();    // wall-clock, steady, tick-typed duration

// measuring in-game intervals
static auto lastCheck = GameTickClock::now();
if (GameTickClock::now() - lastCheck >= game::days{1}) { lastCheck = GameTickClock::now(); }

// converting ticks -> real time
auto realSeconds = std::chrono::duration_cast<std::chrono::seconds>(game::ticks{100}); // 5s
```

`ServerClock` and `GameTickClock` both have `duration = ticks`; literals live in `ll::chrono_literals` (a.k.a. `ll::literals::chrono_literals`).
