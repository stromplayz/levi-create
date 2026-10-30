#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace lc {

// 0 means air, any other value means palette[index - 1]
using VoxelIndex = std::uint16_t;

struct int3 {
    int x{};
    int y{};
    int z{};
};

inline int3 int3Min(int3 const& a, int3 const& b) {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}
inline int3 int3Max(int3 const& a, int3 const& b) {
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

/// A 3D voxel model built from a palette of block names.
/// Layout: index = (y * sz + z) * sx + x
struct VoxelModel {
    int                      sx = 0;
    int                      sy = 0;
    int                      sz = 0;
    std::vector<std::string> palette;
    std::vector<VoxelIndex>  vox;

    std::uint64_t volume() const {
        if (sx <= 0 || sy <= 0 || sz <= 0) return 0;
        return static_cast<std::uint64_t>(sx) * static_cast<std::uint64_t>(sy) * static_cast<std::uint64_t>(sz);
    }
    bool empty() const { return volume() == 0; }

    std::uint64_t indexOf(int x, int y, int z) const {
        return (static_cast<std::uint64_t>(y) * static_cast<std::uint64_t>(sz) + static_cast<std::uint64_t>(z))
                   * static_cast<std::uint64_t>(sx)
             + static_cast<std::uint64_t>(x);
    }
    bool inside(int x, int y, int z) const {
        return x >= 0 && y >= 0 && z >= 0 && x < sx && y < sy && z < sz;
    }
    VoxelIndex get(int x, int y, int z) const { return inside(x, y, z) ? vox[indexOf(x, y, z)] : VoxelIndex{0}; }
    void       set(int x, int y, int z, VoxelIndex v) {
        if (inside(x, y, z)) vox[indexOf(x, y, z)] = v;
    }
    void resize(int nx, int ny, int nz) {
        sx = nx;
        sy = ny;
        sz = nz;
        vox.assign(volume(), VoxelIndex{0});
    }

    std::uint64_t countNonAir() const {
        std::uint64_t n = 0;
        for (auto v : vox)
            if (v != 0) ++n;
        return n;
    }

    int addOrGetPalette(std::string const& name) {
        for (std::size_t i = 0; i < palette.size(); ++i) {
            if (palette[i] == name) return static_cast<int>(i) + 1;
        }
        palette.push_back(name);
        return static_cast<int>(palette.size());
    }

    std::string blockName(VoxelIndex i) const {
        if (i == 0 || i > palette.size()) return {};
        return palette[i - 1];
    }

    std::string toJson() const;
    static std::optional<VoxelModel> fromJson(std::string const& text);
};

struct ModelTransform {
    float yawDegrees   = 0.0f; // rotation around Y axis (any angle supported)
    float pitchDegrees = 0.0f; // rotation around X axis
    float rollDegrees  = 0.0f; // rotation around Z axis
    float scale        = 1.0f;
};

/// Scale by an arbitrary factor (0.0625 .. 16). Nearest-neighbor sampling.
VoxelModel scaleModel(VoxelModel const& src, float factor);

/// Rotate around axis (0 = X, 1 = Y, 2 = Z). Any degree is supported:
/// multiples of 90 degrees are exact voxel rotations, other angles are
/// rasterized by inverse-mapping (nearest voxel sampling).
VoxelModel rotateModel(VoxelModel const& src, int axis, double degrees);

/// Apply scale then yaw, pitch, roll.
VoxelModel transformModel(VoxelModel const& src, ModelTransform const& t);

/// Mirror along axis (0 = X, 1 = Y, 2 = Z).
VoxelModel flipModel(VoxelModel const& src, int axis);

std::string formatVolume(std::uint64_t blocks);

} // namespace lc
