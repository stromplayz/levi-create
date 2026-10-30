# 05 — Forms (legacy `ll::form` API)

Sources: https://docs.levimc.org/LeviLamina/developer_guides/how_to_guides/form_guide/ + https://docs.levimc.org/LeviLamina/api_reference/form/ (canonical: `docs/main/contents/developer_guides/how_to_guides/form_guide.md`, `docs/main/contents/api_reference/form.md`).

Headers: `ll/api/form/SimpleForm.h`, `ll/api/form/ModalForm.h`, `ll/api/form/CustomForm.h`, `ll/api/form/FormBase.h`, `ll/api/form/FormIdManager.h`.

> There are two form APIs: the static legacy `ll::form` (this file) and the reactive `ll::ui` DDUI (see `06-data-driven-ui.md`). Both must run on the **server thread**.

## SimpleForm (button list)

`Callback = std::function<void(Player&, int, FormCancelReason)>` — the `int` is the clicked button index; **`-1` = the player closed the form**. `FormCancelReason` is `std::optional<ModalFormCancelReason>` — always check `has_value()` before use (name it via `magic_enum`).

```cpp
ll::form::SimpleForm form("I'm title", "I'm content");
form.appendHeader("header")
    .appendButton("Button1")
    .appendDivider()
    .appendHeader("header")
    .appendLabel("label")
    .appendButton("Button2")
    .sendTo(player, [](Player& player, int selected, ll::form::FormCancelReason reason) {
        switch (selected) {
        case 0: player.sendMessage("You clicked Button1"); break;
        case 1: player.sendMessage("You clicked Button2"); break;
        case -1:
            player.sendMessage("You closed the form");
            player.sendMessage(reason.transform(magic_enum::enum_name<ModalFormCancelReason>).value_or("Unknown Reason"));
            break;
        }
    });
```

- Constructors: `SimpleForm(title, content)` or default + `setTitle/setContent`.
- `appendButton(text, callback)` or with image: `appendButton(text, imageData, imageType, callback)` (e.g. `"textures/ui/settings_glyph_color_2x"`, `"path"`).
- Visual-only elements: `appendHeader`, `appendLabel`, `appendDivider`. All methods chain.
- `sendUpdate(player, callback)` refreshes an open form (no content restrictions; player must still have it open).

## ModalForm (two-button dialog)

`ModalFormResult = std::optional<ModalFormSelectedButton>`; `enum class ModalFormSelectedButton : bool { Upper = true, Lower = false }`. Note `sendTo`/`sendUpdate` return **bool** here.

```cpp
ll::form::ModalForm form;
form.setTitle("ModalForm")
    .setUpperButton("Upper")
    .setLowerButton("Lower")
    .sendTo(player, [](Player& player, ll::form::ModalFormResult selected, ll::form::FormCancelReason reason) {
        if (!selected) { /* closed without selection */ return; }
        player.sendMessage(fmt::format("ModalForm callback {}", (bool)selected));
    });
```

Constructor form: `ModalForm(title, content, upperButton, lowerButton)`.

## CustomForm (multi-field input)

```cpp
using CustomFormElementResult = std::variant<std::monostate, uint64, double, std::string>;
using CustomFormResult        = std::optional<std::unordered_map<std::string, CustomFormElementResult>>;
```

```cpp
ll::form::CustomForm form("Server Settings");
form.appendInput("motd", "Server MOTD", "Enter MOTD...", "A Minecraft Server")
    .appendToggle("pvp", "Enable PvP", true)
    .appendSlider("renderDistance", "Render Distance", 2, 32, 1, 10)
    .appendDropdown("difficulty", "Difficulty", {"Peaceful", "Easy", "Normal", "Hard"}, 2)
    .appendStepSlider("gamemode", "Default Gamemode", {"Survival", "Creative", "Adventure"}, 0)
    .sendTo(player, [](Player& p, ll::form::CustomFormResult const& result, ll::form::FormCancelReason) {
        if (!result) return; // cancelled
        auto& data        = *result;
        auto  motd        = std::get<std::string>(data.at("motd"));
        auto  pvp         = std::get<uint64>(data.at("pvp"));               // 0/1
        auto  renderDist  = std::get<double>(data.at("renderDistance"));
        auto  difficulty  = std::get<uint64>(data.at("difficulty"));        // index
    });
```

- Element builders: `appendInput(name, text, placeholder, default, tooltip)`, `appendToggle(name, text, default, tooltip)`, `appendDropdown(name, text, options, defaultIdx, tooltip)`, `appendSlider(name, text, min, max, step, default, tooltip)`, `appendStepSlider(name, text, steps, defaultIdx, tooltip)`; visual: `appendHeader/appendLabel/appendDivider`; `setSubmitButton(text)`.
- **Result types are fixed**: toggle → `uint64` (0/1); slider → `double`; dropdown/step slider → `uint64` index; input → `std::string`. Dispatch with `std::holds_alternative` + `std::get` when unsure.
- `CustomFormResult` is an optional map keyed by element **name**; `nullopt` = cancelled.

## FormIdManager

`ll::form::FormIdManager::genFormId()` returns a unique runtime form id — for building your own form API on top.

```cpp
uint formId = ll::form::FormIdManager::genFormId();
```

## sendUpdate (all three forms)

`sendUpdate(player, callback)` has the same parameters as `sendTo` and updates a form the player currently has open. Useful pattern (server-thread coroutine refresh loop, from the guide):

```cpp
buildStatusWindow()->sendTo(*player, callback);
while (true) {
    co_await ll::chrono::ticks{20};
    // re-resolve the player; if gone or closed -> co_return
    buildStatusWindow()->sendUpdate(*player, callback);
}
```
