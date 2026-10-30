#include "core/PlayerState.h"

namespace lc {

PlayerData& PlayerStateMap::get(std::string const& playerName) { return mPlayers[playerName]; }

void PlayerStateMap::erase(std::string const& playerName) { mPlayers.erase(playerName); }

void PlayerStateMap::pushUndo(std::string const& playerName, UndoEntry&& entry) {
    auto& undo = mPlayers[playerName].undo;
    undo.push_back(std::move(entry));
    while (undo.size() > kMaxUndoEntries) undo.pop_front();
}

} // namespace lc
