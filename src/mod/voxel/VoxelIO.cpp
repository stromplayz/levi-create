#include "mod/voxel/VoxelIO.h"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace levicreate::voxel {

namespace {

bool writeFileAtomic(const std::filesystem::path &path,
                     const std::string &content) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return false;
    }
    const auto temp = path.string() + ".tmp";
    {
        std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
        if (!stream) {
            return false;
        }
        stream << content;
        if (!stream.good()) {
            return false;
        }
    }
    std::filesystem::rename(temp, path, ec);
    if (!ec) {
        return true;
    }
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return false;
    }
    return true;
}

bool readFile(const std::filesystem::path &path, std::string &out) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    out = buffer.str();
    return true;
}

} // namespace

VoxelIO::VoxelIO(std::filesystem::path dataDir)
    : mDataDir(std::move(dataDir)) {}

std::filesystem::path VoxelIO::slotPath(int slot) const {
    return structuresDir() / ("slot_" + std::to_string(slot) + ".json");
}

bool VoxelIO::saveSlot(int slot, const VoxelStructure &structure,
                       const std::string &materialName) {
    try {
        auto root = nlohmann::json::parse(structure.toJson());
        root["material"] = materialName;
        return writeFileAtomic(slotPath(slot), root.dump(2));
    } catch (...) {
        return false;
    }
}

bool VoxelIO::loadSlot(int slot, VoxelStructure &structure,
                       std::string &materialName) {
    std::string content;
    if (!readFile(slotPath(slot), content)) {
        return false;
    }
    if (!structure.fromJson(content)) {
        return false;
    }
    try {
        const auto root = nlohmann::json::parse(content);
        if (root.contains("material") && root["material"].is_string()) {
            materialName = root["material"].get<std::string>();
        }
    } catch (...) {
    }
    return true;
}

bool VoxelIO::slotExists(int slot) const {
    std::error_code ec;
    return std::filesystem::exists(slotPath(slot), ec);
}

std::vector<std::string> VoxelIO::listSlots() const {
    std::vector<std::string> slots;
    std::error_code ec;
    if (!std::filesystem::exists(structuresDir(), ec)) {
        return slots;
    }
    for (const auto &entry :
         std::filesystem::directory_iterator(structuresDir(), ec)) {
        slots.push_back(entry.path().filename().string());
    }
    std::sort(slots.begin(), slots.end());
    return slots;
}

bool VoxelIO::saveClipboard(const VoxelStructure &structure) {
    return writeFileAtomic(clipboardPath(), structure.toJson());
}

bool VoxelIO::loadClipboard(VoxelStructure &structure) {
    std::string content;
    if (!readFile(clipboardPath(), content)) {
        return false;
    }
    return structure.fromJson(content);
}

} // namespace levicreate::voxel
