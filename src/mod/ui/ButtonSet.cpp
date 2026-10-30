#include "mod/ui/ButtonSet.h"

#include <algorithm>

#include "mod/Config.h"
#include "mod/VoxelStudio.h"
#include "mod/ui/HudPresenter.h"

namespace levicreate {

namespace {

constexpr const char *kModuleId = "levi-create.studio";
constexpr const char *kButtonPrefix = "levi-create.btn_";

// ---- 64x64 SVG icons (flat, white shapes on transparent background) ----

constexpr const char *kSvgCube =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
<rect x="6" y="6" width="52" height="52" rx="10" fill="#1C232B"/>
<path d="M32 10 50 20v24L32 54 14 44V20z" fill="none" stroke="#FFFFFF" stroke-width="3"/>
<path d="M32 10v22m0 0 18-10M32 32 14 22" fill="none" stroke="#FFFFFF" stroke-width="3"/>
</svg>)svg";

constexpr const char *kSvgBucket =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
<rect x="6" y="6" width="52" height="52" rx="10" fill="#1C232B"/>
<path d="M16 26h26v18a4 4 0 0 1-4 4H20a4 4 0 0 1-4-4z" fill="#FFFFFF"/>
<path d="M42 14c4 5 6 8 6 11a6 6 0 1 1-12 0c0-3 2-6 6-11z" fill="#FFFFFF"/>
</svg>)svg";

constexpr const char *kSvgUndo =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
<rect x="6" y="6" width="52" height="52" rx="10" fill="#1C232B"/>
<path d="M26 18 14 30l12 12" fill="none" stroke="#FFFFFF" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M14 30h22a12 12 0 0 1 0 24h-6" fill="none" stroke="#FFFFFF" stroke-width="4" stroke-linecap="round"/>
</svg>)svg";

constexpr const char *kSvgRotate =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
<rect x="6" y="6" width="52" height="52" rx="10" fill="#1C232B"/>
<path d="M46 26a16 16 0 1 0 2 12" fill="none" stroke="#FFFFFF" stroke-width="4" stroke-linecap="round"/>
<path d="M46 14v14H32z" fill="#FFFFFF"/>
</svg>)svg";

constexpr const char *kSvgTilt =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
<rect x="6" y="6" width="52" height="52" rx="10" fill="#1C232B"/>
<path d="M32 12 48 22v20L32 52 16 42V22z" fill="none" stroke="#FFFFFF" stroke-width="3"/>
<path d="M12 32h40" stroke="#3FD08C" stroke-width="4" stroke-linecap="round"/>
</svg>)svg";

constexpr const char *kSvgMirror =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
<rect x="6" y="6" width="52" height="52" rx="10" fill="#1C232B"/>
<path d="M32 8v48" stroke="#3FD08C" stroke-width="4" stroke-linecap="round" stroke-dasharray="6 5"/>
<path d="M24 20v24L12 32z" fill="#FFFFFF"/>
<path d="M40 20v24l12-12z" fill="#FFFFFF"/>
</svg>)svg";

constexpr const char *kSvgScaleUp =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
<rect x="6" y="6" width="52" height="52" rx="10" fill="#1C232B"/>
<path d="M14 50 36 28" stroke="#FFFFFF" stroke-width="4" stroke-linecap="round"/>
<path d="M20 50h16V34z" fill="#FFFFFF"/>
<path d="M44 14v18m-9-9h18" stroke="#FFFFFF" stroke-width="5" stroke-linecap="round"/>
</svg>)svg";

constexpr const char *kSvgScaleDown =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
<rect x="6" y="6" width="52" height="52" rx="10" fill="#1C232B"/>
<path d="M14 50 36 28" stroke="#FFFFFF" stroke-width="4" stroke-linecap="round"/>
<path d="M20 50h16V34z" fill="#FFFFFF"/>
<path d="M35 23h18" stroke="#FFFFFF" stroke-width="5" stroke-linecap="round"/>
</svg>)svg";

constexpr const char *kSvgCopy =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
<rect x="6" y="6" width="52" height="52" rx="10" fill="#1C232B"/>
<rect x="14" y="14" width="26" height="26" rx="4" fill="none" stroke="#FFFFFF" stroke-width="4"/>
<rect x="24" y="24" width="26" height="26" rx="4" fill="#1C232B" stroke="#FFFFFF" stroke-width="4"/>
</svg>)svg";

constexpr const char *kSvgPaste =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
<rect x="6" y="6" width="52" height="52" rx="10" fill="#1C232B"/>
<rect x="16" y="12" width="32" height="42" rx="5" fill="none" stroke="#FFFFFF" stroke-width="4"/>
<rect x="25" y="7" width="14" height="10" rx="3" fill="#FFFFFF"/>
<path d="M24 30h16M24 40h16" stroke="#FFFFFF" stroke-width="4" stroke-linecap="round"/>
</svg>)svg";

constexpr const char *kSvgSave =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
<rect x="6" y="6" width="52" height="52" rx="10" fill="#1C232B"/>
<path d="M16 14h24l8 8v28H16z" fill="none" stroke="#FFFFFF" stroke-width="4" stroke-linejoin="round"/>
<rect x="24" y="14" width="14" height="12" fill="#FFFFFF"/>
<rect x="22" y="36" width="20" height="14" fill="#FFFFFF"/>
</svg>)svg";

