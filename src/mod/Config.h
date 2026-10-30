#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <pl/Config.hpp>

namespace levicreate {

// Tool ordering must stay in sync with kToolMenuOptions ("Build,Erase,...").
enum class Tool {
    Build = 0,
    Erase = 1,
    Paint = 2,
    Box = 3,
    Sphere = 4,
    Move = 5,
};

struct ModConfig {
    int version = 1;
    bool showPanel = true;
    bool captureCanvas = true;
    bool showStats = true;
    double panelScale = 1.0;
    double previewScale = 1.0;
    int gridSize = 16;
    int toolIndex = 0;
    int materialIndex = 3;
    int rotateAngle = 90;
    int saveSlot = 1;
    int maxDrawCommands = 3800;
    int baseScreenWidth = 1080;
    int baseScreenHeight = 2340;
    bool autoDetectScreen = true;
    std::string accentColor = "#3FD08C";
};

inline constexpr int kMinGridSize = 4;
inline constexpr int kMaxGridSize = 48;
inline constexpr int kMinMaterial = 0;
inline constexpr int kMaxMaterial = 17;
inline constexpr int kMinRotateAngle = 5;
inline constexpr int kMaxRotateAngle = 180;
inline constexpr int kMinSaveSlot = 1;
inline constexpr int kMaxSaveSlot = 9;
inline constexpr int kMinDrawCommands = 800;
inline constexpr int kMaxDrawCommands = 4000;
inline constexpr double kMinPanelScale = 0.5;
inline constexpr double kMaxPanelScale = 2.5;
inline constexpr double kMinPreviewScale = 0.5;
inline constexpr double kMaxPreviewScale = 2.0;

inline constexpr std::string_view kToolMenuOptions =
    "Build,Erase,Paint,Box,Sphere,Move";

Tool toolFromIndex(int index, Tool fallback = Tool::Build);
int toolToIndex(Tool tool);
const char *toolLabel(Tool tool);

} // namespace levicreate

namespace pl::config {

template <> struct Schema<levicreate::ModConfig> {
    static constexpr std::string_view title = "LeviCreate - Voxel Studio";
    static constexpr std::string_view description =
        "Configuration for the LeviCreate voxel building studio "
        "(Chisel & Bits + Axiom style sub-block modeling for LeviLauncher "
        "Android).";

    static constexpr FieldSchema field(std::string_view name) {
        if (name == "version") {
            return {"Version", "Config schema version managed by the mod.",
                    std::nullopt, std::nullopt, true};
        }
        if (name == "showPanel") {
            return {"Show Panel",
                    "Show the voxel studio canvas overlay in the HUD.",
                    std::nullopt, std::nullopt, false};
        }
        if (name == "captureCanvas") {
            return {"Capture Canvas Touch",
                    "Consume touches that land inside the studio canvas so "
                    "the game does not receive them.",
                    std::nullopt, std::nullopt, false};
        }
        if (name == "showStats") {
            return {"Show Stats",
                    "Show tool/size/material status lines above the canvas.",
                    std::nullopt, std::nullopt, false};
        }
        if (name == "panelScale") {
            return {"Panel Scale", "Overall HUD canvas scale multiplier.",
                    levicreate::kMinPanelScale, levicreate::kMaxPanelScale,
                    false};
        }
        if (name == "previewScale") {
            return {"Preview Zoom",
                    "Zoom of the isometric voxel preview inside the canvas.",
                    levicreate::kMinPreviewScale, levicreate::kMaxPreviewScale,
                    false};
        }
        if (name == "gridSize") {
            return {"Grid Size",
                    "Voxels per block edge. 16 = classic 1/16 sub-block "
                    "(bits) precision.",
                    levicreate::kMinGridSize, levicreate::kMaxGridSize, false};
        }
        if (name == "toolIndex") {
            return {"Tool",
                    "Active tool: Build, Erase, Paint, Box, Sphere or Move.",
                    std::nullopt, std::nullopt, false};
        }
        if (name == "materialIndex") {
            return {"Material", "Palette index used by Build/Paint/Box/Sphere.",
                    levicreate::kMinMaterial, levicreate::kMaxMaterial, false};
        }
        if (name == "rotateAngle") {
            return {"Rotate Angle",
                    "Degrees applied by the rotate button (any-degree "
                    "rotation around the vertical axis).",
                    levicreate::kMinRotateAngle, levicreate::kMaxRotateAngle,
                    false};
        }
        if (name == "saveSlot") {
            return {"Save Slot", "Structure slot (1-9) used by save/load.",
                    levicreate::kMinSaveSlot, levicreate::kMaxSaveSlot, false};
        }
        if (name == "maxDrawCommands") {
            return {"Max Draw Commands",
                    "Upper bound for overlay draw commands per submit.",
                    levicreate::kMinDrawCommands, levicreate::kMaxDrawCommands,
                    false};
        }
        if (name == "baseScreenWidth") {
            return {"Screen Width",
                    "Design screen width in pixels. 0 pixels of travel: "
                    "auto-detected from touches when autoDetectScreen is on.",
                    std::nullopt, std::nullopt, false};
        }
        if (name == "baseScreenHeight") {
            return {"Screen Height", "Design screen height in pixels.",
                    std::nullopt, std::nullopt, false};
        }
        if (name == "autoDetectScreen") {
            return {"Auto Detect Screen",
                    "Grow the layout bounds automatically using the largest "
                    "touch coordinates seen.",
                    std::nullopt, std::nullopt, false};
        }
        if (name == "accentColor") {
            return {"Accent Color",
                    "Canvas accent color in #RRGGBB or #AARRGGBB form.",
                    std::nullopt, std::nullopt, false};
        }
        return {};
    }
};

} // namespace pl::config
