#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace pl::log {
class Logger;
}

namespace levicreate::game {

// Signature-driven extension point toward the Minecraft game binary
// (libminecraftpe.so). The Preloader SDK exposes pl::memory::resolveSignatures
// and pl::memory::hook; applying a voxel structure into the live world
// requires per-version byte signatures, which LiteLDev ships as external
// config (see preloader src/pl/runtime/GameHookRules.cpp for the pattern).
//
// LeviCreate reads config/game_bridge.json:
//
//   {
//     "enabled": false,
//     "module": "libminecraftpe.so",
//     "signatures": { "name": "?? ?? ?? ..." }
//   }
//
// When "enabled" is true the bridge resolves every listed signature at
// startup and reports the results in logcat. With the default (disabled, no
// signatures) it does nothing at runtime, so the mod stays 100% safe on any
// game version.
class GameBridge {
public:
    void start(const pl::log::Logger &logger,
               const std::filesystem::path &configDir);
    void stop();

    [[nodiscard]] bool enabled() const { return mEnabled; }
    [[nodiscard]] const std::map<std::string, std::uintptr_t> &
    resolved() const {
        return mResolved;
    }

private:
    bool mEnabled = false;
    std::string mModuleName = "libminecraftpe.so";
    std::map<std::string, std::uintptr_t> mResolved;
};

} // namespace levicreate::game
