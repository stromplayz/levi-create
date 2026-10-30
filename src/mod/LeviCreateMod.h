#pragma once

#include <mutex>
#include <optional>
#include <string>

#include <pl/Mod.hpp>

#include "mod/Config.h"
#include "mod/VoxelStudio.h"
#include "mod/game/GameBridge.h"
#include "mod/ui/ButtonSet.h"
#include "mod/ui/CanvasController.h"

namespace levicreate {

class LeviCreateMod {
public:
    static LeviCreateMod &instance();

    LeviCreateMod();

    [[nodiscard]] ll::mod::NativeMod &getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();
    bool unload();

private:
    ll::mod::NativeMod &mSelf;
    std::mutex mConfigMutex;
    std::optional<pl::config::ConfigFile<ModConfig>> mConfigFile;
    ButtonSet mButtons;
    CanvasController mCanvas;
    game::GameBridge mGameBridge;
    bool mRegistered = false;

    // Mod Menu callbacks (static trampolines).
    static void onModuleToggle(std::string_view moduleId, bool enabled);
    static void onConfigChanged(std::string_view moduleId,
                                std::string_view key, std::string_view value);

    void handleConfigChanged(std::string_view key, std::string_view value);
    [[nodiscard]] bool registerStudioModule();
};

} // namespace levicreate
