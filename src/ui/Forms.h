#pragma once

#include <string>

class Player;

namespace lc::forms {

void openMainMenu(Player& player);
void openSelectionMenu(Player& player);
void openTransformMenu(Player& player);
void openPasteMenu(Player& player);
void openLibraryMenu(Player& player);
void openMiniMenu(Player& player);
void openHelpMenu(Player& player);

/// Loads a saved model into the clipboard (and optionally pastes it).
void loadModel(Player& player, std::string const& name, bool pasteNow);

/// Applies the mini (scale down 1/factor) or expand (scale up factor) workflow.
void miniAction(Player& player, int factor, bool expand);

/// Full command reference, shared with the /vc help command.
std::string const& helpText();

} // namespace lc::forms
