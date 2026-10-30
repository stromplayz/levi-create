# 06 — Data-driven UI (`ll::ui`)

Sources: https://docs.levimc.org/LeviLamina/developer_guides/how_to_guides/data_driven_ui_guide/ + https://docs.levimc.org/LeviLamina/api_reference/data_driven_ui/ (canonical: `docs/main/contents/developer_guides/how_to_guides/data_driven_ui_guide.md`, `docs/main/contents/api_reference/data_driven_ui.md`).

`ll::ui` is LeviLamina's typed C++ interface to Minecraft Bedrock's **data-driven UI (DDUI)** framework: form data lives in a synchronized data store, so text, visibility, disabled states, ranges and input values can change **while the screen is open**. Feature baseline: `@minecraft/server-ui` 2.1.0 stable (no `image()` component).

Headers: `ll/api/ui/form/CustomForm.h`, `ll/api/ui/form/MessageBox.h`, `ll/api/ui/base/UIRawMessage.h`, `ll/api/ui/base/Observable.h`, `ll/api/ui/base/ScreenSession.h`, `ll/api/ui/base/DataStore.h`.

| Legacy `ll::form` | DDUI `ll::ui` |
|---|---|
| Static form payload | Reactive data bindings |
| Indexed response | Typed Observables |
| Re-send to update | `Observable::setData()` while open |
| Selection at close | Inline button callbacks run while open |

## Non-negotiable lifecycle rules

- **Server thread only** — building, showing, updating, closing, and destroying handles (including `showAsync()` tasks) must run on the server thread. `setData()` on a bound observable writes into the player's DDUI data store → server thread too.
- Forms are **move-only** and **show-once**: any structural method after the first `show()`/`showAsync()` throws `std::logic_error` (even if startup later failed). Never call both `show()` and `showAsync()`.
- `show()` returns `Expected<>` reporting *startup only*; the close result arrives via callback (`Expected<ScreenCloseReason>`) or `showAsync()`.
- Structure is immutable after the first show attempt, but bound **values** stay mutable. To change content: `setData()`, never rebuild/re-show.
- Input controls (dropdown/slider/textField/toggle) require observables constructed with `{.clientWritable = true}` — passing a read-only observable throws `std::invalid_argument`. Read-only observables are for labels, visibility, disabled state, slider bounds.
- Numeric values must be finite (NaN/inf rejected).

## Observables

```cpp
ObservableString status{"Ready"};                                // server -> UI only
ObservableString name{"Steve", {.clientWritable = true}};        // two-way
ObservableBoolean pvp{true, {.clientWritable = true}};
ObservableNumber difficulty{1.0, {.clientWritable = true}};

auto const id = name.subscribe([status](std::string const& value) mutable {
    status.setData("Name: " + value);
});
// later: name.unsubscribe(id);
```

- Copies **share state** (value + subscribers + id sequence); `setData()` ignores equal values and notifies synchronously; no internal locking; never (un)subscribe from inside your own callback; subscription ids are **not RAII** — call `unsubscribe(id)` explicitly.
- Types: `ObservableBoolean`, `ObservableNumber` (double), `ObservableString`, `ObservableUIRawMessage`.

## Complete CustomForm

```cpp
using namespace ll::ui;

CustomForm form{player, UIRawMessage::translate("example.settings.title")};
form.header("General")
    .label(status)
    .divider()
    .textField("Player name", name, {.description = std::string{"Shown in this example"}})
    .toggle("Enable PvP", pvp, {.description = std::string{"Allow player combat"}})
    .dropdown("Difficulty", difficulty,
              {{"Peaceful", 0.0, std::string{"No hostile mobs"}},
               {"Easy", 1.0, std::string{"Reduced damage"}},
               {"Normal", 2.0, std::string{"Standard rules"}},
               {"Hard", 3.0, std::string{"Increased challenge"}}})
    .slider("Music volume", volume, 0.0, 100.0, {.description = std::string{"Percent"}, .step = 5.0})
    .spacer()
    .button("Reset", [name, pvp, difficulty, volume, status]() mutable { /* setData defaults */ },
            {.tooltip = std::string{"Restore default values"}})
    .closeButton();

auto started = form.show([name, pvp, difficulty, volume, nameSubscription](CustomForm::Result result) mutable {
    name.unsubscribe(nameSubscription);
    if (!result) return;                       // player left / server stopped / runtime error
    auto const closeReason    = result.value();
    auto const finalName      = name.getData();  // read final values from the observables
    // validate + persist...
});
if (!started) name.unsubscribe(nameSubscription); // handle started.error()
```

