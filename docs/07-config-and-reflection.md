# 07 — Config & Reflection

Sources: https://docs.levimc.org/LeviLamina/api_reference/config/ + https://docs.levimc.org/LeviLamina/api_reference/reflection/ + https://docs.levimc.org/LeviLamina/api_reference/reflection_advanced/ (canonical: `docs/main/contents/api_reference/{config,reflection,reflection_advanced}.md`).

## Config (`ll/api/Config.h`)

A config type must satisfy `ll::config::IsConfig`: an **aggregate** (no user-declared constructors, no private members, no virtual functions) with an integral `version` field.

```cpp
struct MyConfig {
    int         version        = 1;
    std::string greeting       = "Hello";
    int         maxPlayers     = 20;
    bool        enableFeatureX = false;
};

bool saveConfig(T const& config, std::filesystem::path const& path);
bool loadConfig(T& config, std::filesystem::path const& path, F&& updater = defaultConfigUpdater<T, J>);
```

`loadConfig` behavior:
1. File doesn't exist → saves the current config as default, returns `false`.
2. File exists but `version` differs → calls `updater`, returns `false`.
3. Version matches → deserializes directly, returns `true`.

Standard pattern (used in `src/levi_create/LeviCreate.cpp`):

```cpp
auto configPath = mod.getConfigDir() / "config.json";
if (!ll::config::loadConfig(config, configPath)) {
    logger.warn("Cannot load configurations from {}", configPath);
    ll::config::saveConfig(config, configPath);   // write defaults / migrated version
}
```

**Custom version migration** — the default updater merge-patches the old file onto the new defaults. For renames, write your own `bool(T&, nlohmann::ordered_json&)`:

```cpp
bool myUpdater(MyConfigV2& config, nlohmann::ordered_json& data) {
    int oldVersion = data.value("version", 1);
    if (oldVersion == 1 && data.contains("greeting")) {
        config.greeting = data["greeting"];
    }
    data.erase("version");
    auto patch = ll::reflection::serialize<nlohmann::ordered_json>(config);
    patch.value().merge_patch(data);
    data = *std::move(patch);
    return true;
}
ll::config::loadConfig(config, mod.getConfigDir() / "config.json", myUpdater);
```

JSON is `nlohmann::ordered_json` by default (preserves declaration order).

## Reflection (`ll/api/reflection/`)

Headers: `Reflection.h`, `Serialization.h`, `Deserialization.h`, `Serializer.h`, `Dispatcher.h`, `ReflectionError.h`, `TypeName.h`. Maps C++ structs ↔ JSON/NBT without hand-written readers; used by Config, the mod manifest, persistence, packets.

### Aggregates are free

```cpp
struct PlayerData { std::string name{}; int level{}; double health{}; };

auto json = ll::reflection::serialize<nlohmann::json>(player);      // -> ll::Expected<nlohmann::json>
auto back = ll::reflection::deserialize_to<PlayerData>(json.value()); // -> ll::Expected<PlayerData>
// write into an existing object: ll::reflection::deserialize(out, json)
// write into an existing JSON:   ll::reflection::serialize_to<J>(out, value)
```

Type mapping: arithmetic → number (strict), `std::string` → string, enums → name string (fallback to integer), `std::optional<T>` → `T` (**an empty optional omits the whole key** — never writes `null`), variant → current alternative, `pair/tuple/array` → array (exact count), `vector/list/set` → array, `map/unordered_map` → object (string-convertible keys), nested aggregates → object recursively.

### Strictness rules (memorize)

- **Every non-optional member must be present when deserializing.** Default member initializers are NOT fallbacks. Extra keys are ignored.
- **Numeric conversion never truncates**: out-of-range, fractional-into-integer, negative-into-unsigned are all errors.
- Errors carry the field path: `Reflection error in field 'values[0].number': field must be a number or boolean`.

### Custom simple types (`Serializer<T>` specialization)

```cpp
struct PlayerId {
    uint64 value{};
    std::string toString() const;
    static ll::Expected<PlayerId> tryParse(std::string_view text);
};

template <>
struct ll::reflection::Serializer<PlayerId> {
    // Layer 1: string form (associative keys + type_to_string/string_to_type)
    static std::string            to_string(PlayerId const& value) { return value.toString(); }
    static ll::Expected<PlayerId> from_string(std::string_view text) { return PlayerId::tryParse(text); }

    // Layer 2: render as a value inside objects
    template <typename J, typename F>
    static ll::Expected<J> serialize(PlayerId const& value, F const&) { return J{value.toString()}; }

    template <typename J, typename F>
    static ll::Expected<PlayerId> deserialize(J const& j, F const&) {
        if (!j.is_string()) return ll::reflection::makeDeserStringTypeError();
        auto const& text = j.template get_ref<typename std::remove_cvref_t<J>::string_t const&>();
        return from_string(std::string_view{text});
    }
};
```

LeviLamina itself does this for `mce::UUID` and `ll::data::Version`.

### Key formatters

```cpp
auto json = ll::reflection::serialize<nlohmann::json>(value, uppercaseAsciiKey);
auto obj  = ll::reflection::deserialize_to<MyType>(json, uppercaseAsciiKey);
```

Formatters reach member names + associative keys, never value specializations; per-type `operator()<T>` overloads affect writing only.

### Advanced (reflection_advanced.md) — the short version

- Dispatch is compile-time priority-tag based (arithmetic 11 → custom Serializer 10 → optional/Dispatcher 9 → vector 8 → enum 7 → variant 6 → strings 5 → tuple 4 → array 3 → associative 2 → aggregate 1 → fallback 0).
- Signature rules: value form `template <typename J, typename F> static ll::Expected<J> serialize(T const&, F const&)`; in-place form `static ll::Expected<> serialize(T const&, J&, F const&)` (+ mirrored `deserialize`). **Match signatures character-for-character or the specialization is silently ignored** (on aggregates there is no diagnostic — debug with `static_assert(ll::traits::always_false<T>, "")` inside it). **Never return `void`.**
- Member helpers: `field<"name">`, `required_field<"name">`, `default_field<"name">` (+ `member<&T::m>` forms) enable discriminator-driven parsing, flattening sub-objects, and non-default-constructible types (use the value form for those).
- `std::string_view` members deserialize only from a borrowable **lvalue** source — prefer `std::string` to own data.
- `Dispatcher<T, Listener>` fields fire `call()` after assignment/deserialization (config change notifications); movable, not copyable.
- Failure atomicity: the value form constructs into a temporary (target untouched on failure); the in-place form writes field-by-field.
