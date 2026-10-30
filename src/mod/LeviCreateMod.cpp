#include "mod/LeviCreateMod.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <string_view>

#include <pl/ModMenu.hpp>

#include "mod/ui/HudPresenter.h"

namespace levicreate {

namespace {

constexpr const char *kModuleId = "levi-create.studio";

int parseInt(std::string_view value, int fallback) {
    std::string text(value);
    char *end = nullptr;
    errno = 0;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' || errno == ERANGE) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

double parseDouble(std::string_view value, double fallback) {
    std::string text(value);
    char *end = nullptr;
    errno = 0;
    const double parsed = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0' || errno == ERANGE) {
        return fallback;
    }
    return parsed;
}

bool parseBool(std::string_view value, bool fallback) {
    if (value == "true" || value == "1" || value == "on" ||
        value == "enabled") {
        return true;
    }
    if (value == "false" || value == "0" || value == "off" ||
        value == "disabled") {
        return false;
    }
    return fallback;
}

bool isValidColor(std::string_view value) {
    if ((value.size() != 7 && value.size() != 9) || value.front() != '#') {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
    });
}

std::string boolToMenuValue(bool value) { return value ? "true" : "false"; }

std::string doubleToMenuValue(double value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

} // namespace

LeviCreateMod &LeviCreateMod::instance() {
    static LeviCreateMod mod;
    return mod;
}

LeviCreateMod::LeviCreateMod() : mSelf(*ll::mod::NativeMod::current()) {}

bool LeviCreateMod::load() {
    auto &self = getSelf();

    std::lock_guard lock(mConfigMutex);
    mConfigFile.emplace();
    if (!mConfigFile->load()) {
        self.getLogger().error("Failed to load config");
        mConfigFile.reset();
        return false;
    }

    auto &config = mConfigFile->value();
    config.gridSize = std::clamp(config.gridSize, kMinGridSize, kMaxGridSize);
    config.materialIndex =
        std::clamp(config.materialIndex, kMinMaterial, kMaxMaterial);
    config.rotateAngle =
        std::clamp(config.rotateAngle, kMinRotateAngle, kMaxRotateAngle);
    config.saveSlot = std::clamp(config.saveSlot, kMinSaveSlot, kMaxSaveSlot);
    config.maxDrawCommands = std::clamp(config.maxDrawCommands,
                                        kMinDrawCommands, kMaxDrawCommands);
    config.panelScale =
        std::clamp(config.panelScale, kMinPanelScale, kMaxPanelScale);
    config.previewScale =
        std::clamp(config.previewScale, kMinPreviewScale, kMaxPreviewScale);
    if (!isValidColor(config.accentColor)) {
        config.accentColor = "#3FD08C";
    }

    if (!mConfigFile->save()) {
        self.getLogger().warn("Failed to persist normalized config");
    }

    std::error_code ec;
    std::filesystem::create_directories(self.getDataDir(), ec);
    std::filesystem::create_directories(self.getConfigDir(), ec);

    VoxelStudio::instance().begin(self.getLogger(), self.getDataDir(), config);

    self.getLogger().info("LeviCreate Voxel Studio loaded - grid {}, mod dir {}",
                          config.gridSize, self.getModDir().string());
    return true;
}

bool LeviCreateMod::registerStudioModule() {
    const auto config = [&] {
        std::lock_guard lock(mConfigMutex);
        return mConfigFile ? mConfigFile->value() : ModConfig{};
    }();

    return pl::modmenu::ModuleBuilder(kModuleId, "LeviCreate - Voxel Studio")
        .modId(getSelf().getId())
        .description(
            "Sub-block voxel studio (Chisel & Bits x Axiom style): 1/16 bits, "
            "any-degree rotate, scale, move, copy/paste, save slots.")
        .defaultEnabled(config.showPanel)
        .onToggle(&LeviCreateMod::onModuleToggle)
        .onConfigChanged(&LeviCreateMod::onConfigChanged)
        .config("showPanel", "Show Studio Panel",
                pl::modmenu::ConfigType::Toggle,
                boolToMenuValue(config.showPanel))
        .config("captureCanvas", "Canvas Touch Capture",
                pl::modmenu::ConfigType::Toggle,
                boolToMenuValue(config.captureCanvas))
        .config("showStats", "Show Status Lines",
                pl::modmenu::ConfigType::Toggle,
                boolToMenuValue(config.showStats))
        .config("panelScale", "Panel Scale",
                pl::modmenu::ConfigType::SliderFloat,
                doubleToMenuValue(config.panelScale),
                doubleToMenuValue(kMinPanelScale),
                doubleToMenuValue(kMaxPanelScale))
        .config("previewScale", "Preview Zoom",
                pl::modmenu::ConfigType::SliderFloat,
                doubleToMenuValue(config.previewScale),
                doubleToMenuValue(kMinPreviewScale),
                doubleToMenuValue(kMaxPreviewScale))
        .config("gridSize", "Grid Size (bits per block)",
                pl::modmenu::ConfigType::SliderInt,
                std::to_string(config.gridSize),
                std::to_string(kMinGridSize), std::to_string(kMaxGridSize))
        .config("toolIndex", "Tool", pl::modmenu::ConfigType::Radio,
                std::to_string(config.toolIndex),
                std::string(kToolMenuOptions))
        .config("materialIndex", "Material", pl::modmenu::ConfigType::SliderInt,
                std::to_string(config.materialIndex),
                std::to_string(kMinMaterial), std::to_string(kMaxMaterial))
        .config("rotateAngle", "Rotate Angle (deg)",
                pl::modmenu::ConfigType::SliderInt,
                std::to_string(config.rotateAngle),
                std::to_string(kMinRotateAngle),
                std::to_string(kMaxRotateAngle))
        .config("saveSlot", "Save Slot", pl::modmenu::ConfigType::SliderInt,
                std::to_string(config.saveSlot),
                std::to_string(kMinSaveSlot), std::to_string(kMaxSaveSlot))
        .config("maxDrawCommands", "Max Draw Commands",
                pl::modmenu::ConfigType::SliderInt,
                std::to_string(config.maxDrawCommands),
                std::to_string(kMinDrawCommands),
                std::to_string(kMaxDrawCommands))
        .config("accentColor", "Accent Color", pl::modmenu::ConfigType::Color,
                config.accentColor)
        .registerModule();
}

bool LeviCreateMod::enable() {
    auto &self = getSelf();

    mRegistered = registerStudioModule();
    const bool buttons = mRegistered && mButtons.registerAll(self.getId());
    if (mRegistered && buttons) {
        mCanvas.enable();
        mGameBridge.start(self.getLogger(), self.getConfigDir());
        refreshStudioHud();
        self.getLogger().info(
            "LeviCreate enabled: studio module, toolbar buttons and canvas "
            "input registered");
    } else {
        self.getLogger().error(
            "Failed to register LeviCreate UI: module={} buttons={}",
            mRegistered, buttons);
    }
    return mRegistered && buttons;
}

bool LeviCreateMod::disable() {
    auto &self = getSelf();

    mCanvas.disable();
    mButtons.unregisterAll();
    mGameBridge.stop();

    // Clear any overlay we own.
    std::vector<pl::modmenu::DrawCommand> empty;
    pl::modmenu::submitDrawCommands(kModuleId, empty);
    pl::modmenu::unregisterModule(kModuleId);
    mRegistered = false;

    {
        std::lock_guard lock(mConfigMutex);
        if (mConfigFile) {
            mConfigFile->save();
        }
    }

    self.getLogger().info("LeviCreate disabled");
    return true;
}

bool LeviCreateMod::unload() {
    std::lock_guard lock(mConfigMutex);
    mConfigFile.reset();
    getSelf().getLogger().info("LeviCreate unloaded");
    return true;
}

void LeviCreateMod::onModuleToggle(std::string_view moduleId, bool enabled) {
    if (moduleId != kModuleId) {
        return;
    }
    auto &mod = instance();
    {
        std::lock_guard lock(mod.mConfigMutex);
        if (mod.mConfigFile) {
            mod.mConfigFile->value().showPanel = enabled;
            mod.mConfigFile->save();
        }
    }
    VoxelStudio::instance().applyConfig(
        [&] {
            std::lock_guard lock(mod.mConfigMutex);
            return mod.mConfigFile ? mod.mConfigFile->value() : ModConfig{};
        }());
    refreshStudioHud();
    mod.getSelf().getLogger().info("Studio panel {}",
                                   enabled ? "shown" : "hidden");
}

void LeviCreateMod::onConfigChanged(std::string_view moduleId,
                                    std::string_view key,
                                    std::string_view value) {
    if (moduleId != kModuleId || key.empty()) {
        return;
    }
    instance().handleConfigChanged(key, value);
}

void LeviCreateMod::handleConfigChanged(std::string_view key,
                                        std::string_view value) {
    {
        std::lock_guard lock(mConfigMutex);
        if (!mConfigFile) {
            return;
        }
        auto &config = mConfigFile->value();

        if (key == "showPanel") {
            config.showPanel = parseBool(value, config.showPanel);
        } else if (key == "captureCanvas") {
            config.captureCanvas = parseBool(value, config.captureCanvas);
        } else if (key == "showStats") {
            config.showStats = parseBool(value, config.showStats);
        } else if (key == "panelScale") {
            config.panelScale = std::clamp(
                parseDouble(value, config.panelScale), kMinPanelScale,
                kMaxPanelScale);
        } else if (key == "previewScale") {
            config.previewScale = std::clamp(
                parseDouble(value, config.previewScale), kMinPreviewScale,
                kMaxPreviewScale);
        } else if (key == "gridSize") {
            config.gridSize = std::clamp(parseInt(value, config.gridSize),
                                         kMinGridSize, kMaxGridSize);
        } else if (key == "toolIndex") {
            config.toolIndex = std::clamp(
                parseInt(value, config.toolIndex), 0,
                static_cast<int>(Tool::Move));
        } else if (key == "materialIndex") {
            config.materialIndex =
                std::clamp(parseInt(value, config.materialIndex), kMinMaterial,
                           kMaxMaterial);
        } else if (key == "rotateAngle") {
            config.rotateAngle = std::clamp(
                parseInt(value, config.rotateAngle), kMinRotateAngle,
                kMaxRotateAngle);
        } else if (key == "saveSlot") {
            config.saveSlot = std::clamp(parseInt(value, config.saveSlot),
                                         kMinSaveSlot, kMaxSaveSlot);
        } else if (key == "maxDrawCommands") {
            config.maxDrawCommands = std::clamp(
                parseInt(value, config.maxDrawCommands), kMinDrawCommands,
                kMaxDrawCommands);
        } else if (key == "accentColor") {
            if (isValidColor(value)) {
                config.accentColor = std::string(value);
            }
        } else {
            return;
        }

        mConfigFile->save();
    }

    // Push to the studio and refresh the overlay.
    {
        std::lock_guard lock(mConfigMutex);
        if (mConfigFile) {
            VoxelStudio::instance().applyConfig(mConfigFile->value());
        }
    }
    refreshStudioHud();
    getSelf().getLogger().info("Config updated: {}", key);
}

} // namespace levicreate
