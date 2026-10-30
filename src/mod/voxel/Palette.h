#pragma once

#include <cstdint>
#include <string_view>

namespace levicreate::voxel {

// Palette entry colors are 0xAARRGGBB, matching the ModMenu color style used
// by ButtonInfo::styleColors in the Preloader SDK.
struct PaletteEntry {
    std::string_view name;
    std::uint32_t color;
};

// 18 Minecraft-inspired materials (index maps to ModConfig::materialIndex).
inline constexpr PaletteEntry kPalette[] = {
    {"Stone", 0xFF9A9A9Au},
    {"Oak Planks", 0xFFB8945Fu},
    {"White Concrete", 0xFFE9ECECu},
    {"Red", 0xFFB02E26u},
    {"Orange", 0xFFF9801Du},
    {"Yellow", 0xFFFED83Du},
    {"Lime", 0xFF80C71Fu},
    {"Green", 0xFF5E7C16u},
    {"Cyan", 0xFF169C9Cu},
    {"Blue", 0xFF3C44AAu},
    {"Purple", 0xFF8932B8u},
    {"Magenta", 0xFFC74EBDu},
    {"Pink", 0xFFF38BAAu},
    {"Brown", 0xFF835432u},
    {"Gray", 0xFF3E4447u},
    {"Black", 0xFF1D1D21u},
    {"Gold", 0xFFFCEE4Bu},
    {"Diamond", 0xFF5DECF5u},
};

inline constexpr int kPaletteSize =
    static_cast<int>(sizeof(kPalette) / sizeof(kPalette[0]));

inline std::uint32_t paletteColor(int index) {
    if (index < 0 || index >= kPaletteSize) {
        return 0xFFFFFFFFu;
    }
    return kPalette[index].color;
}

inline std::string_view paletteName(int index) {
    if (index < 0 || index >= kPaletteSize) {
        return "Unknown";
    }
    return kPalette[index].name;
}

} // namespace levicreate::voxel
