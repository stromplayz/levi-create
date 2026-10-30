#include "mod/voxel/VoxelStructure.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <nlohmann/json.hpp>

namespace levicreate::voxel {

VoxelStructure::VoxelStructure(int size) { resize(size); }

void VoxelStructure::resize(int newSize) {
    newSize = std::clamp(newSize, 2, 128);

    // Old contents are only valid when the buffer actually matches mSize
    // (a freshly constructed object has an empty buffer).
    const bool hasData = mVoxels.size() ==
        static_cast<std::size_t>(mSize) * mSize * mSize;
    if (newSize == mSize && hasData) {
        return;
    }

    std::vector<std::uint16_t> next(
        static_cast<std::size_t>(newSize) * newSize * newSize, 0);
    if (hasData) {
        const int common = std::min(mSize, newSize);
        for (int y = 0; y < common; ++y) {
            for (int z = 0; z < common; ++z) {
                for (int x = 0; x < common; ++x) {
                    next[(static_cast<std::size_t>(y) * newSize + z) *
                             newSize +
                         x] = mVoxels[idx(x, y, z)];
                }
            }
        }
    }

    mSize = newSize;
    mVoxels = std::move(next);
}

void VoxelStructure::clearAll(std::uint16_t value) {
    std::fill(mVoxels.begin(), mVoxels.end(), value);
}

bool VoxelStructure::inBounds(int x, int y, int z) const {
    return x >= 0 && y >= 0 && z >= 0 && x < mSize && y < mSize && z < mSize;
}

std::uint16_t VoxelStructure::get(int x, int y, int z) const {
    if (!inBounds(x, y, z)) {
        return 0;
    }
    return mVoxels[idx(x, y, z)];
}

void VoxelStructure::set(int x, int y, int z, std::uint16_t value) {
    if (!inBounds(x, y, z)) {
        return;
    }
    mVoxels[idx(x, y, z)] = value;
}

int VoxelStructure::countNonAir() const {
    int count = 0;
    for (const auto voxel : mVoxels) {
        if (voxel != 0) {
            ++count;
        }
    }
    return count;
}

void VoxelStructure::fillBox(int x0, int y0, int z0, int x1, int y1, int z1,
                             std::uint16_t value) {
    const int lx0 = std::max(0, std::min(x0, x1));
    const int lx1 = std::min(mSize - 1, std::max(x0, x1));
    const int ly0 = std::max(0, std::min(y0, y1));
    const int ly1 = std::min(mSize - 1, std::max(y0, y1));
    const int lz0 = std::max(0, std::min(z0, z1));
    const int lz1 = std::min(mSize - 1, std::max(z0, z1));

    for (int y = ly0; y <= ly1; ++y) {
        for (int z = lz0; z <= lz1; ++z) {
            for (int x = lx0; x <= lx1; ++x) {
                mVoxels[idx(x, y, z)] = value;
            }
        }
    }
}

void VoxelStructure::fillSphere(float cx, float cy, float cz, float radius,
                                std::uint16_t value) {
    if (radius <= 0.0f) {
        return;
    }
    const float r2 = radius * radius;
    const int minX = std::max(0, static_cast<int>(std::floor(cx - radius)));
    const int maxX =
        std::min(mSize - 1, static_cast<int>(std::ceil(cx + radius)));
    const int minY = std::max(0, static_cast<int>(std::floor(cy - radius)));
    const int maxY =
        std::min(mSize - 1, static_cast<int>(std::ceil(cy + radius)));
    const int minZ = std::max(0, static_cast<int>(std::floor(cz - radius)));
    const int maxZ =
        std::min(mSize - 1, static_cast<int>(std::ceil(cz + radius)));

    for (int y = minY; y <= maxY; ++y) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                const float dx = x + 0.5f - cx;
                const float dy = y + 0.5f - cy;
                const float dz = z + 0.5f - cz;
                if (dx * dx + dy * dy + dz * dz <= r2) {
                    mVoxels[idx(x, y, z)] = value;
                }
            }
        }
    }
}

bool VoxelStructure::isSolid(int x, int y, int z) const {
    return get(x, y, z) != 0;
}

bool VoxelStructure::faceExposed(int x, int y, int z) const {
    static const int kOffsets[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
    };
    if (!isSolid(x, y, z)) {
        return false;
    }
    for (const auto &offset : kOffsets) {
        if (!inBounds(x + offset[0], y + offset[1], z + offset[2])) {
            return true; // grid border counts as exposed
        }
        if (get(x + offset[0], y + offset[1], z + offset[2]) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<int> VoxelStructure::collectSurfaceVoxels() const {
    std::vector<int> surface;
    const int n = mSize;
    for (int y = 0; y < n; ++y) {
        for (int z = 0; z < n; ++z) {
            for (int x = 0; x < n; ++x) {
                if (faceExposed(x, y, z)) {
                    surface.push_back(idx(x, y, z));
                }
            }
        }
    }
    return surface;
}

std::string VoxelStructure::toJson() const {
    nlohmann::json runs = nlohmann::json::array();
    std::uint16_t current = mVoxels.empty() ? 0 : mVoxels.front();
    std::size_t runLength = 0;
    for (const auto voxel : mVoxels) {
        if (voxel == current) {
            ++runLength;
        } else {
            runs.push_back(nlohmann::json::array({current, runLength}));
            current = voxel;
            runLength = 1;
        }
    }
    if (runLength > 0) {
        runs.push_back(nlohmann::json::array({current, runLength}));
    }

    nlohmann::json root;
    root["format"] = "levi-create-structure";
    root["version"] = 1;
    root["size"] = mSize;
    root["voxels"] = std::move(runs);
    return root.dump();
}

bool VoxelStructure::fromJson(const std::string &json) {
    try {
        const auto root = nlohmann::json::parse(json);
        if (!root.is_object() || !root.contains("size") ||
            !root.contains("voxels")) {
            return false;
        }

        const int size = std::clamp(root["size"].get<int>(), 2, 128);
        mSize = size;
        mVoxels.assign(static_cast<std::size_t>(size) * size * size, 0);

        std::size_t cursor = 0;
        for (const auto &run : root["voxels"]) {
            if (!run.is_array() || run.size() != 2) {
                continue;
            }
            const auto value = static_cast<std::uint16_t>(run[0].get<int>());
            const auto length = run[1].get<std::size_t>();
            for (std::size_t i = 0; i < length && cursor < mVoxels.size();
                 ++i) {
                mVoxels[cursor++] = value;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace levicreate::voxel
