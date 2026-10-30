#pragma once

#include <vector>

#include <pl/ModMenu.hpp>

#include "mod/ui/StudioLayout.h"

namespace levicreate {

// Builds and submits the HUD overlay draw commands for the voxel studio
// canvas: isometric shaded preview, panel frame, status text and tool
// highlight. Keeps the command count under the preloader's 4096 cap by
// downsampling dense structures (LOD).
class HudPresenter {
public:
    void submit();

private:
    void appendPreview(
        const ui::StudioLayout &layout,
        std::vector<pl::modmenu::DrawCommand> &commands, int budget);

    void buildCommands(const ui::StudioLayout &layout,
                       std::vector<pl::modmenu::DrawCommand> &commands);
};

// Convenience: one shared presenter instance, submits the current overlay.
void refreshStudioHud();

} // namespace levicreate
