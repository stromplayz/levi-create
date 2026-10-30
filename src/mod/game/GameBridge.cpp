#include "mod/game/GameBridge.h"

#include <fstream>
#include <vector>

#include <pl/Logger.hpp>
#include <pl/memory/Signature.hpp>

#include <nlohmann/json.hpp>

namespace levicreate::game {

void GameBridge::start(const pl::log::Logger &logger,
                       const std::filesystem::path &configDir) {
    namespace fs = std::filesystem;

    const auto bridgeConfigPath = configDir / "game_bridge.json";

    // Write a documented default config the first time so users only have to
    // flip "enabled" and drop in signatures.
    if (!fs::exists(bridgeConfigPath)) {
        std::error_code ec;
        fs::create_directories(configDir, ec);
        nlohmann::json defaults = {
            {"enabled", false},
            {"module", "libminecraftpe.so"},
            {"signatures", nlohmann::json::object()},
        };
        std::ofstream stream(bridgeConfigPath, std::ios::binary | std::ios::trunc);
        if (stream) {
            stream << defaults.dump(2) << '\n';
        }
    }

    try {
        std::ifstream stream(bridgeConfigPath, std::ios::binary);
        if (!stream) {
            return;
        }
        nlohmann::json parsed;
        stream >> parsed;

        if (parsed.contains("module") && parsed["module"].is_string()) {
            mModuleName = parsed["module"].get<std::string>();
        }
        mEnabled = parsed.contains("enabled") &&
                   parsed["enabled"].is_boolean() &&
                   parsed["enabled"].get<bool>();
    } catch (const std::exception &ex) {
        logger.warn("GameBridge: failed to parse {}: {}", bridgeConfigPath.string(),
                    ex.what());
        return;
    }

    if (!mEnabled) {
        logger.info(
            "GameBridge: disabled (default). World application requires "
            "per-version signatures in config/game_bridge.json - see README.");
        return;
    }

    try {
        std::ifstream stream(bridgeConfigPath, std::ios::binary);
        nlohmann::json parsed;
        stream >> parsed;

        std::vector<std::string> names;
        if (parsed.contains("signatures") && parsed["signatures"].is_object()) {
            for (auto it = parsed["signatures"].begin();
                 it != parsed["signatures"].end(); ++it) {
                if (it.value().is_string()) {
                    names.push_back(it.key());
                }
            }
        }

        if (names.empty()) {
            logger.warn("GameBridge: enabled but no signatures configured - "
                        "nothing to resolve.");
            mEnabled = false;
            return;
        }

        const auto results = pl::memory::resolveSignatures(names, mModuleName);
        for (const auto &name : names) {
            const auto found = results.find(name);
            if (found != results.end() && found->second != 0) {
                mResolved[name] = found->second;
                logger.info("GameBridge: resolved {} @ 0x{:x} in {}", name,
                            found->second, mModuleName);
            } else {
                logger.warn("GameBridge: signature '{}' not found in {}", name,
                            mModuleName);
            }
        }

        // NOTE: actually *applying* a voxel structure into the live world
        // requires calling game functions at these addresses. The calling
        // convention and semantics change between Minecraft versions, so
        // LeviCreate only resolves and reports here. Bind the calls in
        // levi_create_apply.cpp once the signatures for your game version
        // are known (see README "World application (advanced)").
    } catch (const std::exception &ex) {
        logger.error("GameBridge: {}", ex.what());
        mEnabled = false;
    }
}

void GameBridge::stop() {
    mEnabled = false;
    mResolved.clear();
}

} // namespace levicreate::game
