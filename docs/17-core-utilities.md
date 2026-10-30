# 17 — Core Utilities (Base, Expected, Utils)

Sources: https://docs.levimc.org/LeviLamina/api_reference/base/ + https://docs.levimc.org/LeviLamina/api_reference/expected/ + https://docs.levimc.org/LeviLamina/api_reference/utils/ (canonical: `docs/main/contents/api_reference/{base,expected,utils}.md`).

## Base (`ll/api/base/`) — foundation types

Headers: `Macro.h` (exports), `Concepts.h`, `TypeTraits.h`, `StdInt.h`, `FixedString.h`, `Containers.h` (`SmallDenseSet`, `SmallStringMap`, ...), `Meta.h` (`visitIndex`), `SourceLocation.h`, `ToString.h`, `CompilerPredefine.h`.

Export macros (switch on `LL_EXPORT`, set via `add_defines("LL_EXPORT")`):

| Macro | Building LeviLamina | Consumed by mod |
|---|---|---|
| `LLAPI` | `__declspec(dllexport)` | `__declspec(dllimport)` |
| `LLNDAPI` | `[[nodiscard]] LLAPI` | `[[nodiscard]] LLAPI` |
| `LLCAPI` | `extern "C" LLAPI` | `extern "C" LLAPI` |

Create your own export macros (rename per-mod):

```cpp
#ifdef MY_MOD_EXPORT
#define MY_MOD_API __declspec(dllexport)
#else
#define MY_MOD_API __declspec(dllimport)
#endif
// xmake.lua: add_defines("MY_MOD_EXPORT")
```

`ll::concepts::` (selected): `IsString<T>`, `IsOneOf<T, Ts...>`, `IsInTypes<T, U>`, `IsExpected<T>`, `IsLeviExpected<T>`, `IsOptional<T>`, `RangeLoopable<T>`, `Associative<T>`, `ArrayLike<T>`, `Specializes<T, Z>`, `TupleLike<T>`, `Stringable<T>`, `Awaitable<T>`, `Require<T, Pred>`.

```cpp
template <ll::concepts::IsString T>
void processString(T const& str) { /* std::string / string_view / char* ... */ }

template <ll::FixedString Name>      // compile-time strings (NTTP; used by i18n `_tr`)
struct NamedType { static constexpr auto name = Name; };
```

## Expected (`ll/api/Expected.h`) — error handling

```cpp
template <class T = void> using Expected = nonstd::expected<T, ll::Error>;
```

```cpp
class Error {
public:
    explicit operator bool() const noexcept;   // true if error exists
    std::string message() const noexcept;
    template <class T> bool isA() noexcept;
    template <class T> T& as() noexcept;
    Error& join(Error) noexcept;
    Error const& log(io::Logger&, io::LogLevel = io::LogLevel::Error) const noexcept;
    Error const& log(CommandOutput&, CommandOutputMessageType = ...) const noexcept;
};

class ErrorInfoBase {                       // subclass this for custom error types
public:
    virtual ~ErrorInfoBase();
    virtual std::string message() const noexcept = 0;
};
```

Factories: `makeError<T>(args...)`, `makeStringError(str)`, `makeErrorCodeError(ec|errc)`, `makeExceptionError(eptr = current_exception())`, `forwardError(err)`.

```cpp
ll::Expected<int> divide(int a, int b) {
    if (b == 0) return ll::makeStringError("Division by zero");
    return a / b;
}

auto result = divide(10, 0);
if (!result) { result.error().log(logger); return; }
logger.info("Result: {}", *result);

// propagation
auto x = divide(a, 2);
if (!x) return ll::forwardError(x.error());

// void form
ll::Expected<> initialize() { return {}; }   // success
```

Custom typed errors: subclass `ErrorInfoBase`, override `message()`, create with `ll::makeError<FileNotFoundError>(path)`, inspect with `result.error().isA<FileNotFoundError>()` / `.as<FileNotFoundError>()`. Mod lifecycle callbacks are `Expected<>`-flavored; reflection/config errors flow through the same type.

## Utils (`ll/api/utils/*.h`) — note the snake_case namespaces

```cpp
namespace ll::sys_utils {     // SystemUtils.h
    std::string getSystemName();
    std::string getSystemLocaleCode();
    data::Version getSystemVersion();
    void* getModuleHandle(void* addr);
    std::filesystem::path getModulePath(void* addr);
    std::string getModuleFileName(void* addr);
    std::optional<std::string> getEnvironmentVariable(std::string_view name);
}

namespace ll::string_utils {  // StringUtils.h
    std::vector<std::string> splitByPattern(std::string_view str, std::string_view pattern);
    std::string& replaceAll(std::string& str, std::string_view oldVal, std::string_view newVal);
    std::string& toLower(std::string& str);
    std::string& toUpper(std::string& str);
    std::string toCamelCase(std::string_view str);
    std::string toSnakeCase(std::string_view str);
}

namespace ll::file_utils {    // FileUtils.h
    std::optional<std::string> readFile(std::filesystem::path const& path);
    bool writeFile(std::filesystem::path const& path, std::string_view content);
    std::optional<data::Version> getVersion(std::filesystem::path const& path);
}

namespace ll::hash_utils {    // HashUtils.h
    size_t doHash(auto const&... args);
    struct HashCombiner { size_t hash = 0; HashCombiner& add(auto const& val); operator size_t() const; };
}

namespace ll::random_utils {  // RandomUtils.h
    int randomInt(int min, int max);
    double randomReal(double min, double max);
}

namespace ll::base64_utils {  // Base64Utils.h
    std::string encode(std::string_view data);
    std::string decode(std::string_view data);
}

namespace ll::compress_utils { // CompressUtils.h
    enum class CompressFormat : int { Gzip, Zlib };
    enum class CompressLevel : int { Store, Fastest, Default, Best };

    Expected<std::string> compress(std::string_view data, CompressFormat = Gzip, CompressLevel = Default);
    Expected<std::string> decompress(std::string_view data, CompressFormat = Gzip, size_t maxSize = 0);
    Expected<> compressFile(src, dst, ...);   // completes under a temp name — no half-written files
    Expected<> decompressFile(src, dst, ...);
    // streaming: Compressor codec{outputFn}; codec.status(); codec.write(chunk); codec.finish();
}
```

Safety notes: file APIs return `optional`/`bool` — check them; **`decompress` has no output limit unless `maxSize` is passed — always pass one for untrusted data** (small inputs can expand enormously); other headers without dedicated sections: `ErrorUtils.h`, `StacktraceUtils.h`, `FontUtils.h`.