Component methods (all chainable, layout = call order): `button(label, callback, opts)`, `closeButton()`, `divider(opts)`, `dropdown(label, ObservableNumber, items, opts)`, `header(text, opts)`, `label(text, opts)`, `slider(label, value, min, max, opts)`, `spacer(opts)`, `textField(label, ObservableString, opts)`, `toggle(label, ObservableBoolean, opts)`. Options structs: `ButtonOptions{disabled, tooltip, visible}`, `SliderOptions{description, disabled, step, visible}`, etc. — every option member can itself be an observable (`BooleanValue`/`NumberValue` variants) for reactive styling. `DropdownItemData{label, value, description?}`.

## Coroutine path

```cpp
ll::coro::CoroTask<> showSettingsAsync(Player& player) {
    ll::ui::ObservableBoolean enabled{false, {.clientWritable = true}};
    ll::ui::CustomForm form{player, "Async settings"};
    form.toggle("Enabled", enabled).closeButton();

    auto result = co_await form.showAsync();   // task must run on the server thread
    if (!result) co_return;
    auto const finalValue = enabled.getData();
}

void launchSettings(Player& player) {
    showSettingsAsync(player).launch(ll::thread::ServerThreadExecutor::getDefault());
}
```

## MessageBox

Two-button dialog whose result reports **which** button: `MessageBoxResult{closeReason, optional<uint32> selection}` — `1` = button1, `2` = button2, empty = closed without selection (busy player, programmatic close). One-based numbering.

```cpp
ll::ui::MessageBox box{player, "Reset settings"};
box.body("Reset all saved settings?")
   .button1("Yes", std::string{"This cannot be undone"})
   .button2("No");

box.show([](ll::ui::MessageBox::Result result) {
    if (!result || !result->selection) return;
    if (*result->selection == 1) { /* reset */ }
});
```

## Text values & closing

- `TextValue = std::variant<std::string, UIRawMessage, ObservableString, ObservableUIRawMessage>`. `UIRawMessage::text/translate(key)/translate(key, substitutions)/rawText(...)` — translation happens client-side in the player's language.
- `form.close()` closes just this form (`Expected<>`; only while showing). `ll::ui::closeScreen(player)` (from `ScreenSession.h`) closes **all** DDUI screens for that player; sessions complete with `ScreenCloseReason::ProgrammaticCloseAll`.
- Close reasons (`ScreenCloseReason` = Bedrock's `DataDrivenScreenClosedReason`): `ProgrammaticClose(0)`, `ProgrammaticCloseAll(1)`, `ClientCanceled(2)`, `UserBusy(3)`, `InvalidForm(4)`.

## Low-level ScreenSession (custom resource-pack screens)

`ScreenSession{player, "example:runtime_status"}` + `createProperty(namespace, name, cereal::DynamicValue)` + `property->bind<T>("path")` / `bindClientWritable<T>` + `session.show()`. **Keep the session alive** — Property/Binding handles die with it. `Binding::listen()` returns a move-only **RAII** `Subscription` (unlike Observable's id-based model). Only use when the resource pack defines the screen and its data contract; prefer `cereal::DynamicValue` over the JSON-string overload.

## Common mistakes table (from the guide)

| Symptom | Cause / fix |
|---|---|
| Input control construction throws | Its observable was read-only; construct with `{.clientWritable = true}` |
| `show()` succeeded but no close result | `show()` reports startup only — use the callback or `showAsync()` |
| Subscription stops immediately | `Binding::listen()` RAII `Subscription` was discarded; retain it |
| Observable callback never stops | Observable ids are explicit; call `unsubscribe(id)` |
| Structural method throws after show | Structure locks on first show attempt; create a new form |
| UI doesn't update after changing a plain variable | Bind an Observable and call `setData()` |
| `close()` reports not showing | Call only after successful startup and before completion |
| NaN or infinity rejected | DDUI numbers must be finite |
| Only one screen should close | Use the form's `close()`, not `closeScreen(player)` |
