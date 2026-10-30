#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "mod/voxel/VoxelStructure.h"

namespace levicreate::voxel {

// Structure slot storage under <dataDir>/structures/slot_<1..9>.json and a
// clipboard snapshot under <dataDir>/clipboard.json.
class VoxelIO {
public:
    explicit VoxelIO(std::filesystem::path dataDir);

    [[nodiscard]] bool saveSlot(int slot, const VoxelStructure &structure,
                                const std::string &materialName);
    [[nodiscard]] bool loadSlot(int slot, VoxelStructure &structure,
                                std::string &materialName);
    [[nodiscard]] bool slotExists(int slot) const;
    [[nodiscard]] std::vector<std::string> listSlots() const;

    [[nodiscard]] bool saveClipboard(const VoxelStructure &structure);
    [[nodiscard]] bool loadClipboard(VoxelStructure &structure);

private:
    std::filesystem::path mDataDir;

    [[nodiscard]] std::filesystem::path structuresDir() const {
        return mDataDir / "structures";
    }
    [[nodiscard]] std::filesystem::path slotPath(int slot) const;
    [[nodiscard]] std::filesystem::path clipboardPath() const {
        return mDataDir / "clipboard.json";
    }
};

} // namespace levicreate::voxel
