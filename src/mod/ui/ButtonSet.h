#pragma once

#include <string>
#include <vector>

#include <pl/ModMenu.hpp>

namespace levicreate {

// Registers the floating studio toolbar (Mod Menu on-screen buttons) and
// dispatches their events into the VoxelStudio.
class ButtonSet {
public:
    [[nodiscard]] bool registerAll(const std::string &modId);
    void unregisterAll();

private:
    struct ButtonDef {
        const char *id;
        const char *displayName;
        const char *label;
        const char *svg; // 64x64 icon
        void (*action)();
    };

    static void onButtonEvent(std::string_view buttonId,
                              pl::modmenu::ButtonEvent event, float value);

    std::vector<std::string> mRegisteredIds;
};

} // namespace levicreate
