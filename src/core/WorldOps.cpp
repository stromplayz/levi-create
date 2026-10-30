#include "core/WorldOps.h"

#include "mc/deps/core/string/HashedString.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockChangeContext.h"

#include <unordered_map>

namespace lc {

namespace {

Block const* resolveBlock(std::string const& name) {
    // Block registry is stable while the server runs; cache lookups.
    // All our operations run on the server thread, so no lock is needed.
    static std::unordered_map<std::string, Block const*> cache;
    auto                                                 it = cache.find(name);
    if (it != cache.end()) return it->second;

    Block const* p = nullptr;
    if (auto ref = Block::tryGetFromRegistry(HashedString(name))) {
        if (ref.has_value()) {
            Block const& b = *ref;
            p              = &b;
        }
    }
    cache.emplace(name, p);
    return p;
}

bool setBlockAt(BlockSource& bs, int3 const& p, Block const& b) {
    BlockChangeContext ctx{};
    return bs.setBlock(BlockPos{p.x, p.y, p.z}, b, 3, nullptr, ctx);
}

bool checkBounds(int3 const& mn, int3 const& mx, std::string& err) {
    if (mn.x > mx.x || mn.y > mx.y || mn.z > mx.z) {
        err = "Invalid region (min > max).";
        return false;
    }
    std::int64_t dx = static_cast<std::int64_t>(mx.x) - mn.x + 1;
    std::int64_t dy = static_cast<std::int64_t>(mx.y) - mn.y + 1;
    std::int64_t dz = static_cast<std::int64_t>(mx.z) - mn.z + 1;
    if (dx <= 0 || dy <= 0 || dz <= 0) {
        err = "Invalid region size.";
        return false;
    }
    if (dx > kMaxRegionSide || dy > kMaxRegionSide || dz > kMaxRegionSide) {
        err = "Region too large on one axis (max side " + std::to_string(kMaxRegionSide) + ").";
        return false;
    }
    std::uint64_t vol = static_cast<std::uint64_t>(dx) * static_cast<std::uint64_t>(dy)
                      * static_cast<std::uint64_t>(dz);
    if (vol > kMaxRegionBlocks) {
        err = "Region too large: " + formatVolume(vol) + " blocks (max " + formatVolume(kMaxRegionBlocks)
            + "). Select a smaller area.";
        return false;
    }
    return true;
}

} // namespace

std::optional<VoxelModel>
WorldOps::captureRegion(BlockSource& bs, int3 mn, int3 mx, std::string& err, std::uint64_t maxBlocks) {
    if (!checkBounds(mn, mx, err)) return std::nullopt;
    std::int64_t dx = static_cast<std::int64_t>(mx.x) - mn.x + 1;
    std::int64_t dy = static_cast<std::int64_t>(mx.y) - mn.y + 1;
    std::int64_t dz = static_cast<std::int64_t>(mx.z) - mn.z + 1;
    std::uint64_t vol = static_cast<std::uint64_t>(dx) * static_cast<std::uint64_t>(dy)
                      * static_cast<std::uint64_t>(dz);
    if (vol > maxBlocks) {
        err = "Region too large: " + formatVolume(vol) + " blocks (max " + formatVolume(maxBlocks) + ").";
        return std::nullopt;
    }

    VoxelModel m;
    m.resize(dx, dy, dz);

    std::unordered_map<std::string, VoxelIndex> palMap;
    std::uint64_t                               i = 0;
    for (int y = 0; y < dy; ++y) {
        for (int z = 0; z < dz; ++z) {
            for (int x = 0; x < dx; ++x, ++i) {
                Block const& b = bs.getBlock(BlockPos{mn.x + x, mn.y + y, mn.z + z});
                if (b.isAir()) {
                    m.vox[i] = 0;
                    continue;
                }
                std::string const& name = b.getTypeName();
                auto               it   = palMap.find(name);
                if (it == palMap.end()) {
                    VoxelIndex idx = static_cast<VoxelIndex>(m.addOrGetPalette(name));
                    palMap.emplace(name, idx);
                    m.vox[i] = idx;
                } else {
                    m.vox[i] = it->second;
                }
            }
        }
    }
    return m;
}

PlaceResult WorldOps::placeModel(
    BlockSource&        bs,
    int3                origin,
    VoxelModel const&   m,
    PlaceOptions const& opt,
    std::string&        err
) {
    PlaceResult res;
    if (m.empty()) {
        err = "Nothing to place (empty model).";
        return res;
    }
    if (m.volume() > opt.maxBlocks) {
        err = "Model too large: " + formatVolume(m.volume()) + " blocks (max " + formatVolume(opt.maxBlocks) + ").";
        return res;
    }

    // Resolve palette once.
    std::vector<Block const*> resolved(m.palette.size(), nullptr);
    for (std::size_t i = 0; i < m.palette.size(); ++i) {
        resolved[i] = resolveBlock(m.palette[i]);
    }
    Block const* air = resolveBlock("minecraft:air");

    std::uint64_t i = 0;
    for (int y = 0; y < m.sy; ++y) {
        for (int z = 0; z < m.sz; ++z) {
            for (int x = 0; x < m.sx; ++x, ++i) {
                VoxelIndex v = m.vox[i];
                if (v == 0 && !opt.includeAir) {
                    ++res.airSkipped;
                    continue;
                }
                Block const* b = (v == 0) ? air : resolved[v - 1];
                if (!b) {
                    ++res.failed;
                    continue;
                }
                int3 p{origin.x + x, origin.y + y, origin.z + z};
                if (setBlockAt(bs, p, *b)) ++res.placed;
                else ++res.failed;
            }
        }
    }
    return res;
}

bool WorldOps::fillRegion(
    BlockSource&       bs,
    int3               mn,
    int3               mx,
    std::string const& blockName,
    bool               blockIsAir,
    std::uint64_t&     changed,
    std::string&       err
) {
    changed = 0;
    if (!checkBounds(mn, mx, err)) return false;

    Block const* b = nullptr;
    if (!blockIsAir) {
        b = resolveBlock(blockName);
        if (!b) {
            err = "Unknown block: " + blockName;
            return false;
        }
    } else {
        b = resolveBlock("minecraft:air");
        if (!b) {
            err = "Cannot resolve air block.";
            return false;
        }
    }

    for (int y = mn.y; y <= mx.y; ++y) {
        for (int z = mn.z; z <= mx.z; ++z) {
            for (int x = mn.x; x <= mx.x; ++x) {
                if (setBlockAt(bs, {x, y, z}, *b)) ++changed;
            }
        }
    }
    return true;
}

bool WorldOps::replaceRegion(
    BlockSource&       bs,
    int3               mn,
    int3               mx,
    std::string const& fromName,
    std::string const& toName,
    std::uint64_t&     changed,
    std::string&       err
) {
    changed = 0;
    if (!checkBounds(mn, mx, err)) return false;

    Block const* from = resolveBlock(fromName);
    if (!from) {
        err = "Unknown block: " + fromName;
        return false;
    }
    Block const* to = resolveBlock(toName);
    if (!to) {
        err = "Unknown block: " + toName;
        return false;
    }

    for (int y = mn.y; y <= mx.y; ++y) {
        for (int z = mn.z; z <= mx.z; ++z) {
            for (int x = mn.x; x <= mx.x; ++x) {
                Block const& cur = bs.getBlock(BlockPos{x, y, z});
                // Compare by type name so legacy states still match.
                if (cur.getTypeName() == fromName) {
                    if (setBlockAt(bs, {x, y, z}, *to)) ++changed;
                }
            }
        }
    }
    return true;
}

bool WorldOps::countRegion(
    BlockSource&                                    bs,
    int3                                            mn,
    int3                                            mx,
    std::unordered_map<std::string, std::uint64_t>& counts,
    std::string&                                    err
) {
    counts.clear();
    if (!checkBounds(mn, mx, err)) return false;
    for (int y = mn.y; y <= mx.y; ++y) {
        for (int z = mn.z; z <= mx.z; ++z) {
            for (int x = mn.x; x <= mx.x; ++x) {
                Block const& b = bs.getBlock(BlockPos{x, y, z});
                if (b.isAir()) continue;
                auto& c = counts[b.getTypeName()];
                ++c;
            }
        }
    }
    return true;
}

} // namespace lc
