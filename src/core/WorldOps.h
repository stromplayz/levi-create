#pragma once

#include "core/VoxelModel.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

// Forward declaration of LeviLamina/MC types (defined in MC headers included by the .cpp)
class BlockSource;

namespace lc {

inline constexpr std::uint64_t kMaxRegionBlocks = 2000000; // 2M blocks per operation
inline constexpr int           kMaxRegionSide   = 512;     // per axis

struct PlaceOptions {
    bool          includeAir = false;
    std::uint64_t maxBlocks  = kMaxRegionBlocks;
};

struct PlaceResult {
    std::uint64_t placed     = 0;
    std::uint64_t failed     = 0;
    std::uint64_t airSkipped = 0;
};

class WorldOps {
public:
    /// Reads a world region into a voxel model.
    static std::optional<VoxelModel>
    captureRegion(BlockSource& bs, int3 mn, int3 mx, std::string& err, std::uint64_t maxBlocks = kMaxRegionBlocks);

    /// Writes a voxel model into the world at `origin` (model's 0,0,0 corner).
    static PlaceResult placeModel(
        BlockSource&       bs,
        int3               origin,
        VoxelModel const&  m,
        PlaceOptions const& opt,
        std::string&       err
    );

    /// Fills a region with one block type. Returns changed count or false on error.
    static bool fillRegion(
        BlockSource&       bs,
        int3               mn,
        int3               mx,
        std::string const& blockName,
        bool               blockIsAir,
        std::uint64_t&     changed,
        std::string&       err
    );

    /// Replaces one block type with another inside a region.
    static bool replaceRegion(
        BlockSource&       bs,
        int3               mn,
        int3               mx,
        std::string const& fromName,
        std::string const& toName,
        std::uint64_t&     changed,
        std::string&       err
    );

    /// Counts block types in a region. Returns false on error.
    static bool countRegion(
        BlockSource&                                              bs,
        int3                                                      mn,
        int3                                                      mx,
        std::unordered_map<std::string, std::uint64_t>&           counts,
        std::string&                                              err
    );
};

} // namespace lc
