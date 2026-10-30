#include "mod/voxel/VoxelTransform.h"

#include <algorithm>
#include <cmath>

namespace levicreate::voxel::transform {

void rotateYDeg(VoxelStructure &structure, double degrees) {
    const int n = structure.size();
    VoxelStructure next(n);

    // Normalize into [0, 360) so 90/180/270 land on exact integer math after
    // rounding.
    double angle = std::fmod(degrees, 360.0);
    if (angle < 0) {
        angle += 360.0;
    }
    const double rad = angle * 3.14159265358979323846 / 180.0;
    const double cosA = std::cos(rad);
    const double sinA = std::sin(rad);

    const double center = (n - 1) / 2.0;

    for (int y = 0; y < n; ++y) {
        for (int z = 0; z < n; ++z) {
            for (int x = 0; x < n; ++x) {
                // Destination center -> source position (inverse rotation).
                const double dx = x - center;
                const double dz = z - center;
                const double sx = center + dx * cosA + dz * sinA;
                const double sz = center - dx * sinA + dz * cosA;

                const int isx = static_cast<int>(std::lround(sx));
                const int isz = static_cast<int>(std::lround(sz));
                if (sx >= -0.5 && sz >= -0.5 && isx >= 0 && isz >= 0 &&
                    isx < n && isz < n) {
                    next.set(x, y, z, structure.get(isx, y, isz));
                }
            }
        }
    }

    structure = std::move(next);
}

void rotateX90(VoxelStructure &structure) {
    const int n = structure.size();
    VoxelStructure next(n);
    for (int y = 0; y < n; ++y) {
        for (int z = 0; z < n; ++z) {
            for (int x = 0; x < n; ++x) {
                next.set(x, y, z, structure.get(x, n - 1 - z, y));
            }
        }
    }
    structure = std::move(next);
}

void rotateZ90(VoxelStructure &structure) {
    const int n = structure.size();
    VoxelStructure next(n);
    for (int y = 0; y < n; ++y) {
        for (int z = 0; z < n; ++z) {
            for (int x = 0; x < n; ++x) {
                next.set(x, y, z, structure.get(n - 1 - y, x, z));
            }
        }
    }
    structure = std::move(next);
}

void mirrorX(VoxelStructure &structure) {
    const int n = structure.size();
    VoxelStructure next(n);
    for (int y = 0; y < n; ++y) {
        for (int z = 0; z < n; ++z) {
            for (int x = 0; x < n; ++x) {
                next.set(x, y, z, structure.get(n - 1 - x, y, z));
            }
        }
    }
    structure = std::move(next);
}

void mirrorZ(VoxelStructure &structure) {
    const int n = structure.size();
    VoxelStructure next(n);
    for (int y = 0; y < n; ++y) {
        for (int z = 0; z < n; ++z) {
            for (int x = 0; x < n; ++x) {
                next.set(x, y, z, structure.get(x, y, n - 1 - z));
            }
        }
    }
    structure = std::move(next);
}

double scaleBy(VoxelStructure &structure, double factor) {
    if (factor <= 0.0) {
        return 1.0;
    }
    factor = std::clamp(factor, 0.25, 4.0);
    const int oldSize = structure.size();
    const int newSize = std::clamp(
        static_cast<int>(std::lround(oldSize * factor)), 2, 48);
    if (newSize == oldSize) {
        return 1.0;
    }

    VoxelStructure next(newSize);
    const double ratio = static_cast<double>(oldSize) / newSize;
    for (int y = 0; y < newSize; ++y) {
        for (int z = 0; z < newSize; ++z) {
            for (int x = 0; x < newSize; ++x) {
                const int sx = std::clamp(
                    static_cast<int>(std::floor((x + 0.5) * ratio - 0.5)), 0,
                    oldSize - 1);
                const int sy = std::clamp(
                    static_cast<int>(std::floor((y + 0.5) * ratio - 0.5)), 0,
                    oldSize - 1);
                const int sz = std::clamp(
                    static_cast<int>(std::floor((z + 0.5) * ratio - 0.5)), 0,
                    oldSize - 1);
                next.set(x, y, z, structure.get(sx, sy, sz));
            }
        }
    }

    structure = std::move(next);
    return static_cast<double>(newSize) / oldSize;
}

void translate(VoxelStructure &structure, int dx, int dy, int dz) {
    if (dx == 0 && dy == 0 && dz == 0) {
        return;
    }
    const int n = structure.size();
    VoxelStructure next(n);
    for (int y = 0; y < n; ++y) {
        for (int z = 0; z < n; ++z) {
            for (int x = 0; x < n; ++x) {
                const auto value = structure.get(x, y, z);
                if (value == 0) {
                    continue;
                }
                next.set(x + dx, y + dy, z + dz, value);
            }
        }
    }
    structure = std::move(next);
}

VoxelStructure downsampled(const VoxelStructure &structure, int factor) {
    if (factor <= 1) {
        return structure;
    }
    const int oldSize = structure.size();
    const int newSize = std::max(2, oldSize / factor);
    VoxelStructure next(newSize);

    for (int y = 0; y < newSize; ++y) {
        for (int z = 0; z < newSize; ++z) {
            for (int x = 0; x < newSize; ++x) {
                std::uint16_t found = 0;
                for (int dy = 0; dy < factor && found == 0; ++dy) {
                    for (int dz = 0; dz < factor && found == 0; ++dz) {
                        for (int dx = 0; dx < factor && found == 0; ++dx) {
                            const auto value = structure.get(
                                x * factor + dx, y * factor + dy,
                                z * factor + dz);
                            if (value != 0) {
                                found = value;
                            }
                        }
                    }
                }
                if (found != 0) {
                    next.set(x, y, z, found);
                }
            }
        }
    }
    return next;
}

} // namespace levicreate::voxel::transform
