#pragma once

#include "core/PlayerState.h"
#include "core/WorldOps.h"

#include <string>

class Player;

namespace lc {

namespace ops {

/// Requires a complete selection; returns the normalized min/max corners.
bool requireSelection(Player& player, int3& mn, int3& mx, std::string& err);

/// Snapshots a region into the player's undo stack.
bool pushUndoSnapshot(Player& player, int3 mn, int3 mx);

/// Clears a region to air (with undo snapshot).
bool clearRegion(Player& player, int3 mn, int3 mx, std::uint64_t& changed, std::string& err);

/// Places a model anchored at `origin`. Returns false with err on hard error.
bool pasteModel(
    Player&            player,
    VoxelModel const&  m,
    int3               origin,
    bool               includeAir,
    PlaceResult&       res,
    std::string&       err
);

/// The block position at the player's feet.
int3 feetBlock(Player& player);

/// Anchor used when pasting: centered on X/Z at the feet, so big models appear around the player.
int3 feetAnchor(Player& player, int sx, int sy, int sz);

Clipboard* clipboardOf(Player& player);

/// Creates a scaled-down (mini, factor<0 means 1/factor) or scaled-up copy of the
/// selection placed beside the original. Also stores it in the clipboard.
bool miniOrExpand(Player& player, int factor, bool expand, std::string& msg, std::string& err);

} // namespace ops

} // namespace lc
