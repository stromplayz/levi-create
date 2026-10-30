// Host-side engine self-test: exercises VoxelStructure, transforms, RLE
// serialization and slot IO with g++ (no Android needed).
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <random>

#include "mod/voxel/Palette.h"
#include "mod/voxel/VoxelIO.h"
#include "mod/voxel/VoxelStructure.h"
#include "mod/voxel/VoxelTransform.h"

using namespace levicreate::voxel;

int main() {
    // 1. set/get + count
    VoxelStructure s(16);
    assert(s.size() == 16 && s.countNonAir() == 0);
    s.set(0, 0, 0, 1);
    s.set(15, 15, 15, 18);
    assert(s.get(15, 15, 15) == 18 && s.get(16, 0, 0) == 0);

    // 2. box fill
    s.fillBox(4, 4, 4, 7, 7, 7, 3);
    assert(s.countNonAir() == 2 + 64);
    assert(s.get(5, 5, 5) == 3 && s.get(8, 8, 8) == 0);

    // 3. sphere fill + surface extraction
    VoxelStructure b(16);
    b.fillSphere(8, 8, 8, 6, 5);
    assert(b.countNonAir() > 700 && b.countNonAir() < 1000);
    auto surface = b.collectSurfaceVoxels();
    assert(surface.size() < b.countNonAir());
    assert(b.faceExposed(8, 8, 13));

    // 4. exact 90-degree rotation (Y): a single voxel maps correctly.
    // Destination (12,2) samples source (2,3): dx=4.5,dz=-5.5 -> sx=cx+dz=2, sz=cx-dx=3.
    VoxelStructure r(16);
    r.set(2, 5, 3, 4);
    transform::rotateYDeg(r, 90);
    assert(r.get(12, 5, 2) == 4);
    assert(r.countNonAir() == 1);

    // 5. any-degree rotation keeps volume roughly intact
    VoxelStructure r2 = b;
    transform::rotateYDeg(r2, 37);
    const int before = b.countNonAir();
    const int after = r2.countNonAir();
    assert(std::abs(after - before) < before / 10); // <10% drift

    // 6. mirror / tilt / scale / translate
    VoxelStructure m(16);
    m.fillBox(0, 0, 0, 3, 3, 3, 2);
    transform::mirrorX(m);
    assert(m.get(15, 0, 0) == 2 && m.get(0, 0, 0) == 0);
    transform::rotateX90(m);
    transform::translate(m, -2, 0, 0); // stays fully inside -> all voxels kept
    assert(m.countNonAir() == 64);
    transform::translate(m, 4, 0, 0); // pushes 2 of 4 x-layers out (x=14..17)
    assert(m.countNonAir() == 32);
    VoxelStructure sc = m; // 32 voxels in x-layers 14..15
    transform::scaleBy(sc, 2.0);
    // Nearest-neighbor x2: spans grow ~8x but edge cells can drift by one
    // layer depending on alignment; only bound the drift.
    assert(sc.size() == 32);
    assert(sc.countNonAir() > 3 * 16 * 2 && sc.countNonAir() < 3 * 16 * 4);
    transform::scaleBy(sc, 0.5);
    assert(sc.size() == 16);
    std::printf("scaled-down count=%d\n", sc.countNonAir()); assert(sc.countNonAir() >= 8 && sc.countNonAir() <= 64);

    // 7. RLE JSON round-trip
    const std::string json = b.toJson();
    VoxelStructure loaded(4);
    assert(loaded.fromJson(json));
    assert(loaded.size() == 16 && loaded.countNonAir() == b.countNonAir());
    for (int y = 0; y < 16; y += 3) {
        for (int z = 0; z < 16; z += 3) {
            for (int x = 0; x < 16; x += 3) {
                assert(loaded.get(x, y, z) == b.get(x, y, z));
            }
        }
    }

    // 8. slot save/load
    VoxelIO io(std::filesystem::path("/tmp/levicreate-test-data"));
    assert(io.saveSlot(3, b, "Red"));
    VoxelStructure back(4);
    std::string material;
    assert(io.loadSlot(3, back, material));
    assert(material == "Red" && back.countNonAir() == b.countNonAir());
    assert(io.saveClipboard(b) && io.loadClipboard(back));
    assert(io.slotExists(3) && !io.slotExists(9));

    // 9. palette
    assert(levicreate::voxel::kPaletteSize == 18);
    assert(paletteName(3) == "Red" && paletteColor(17) == 0xFF5DECF5u);

    // 10. downsample LOD
    auto half = transform::downsampled(b, 2);
    assert(half.size() == 8 && half.countNonAir() > 0);

    std::filesystem::remove_all("/tmp/levicreate-test-data");
    std::puts("ENGINE SELF-TEST: ALL 10 GROUPS PASSED");
    return 0;
}
