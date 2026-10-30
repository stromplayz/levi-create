#include "core/PlayerOps.h"

#include "mod/LeviCreate.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockSource.h"
#include "mc/deps/core/math/Vec3.h"

#include <algorithm>
#include <cmath>

namespace lc {
namespace ops {

bool requireSelection(Player& player, int3& mn, int3& mx, std::string& err) {
    auto& sel = LeviCreate::getInstance().states().get(player.getRealName()).sel;
    if (!sel.complete()) {
        err = "Select two corners first: use the wand (iron axe; sneak+right-click = pos1, right-click = pos2) or "
              "/vc pos1 and /vc pos2.";
        return false;
    }
    mn = sel.minPos();
    mx = sel.maxPos();
    return true;
}

bool pushUndoSnapshot(Player& player, int3 mn, int3 mx) {
    auto&       data = LeviCreate::getInstance().states().get(player.getRealName());
    std::string err;
    auto        snap = WorldOps::captureRegion(player.getDimensionBlockSource(), mn, mx, err);
    if (!snap) return false;
    UndoEntry e;
    e.minPos   = mn;
    e.snapshot = std::move(*snap);
    data.undo.push_back(std::move(e));
    while (data.undo.size() > PlayerStateMap::kMaxUndoEntries) data.undo.pop_front();
    return true;
}

bool clearRegion(Player& player, int3 mn, int3 mx, std::uint64_t& changed, std::string& err) {
    pushUndoSnapshot(player, mn, mx);
    return WorldOps::fillRegion(player.getDimensionBlockSource(), mn, mx, "minecraft:air", true, changed, err);
}

bool pasteModel(
    Player&            player,
    VoxelModel const&  m,
    int3               origin,
    bool               includeAir,
    PlaceResult&       res,
    std::string&       err
) {
    PlaceOptions opt;
    opt.includeAir = includeAir;
    res            = WorldOps::placeModel(player.getDimensionBlockSource(), origin, m, opt, err);
    return err.empty();
}

int3 feetBlock(Player& player) {
    Vec3 const& pos = player.getPosition();
    return {static_cast<int>(std::floor(pos.x)), static_cast<int>(std::floor(pos.y)),
            static_cast<int>(std::floor(pos.z))};
}

int3 feetAnchor(Player& player, int sx, int sy, int sz) {
    Vec3 const& pos = player.getPosition();
    return {static_cast<int>(std::floor(pos.x)) - sx / 2,
            static_cast<int>(std::floor(pos.y)),
            static_cast<int>(std::floor(pos.z)) - sz / 2};
}

Clipboard* clipboardOf(Player& player) {
    auto& data = LeviCreate::getInstance().states().get(player.getRealName());
    if (!data.clip || !data.clip->valid()) return nullptr;
    return &(*data.clip);
}

bool miniOrExpand(Player& player, int factor, bool expand, std::string& msg, std::string& err) {
    int3 mn, mx;
    if (!requireSelection(player, mn, mx, err)) return false;

    int f = std::clamp(factor, 2, 16);

    auto& bs = player.getDimensionBlockSource();
    auto  model = WorldOps::captureRegion(bs, mn, mx, err);
    if (!model) return false;

    double scale = expand ? static_cast<double>(f) : 1.0 / static_cast<double>(f);
    VoxelModel scaled = scaleModel(*model, static_cast<float>(scale));
    if (scaled.empty()) {
        err = "Scaling produced an empty model.";
        return false;
    }

    // Store in clipboard.
    auto& data = LeviCreate::getInstance().states().get(player.getRealName());
    Clipboard clip;
    clip.model      = scaled;
    clip.sourceName = expand ? "expanded-selection" : "mini-selection";
    data.clip       = std::move(clip);

    // Place beside the original (past the max Z corner), keeping the base level.
    int3 origin{mn.x, mn.y, mx.z + 2};
    PlaceResult res;
    if (!pasteModel(player, scaled, origin, false, res, err)) return false;

    msg = (expand ? "Expanded x" : "Miniature x") + std::to_string(f) + ": placed "
        + formatVolume(res.placed) + " blocks beside the original (" + std::to_string(origin.x) + " "
        + std::to_string(origin.y) + " " + std::to_string(origin.z) + "). Also stored in clipboard (/vc paste).";
    return true;
}

} // namespace ops
} // namespace lc
