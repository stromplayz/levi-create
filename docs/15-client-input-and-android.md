# 15 — Client Input & Android Native Mods

Sources: https://docs.levimc.org/LeviLamina/api_reference/input/ + the Android section https://docs.levimc.org/LeviLamina/developer_guides/android/ (canonical: `docs/main/contents/api_reference/input.md`, `docs/main/contents/developer_guides/android/**`).

## Part A — Input (Client) — `ll::api/input` (server builds do NOT have this)

Headers: `ll/api/input/KeyRegistry.h`, `KeyHandle.h`, `ll/api/event/input/KeyInputEvent.h`, `MouseInputEvent.h`.

```cpp
auto& myKey = ll::input::KeyRegistry::getInstance().getOrCreateKey("toggle", {'K'} /*, allowRemap = true */);

myKey.registerButtonDownHandler([](::FocusImpact, ::IClientInstance&) { /* pressed */ });
myKey.registerButtonUpHandler([](::FocusImpact, ::IClientInstance&) { /* released */ });
```

- Key codes are **Windows virtual-key codes**; the owner defaults to `mod::NativeMod::current()`; `KeyHandle` also exposes `getName/getFullName/getKeyCodes/isAllowRemap/setAllowRemap/setInputMappingStack`.
- Or consume raw input as cancellable events:

```cpp
bus.emplaceListener<ll::event::KeyInputEvent>([](ll::event::KeyInputEvent& event) {
    if (event.keyCode() == 'E' && event.isDown()) event.cancel();  // block default action
});
bus.emplaceListener<ll::event::MouseInputEvent>([](ll::event::MouseInputEvent& event) {
    auto x = event.x(); auto y = event.y();  // also dx(), dy(), buttonData(), actionButtonId()
});
```

- Gate client code behind `is_config("target_type", "client")` sections in `xmake.lua`.

## Part B — Android Native Mods (Preloader SDK — **not** LeviLamina!)