constexpr const char *kSvgLoad =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
<rect x="6" y="6" width="52" height="52" rx="10" fill="#1C232B"/>
<path d="M12 20h14l4 6h22v22a4 4 0 0 1-4 4H16a4 4 0 0 1-4-4z" fill="none" stroke="#FFFFFF" stroke-width="4" stroke-linejoin="round"/>
</svg>)svg";

constexpr const char *kSvgTrash =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
<rect x="6" y="6" width="52" height="52" rx="10" fill="#1C232B"/>
<path d="M16 20h32" stroke="#FFFFFF" stroke-width="4" stroke-linecap="round"/>
<path d="M22 20v28h20V20" fill="none" stroke="#FFFFFF" stroke-width="4" stroke-linejoin="round"/>
<path d="M27 14h10v6H27z" fill="#FFFFFF"/>
</svg>)svg";

void dispatch(std::string_view id) {
    auto &studio = VoxelStudio::instance();
    const auto config = studio.config();

    if (id == "mode") {
        studio.cycleTool();
    } else if (id == "material") {
        studio.cycleMaterial();
    } else if (id == "undo") {
        studio.undo();
    } else if (id == "rotate") {
        studio.rotateY(config.rotateAngle);
    } else if (id == "tilt") {
        studio.rotateX90();
    } else if (id == "mirror") {
        studio.mirror();
    } else if (id == "scale_up") {
        studio.scaleStep(1);
    } else if (id == "scale_down") {
        studio.scaleStep(-1);
    } else if (id == "copy") {
        studio.copyAll();
    } else if (id == "paste") {
        studio.pasteAll();
    } else if (id == "save") {
        studio.saveToSlot(config.saveSlot);
    } else if (id == "load") {
        studio.loadFromSlot(config.saveSlot);
    } else if (id == "clear") {
        studio.clearGrid();
    } else {
        return;
    }
    refreshStudioHud();
}

} // namespace

bool ButtonSet::registerAll(const std::string &modId) {
    static const ButtonDef kButtons[] = {
        {"mode", "LeviCreate: Cycle Tool", "Tool", kSvgCube, nullptr},
        {"material", "LeviCreate: Cycle Material", "Mat", kSvgBucket,
         nullptr},
        {"undo", "LeviCreate: Undo", "Undo", kSvgUndo, nullptr},
        {"rotate", "LeviCreate: Rotate", "Rot", kSvgRotate, nullptr},
        {"tilt", "LeviCreate: Tilt 90", "Tilt", kSvgTilt, nullptr},
        {"mirror", "LeviCreate: Mirror", "Mir", kSvgMirror, nullptr},
        {"scale_up", "LeviCreate: Scale Up", "S+", kSvgScaleUp, nullptr},
        {"scale_down", "LeviCreate: Scale Down", "S-", kSvgScaleDown,
         nullptr},
        {"copy", "LeviCreate: Copy", "Copy", kSvgCopy, nullptr},
        {"paste", "LeviCreate: Paste", "Paste", kSvgPaste, nullptr},
        {"save", "LeviCreate: Save Slot", "Save", kSvgSave, nullptr},
        {"load", "LeviCreate: Load Slot", "Load", kSvgLoad, nullptr},
        {"clear", "LeviCreate: Clear Grid", "Clear", kSvgTrash, nullptr},
    };

    bool all = true;
    for (const auto &def : kButtons) {
        const std::string buttonId = kButtonPrefix + std::string(def.id);
        const bool registered =
            pl::modmenu::ButtonBuilder(buttonId, def.displayName)
                .moduleId(kModuleId)
                .modId(modId)
                .label(def.label)
                .behavior(pl::modmenu::ButtonBehavior::Click)
                .stylePreset(pl::modmenu::ButtonStylePreset::Keycap)
                .styleColors(0xF01C232Bu, 0xFF3FD08Cu, 0x803FD08Cu)
                .textColor(0xFFFFFFFFu)
                .activeTextColor(0xFF101418u)
                .sizeScale(0.75f, 0.75f)
                .svgIcon(def.svg)
                .onEvent(&ButtonSet::onButtonEvent)
                .registerButton();
        if (registered) {
            mRegisteredIds.push_back(buttonId);
        } else {
            all = false;
        }
    }
    return all;
}

void ButtonSet::unregisterAll() {
    for (const auto &id : mRegisteredIds) {
        pl::modmenu::unregisterButton(id);
    }
    mRegisteredIds.clear();
}

void ButtonSet::onButtonEvent(std::string_view buttonId,
                              pl::modmenu::ButtonEvent event, float value) {
    (void)value;
    if (event != pl::modmenu::ButtonEvent::Click) {
        return;
    }
    constexpr std::string_view prefix = kButtonPrefix;
    if (buttonId.size() > prefix.size() &&
        buttonId.substr(0, prefix.size()) == prefix) {
        dispatch(buttonId.substr(prefix.size()));
    }
}

} // namespace levicreate
