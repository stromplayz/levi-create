# 11 — I18n & Logging

Sources: https://docs.levimc.org/LeviLamina/developer_guides/how_to_guides/i18n_guide/ + https://docs.levimc.org/LeviLamina/api_reference/i18n/ + https://docs.levimc.org/LeviLamina/api_reference/io/ (canonical: `docs/main/contents/developer_guides/how_to_guides/i18n_guide.md`, `docs/main/contents/api_reference/{i18n,io}.md`).

## I18n (`ll/api/i18n/I18n.h`)

Singleton: `ll::i18n::getInstance()`. Load in the mod's `load()`; `load()` accepts a **directory** of per-locale JSON files or a **single** JSON file with locale codes as top-level keys.

```cpp
bool MyMod::load() {
    if (auto res = ll::i18n::getInstance().load(getSelf().getLangDir()); !res) {
        getSelf().getLogger().error("i18n load failed");
        res.error().log(getSelf().getLogger());
    }
    return true;
}
```

Standard layout (`Mod::getLangDir()`):

```
lang/
├── en.json
├── zh.json
├── ja.json
└── ... (de, fr, id, it, ko, pt_BR, ru, th, tr, vi, zh_CN, zh_TW)
```

Flat JSON maps — key = source string, value = translation, `{}` (or `{0}`) placeholders:

```json
{
  "hello.world": "Hello, World!",
  "hello.user": "Hello, {}!",
  "item.count": "You have {} items"
}
```

Translate with literals (`ll::literals::i18n_literals` a.k.a. `ll::i18n_literals`):

```cpp
using namespace ll::literals::i18n_literals;

"Hello, world!"_tr();                    // default locale
"Mod {0} already loaded"_tr("parameter"); // with args
"hello.world"_trl("zh");                  // explicit locale
"hello.user"_trl("en", "Bob");            // explicit locale + args
```

Runtime updates: `i18n.set("en", "custom.key", "Custom message");` / `i18n.get(key, localeCode)` / `clear()`. `load()` returns `Expected<>`.

## IO & Logger (`ll/api/io/`)

Headers: `Logger.h`, `LogLevel.h`, `LoggerRegistry.h`, `Sink.h`, `FileSink.h`, `ConsoleSink.h`, `RotatePolicy.h`, `FileUtils.h`, `StdoutRedirector.h`.

```cpp
enum class LogLevel : int { Off = -1, Fatal, Error, Warn, Info, Debug, Trace };
```

The mod's own logger (`Mod::getLogger()`) starts with two sinks: console `"ll_default_console"` and `logs/latest.log` file `"ll_default_file"`.

```cpp
auto& logger = mod.getLogger();
logger.info("Server started");
logger.warn("Low memory: {} MB", 128);
logger.error("Failed to load config");
logger.debug("Player count: {}", 5);           // fmt-style, compile-time checked

logger.setLevel(ll::io::LogLevel::Warn);        // filter
logger.setFlushLevel(ll::io::LogLevel::Error);  // auto-flush

if (logger.shouldLog(ll::io::LogLevel::Debug)) {
    logger.debug("Debug info: {}", computeExpensiveDebugInfo());  // guard expensive payloads
}
```

Custom sinks / restyling (thread-safe via `setSink`):

```cpp
logger.setSink("ll_default_console",
    std::make_shared<ll::io::ConsoleSink>(makePolymorphic<ll::io::PatternFormatter>("[{2}] {0}")));

logger.setSink("mymod",
    std::make_shared<ll::io::FileSink>(
        "logs/mymod.log",
        makePolymorphic<ll::io::PatternFormatter>("[{3:.3%F %T.} {2}][{1}] {0}", false)));
```

Rotation — `FileSink` only appends unless given a `RotatePolicy` (defaults are sane: 32 MiB, daily interval, gzip on, keep 1024 archives / 120 days / 512 MiB):

```cpp
ll::io::RotatePolicy policy{};
policy.maxFileSize = 8ull * 1024 * 1024;
policy.interval    = ll::io::RotateInterval::Daily;
policy.maxFiles    = 64;

logger.setSink("mymod", std::make_shared<ll::io::FileSink>("logs/mymod.log",
    makePolymorphic<ll::io::PatternFormatter>("[{3:.3%F %T.} {2}][{1}] {0}", false), policy));
```

Gotchas (verbatim intent): messages are written on a background thread — prefer `setFormatter`/`setFlushLevel`/`setSink` over mutating a sink fetched via `getSink`; `FileSink`s pointing at the same normalized path share file state/rotation but keep their own formatters; rotate triggers (size/time) are independent — whichever comes first wins; retention (`maxFiles`/`maxAgeDays`/`totalSizeCap`) unions independently.

## FileUtils (quick reference)

```cpp
namespace ll::file_utils {
std::optional<std::string> readFile(std::filesystem::path const& path);
bool writeFile(std::filesystem::path const& path, std::string_view content);
std::optional<data::Version> getVersion(std::filesystem::path const& path);
}
```