> Verbatim scope note: these docs are about **native mods for Minecraft Bedrock on Android**, loaded by Android LeviLauncher through the Preloader SDK ([LiteLDev/preloader-android](https://github.com/LiteLDev/preloader-android)) — **not by LeviLamina**. They use their own `pl::` API. Template: [QYCottage/levilauncher-android-mod-template](https://github.com/QYCottage/levilauncher-android-mod-template), reference implementation: `examples/full-cpp-mod`.

### Toolchain & packaging

- CMake + **Android NDK** (not Gradle). Fetch the SDK via FetchContent and link `preloader`:

```cmake
include(FetchContent)
FetchContent_Declare(preloader_android
    GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
    GIT_TAG 0.2.2)
FetchContent_MakeAvailable(preloader_android)
target_link_libraries(my_mod PRIVATE preloader)
```

- Build for **`arm64-v8a`** only. NDK path: `build.ps1 -Ndk <path>` or `ANDROID_NDK_HOME`/`ANDROID_NDK_ROOT`/`ANDROID_HOME`/`ANDROID_SDK_ROOT`.
- Output: a mod directory packed as **`.levipack`**:

```
full-cpp-mod/
├── manifest.json
├── libfull_cpp_mod.so
└── config/
    ├── config.json
    └── config.schema.json
```

- `manifest.json` (`type` must be `preload-native`): `{"type":"preload-native","name":"...","author":"...","version":"1.0.0","entry":"libfull_cpp_mod.so","minecraft_versions":[],"overwrite_files":[],"overwrite_folders":[]}` (+ optional `icon`). The **directory name is the runtime mod id** — keep it stable after release (paths, Mod Menu ownership, persisted state).

### Lifecycle (`<pl/Mod.hpp>`)

```cpp
class FullCppMod {
public:
    static FullCppMod& instance();
    FullCppMod() : mSelf(*ll::mod::NativeMod::current()) {}
    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }
    bool load();      // required — config, directories, state
    bool enable();    // optional — hooks, input callbacks, runtime UI
    bool disable();   // optional — undo
    bool unload();    // optional — release remaining state
private:
    ll::mod::NativeMod& mSelf;
};
PL_REGISTER_MOD(FullCppMod, FullCppMod::instance())   // exactly once per .so; instance lives for the process
```

`NativeMod` members: `getJavaVM()`, `getLogger()`, `getId()/getName()/getAuthor()/getVersion()`, `getEntryPath()/getEntryFileName()/getIconPath()`, `getModDir()/getDataDir()/getConfigDir()/getResourceDir()`, `getManifestPath()/getLibraryPath()`, `getState()/isLoaded()/isEnabled()/isDisabled()`. Do **not** throw across lifecycle boundaries — catch and return `false`.

### Typed config (`<pl/Config.hpp>`)

`pl::config::ConfigFile<T>` (hold as `std::optional`, `emplace()` + `load()` in `load()`): aggregate struct + integral `version` + default member initializers; merge-on-load (missing files created, new fields defaulted, user values preserved, unknown keys dropped, `$`-prefixed fields ignored); `save()` writes back; auto-generates `config.schema.json` (UI metadata via `pl::config::Schema<T>` specialization: `title`, `description`, `minimum`, `maximum`, `readOnly`). Enums serialize as strings via magic_enum; supported: aggregates, `std::string`, `bool`, integrals, floats, `std::vector<T>`, `std::optional<T>`.

### Mod Menu (`<pl/ModMenu.hpp>`)

```cpp
pl::modmenu::ModuleBuilder("full_cpp_mod.hud", "Full C++ Config Demo")
    .modId(getSelf().getId())
    .description("...")
    .defaultEnabled(config.showOverlay)
    .onToggle(callback)
    .config("opacity", "Opacity", pl::modmenu::ConfigType::SliderInt, "80", "0", "100")
    .registerModule();

pl::modmenu::ButtonBuilder("full_cpp_mod.quick_drop", "Quick Drop")
    .moduleId("full_cpp_mod.hud").modId(self.getId())
    .label("Q").androidKeyCode(45)
    .behavior(pl::modmenu::ButtonBehavior::Click).onEvent(cb)
    .registerButton();
```

- Config types: `Toggle`, `SliderInt`, `SliderFloat`, `Radio`, `Color`. Button behaviors: click/hold/toggle/pointer; icons via `pngIcon()/webpIcon()/svgIcon()/icon()`.
- HUD overlay: `registerFont(fontId, ttfData)`, `registerImage(imageId, rgba, w, h)` (size must be exactly `w*h*4`), `submitDrawCommands(moduleId, commands)` with `DrawCommand{type, x, y, w, h, fontId|imageId}`. Clean up temporary entries with `unregisterModule()/unregisterButton()` in `disable()`.

### Input (`<pl/Input.hpp>`)

```cpp
struct TouchEvent { int action; int pointerId; float x; float y; };
struct KeyEvent   { int keyCode; unsigned int unicodeChar; bool isKeyDown; };
struct MouseEvent { int button; bool isDown; };

pl::input::registerTouchCallback(cb);  // return true from a callback to consume
pl::input::registerKeyCallback(cb);
pl::input::registerMouseCallback(cb);
pl::input::showKeyboard(); pl::input::hideKeyboard();  // need the game Activity
```

**No unregister API exists** — register once per process and gate behavior on your own enabled flag; keep callbacks short and non-blocking.

### Memory: Hook / Signature / Patch (`pl/memory/*.hpp`)

- **Hook**: `pl::memory::hook(target, detour, &originalOut, priority)` → 0/-1; `unhook(target, detour)`; RAII `pl::memory::HookHandle(target, detour, &original, priority)` with `.installed()/.reset()`. Priority enum identical to LeviLamina's (lower runs earlier). Rules: detour signature must match exactly; call the trampoline (`originalFunc`), never the target (recursion); keep handles in mod-owned state.
- **Signature**: `resolveSignature(signature, moduleName)` / `resolveSignatures(span, moduleName)` → `uintptr_t` (**0 = not found — always check**). Pattern syntax: `48 8B ?? ?? 89` ≡ `488B????89`; `?`/`??` whole-byte wildcard, `A?` low-nibble, `?F` high-nibble. Example target: `libminecraftpe.so`.
- **Patch**: `writeBytes(address, "00 00 80 D2 C0 03 5F D6", name)` / `writeBytes(address, span, name)`, `readBytes(addr, len)`, `revertPatch(name)`, `revertAllPatches()`; RAII `pl::memory::PatchHandle(addr, hex, name)` with `.applied()/.reset()`. Notes: hex tokens are 1–2 digits, whitespace-separated; reusing a name replaces the saved record; wrong bytes/addresses still crash the process.
- Full worked example (`memory_hook_macros.md`): resolve `Game_update` by signature → construct `HookHandle(reinterpret_cast<void*>(target), reinterpret_cast<void*>(&updateHook), reinterpret_cast<void**>(&mOriginalUpdate))` → `.installed()` → `.reset()` in `disable()`.

### SDK headers & naming

Public headers (the complete list): `pl/Config.hpp`, `pl/Export.hpp`, `pl/Input.hpp`, `pl/Logger.hpp`, `pl/Mod.hpp`, `pl/ModMenu.hpp`, `pl/memory/Hook.hpp`, `pl/memory/Patch.hpp`, `pl/memory/Signature.hpp`. `PL_EXPORT` marks default-visibility symbols (rarely needed); `PL_REGISTER_MOD(Type, instanceExpr)` registers the lifecycle object. Naming (Lamina C++ style): files/types `UpperCamelCase`, functions/variables `lowerCamelCase`, private members `mUpperCamelCase`, constants `UpperCamelCase`, macros `UPPER_SNAKE_CASE`.

### Checklist (verbatim from the quick start)

- Build for `arm64-v8a`; link the `preloader` target; only public headers from `include/pl`.
- Register one long-lived object with `PL_REGISTER_MOD`; keep `ll::mod::NativeMod&` from `NativeMod::current()`.
- Load config before registering runtime UI.
- Store hook and patch handles in mod-owned state.
- Unregister temporary Mod Menu entries during `disable()`.
- Keep callbacks valid until unregistered or the mod unloads.
