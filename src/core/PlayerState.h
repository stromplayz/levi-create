#pragma once

#include "core/VoxelModel.h"

#include <deque>
#include <map>
#include <optional>
#include <string>

namespace lc {

struct Selection {
    bool has1 = false;
    bool has2 = false;
    int3 p1{};
    int3 p2{};
    int  dimId = 0;

    bool complete() const { return has1 && has2; }
    int3 minPos() const { return int3Min(p1, p2); }
    int3 maxPos() const { return int3Max(p1, p2); }
    std::uint64_t volumeBlocks() const {
        int3 mn = minPos();
        int3 mx = maxPos();
        std::int64_t dx = static_cast<std::int64_t>(mx.x) - mn.x + 1;
        std::int64_t dy = static_cast<std::int64_t>(mx.y) - mn.y + 1;
        std::int64_t dz = static_cast<std::int64_t>(mx.z) - mn.z + 1;
        if (dx <= 0 || dy <= 0 || dz <= 0) return 0;
        return static_cast<std::uint64_t>(dx * dy * dz);
    }
    void reset() {
        has1 = has2 = false;
        p1 = p2 = int3{};
    }
};

struct Clipboard {
    VoxelModel  model;
    std::string sourceName = "clipboard";
    bool        valid() const { return !model.empty(); }
};

struct UndoEntry {
    int3       minPos{};
    VoxelModel snapshot; // full block snapshot of the region before modification
};

struct PlayerData {
    Selection                sel;
    std::optional<Clipboard> clip;
    std::deque<UndoEntry>    undo;
};

class PlayerStateMap {
public:
    PlayerData& get(std::string const& playerName);
    void        erase(std::string const& playerName);
    void        pushUndo(std::string const& playerName, UndoEntry&& entry);

    static constexpr std::size_t kMaxUndoEntries = 8;

private:
    std::map<std::string, PlayerData> mPlayers;
};

} // namespace lc
