#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace levicreate::voxel {

// ---------------------------------------------------------------------------
// VoxelStructure: a dense N x N x N grid of voxels ("bits").
// value 0 = air; value v > 0 = material palette index (v - 1) + 1.
// Layout: index(x, y, z) = (y * N + z) * N + x  (y up, Minecraft style).
// ---------------------------------------------------------------------------
class VoxelStructure {
public:
    VoxelStructure() = default;
    explicit VoxelStructure(int size);

    [[nodiscard]] int size() const noexcept { return mSize; }

    // Rebuilds the grid, preserving old voxels where both grids overlap.
    void resize(int newSize);

    void clearAll(std::uint16_t value = 0);

    [[nodiscard]] bool inBounds(int x, int y, int z) const;
    [[nodiscard]] std::uint16_t get(int x, int y, int z) const;
    void set(int x, int y, int z, std::uint16_t value);

    [[nodiscard]] std::size_t voxelCount() const { return mVoxels.size(); }
    [[nodiscard]] int countNonAir() const;

    // Geometry helpers -------------------------------------------------------
    void fillBox(int x0, int y0, int z0, int x1, int y1, int z1,
                 std::uint16_t value);
    void fillSphere(float cx, float cy, float cz, float radius,
                    std::uint16_t value);

    [[nodiscard]] bool isSolid(int x, int y, int z) const;
    // True when the neighbor cell is outside the grid or air.
    [[nodiscard]] bool faceExposed(int x, int y, int z) const;
    [[nodiscard]] std::vector<int> collectSurfaceVoxels() const;

    // Serialization ----------------------------------------------------------
    // JSON: {"size": N, "voxels": [[value, runLength], ...]} (RLE).
    [[nodiscard]] std::string toJson() const;
    [[nodiscard]] bool fromJson(const std::string &json);

private:
    int mSize = 16;
    std::vector<std::uint16_t> mVoxels;

    [[nodiscard]] std::size_t idx(int x, int y, int z) const {
        return (static_cast<std::size_t>(y) * mSize + z) * mSize + x;
    }
};

} // namespace levicreate::voxel
