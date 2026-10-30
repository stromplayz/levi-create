#pragma once

namespace levicreate::ui {

// Shared HUD layout so the presenter and the touch controller agree on where
// the studio canvas sits on screen.
struct StudioLayout {
    float screenW = 1080.0f;
    float screenH = 2340.0f;
    float canvasX = 0.0f;
    float canvasY = 0.0f;
    float canvasW = 0.0f;
    float canvasH = 0.0f;
    float textSize = 30.0f;
    float padding = 16.0f;

    [[nodiscard]] bool contains(float x, float y) const {
        return x >= canvasX && x <= canvasX + canvasW && y >= canvasY &&
               y <= canvasY + canvasH;
    }
};

[[nodiscard]] inline StudioLayout computeLayout(float screenW, float screenH,
                                                double panelScale) {
    const float s = static_cast<float>(panelScale);
    StudioLayout layout;
    layout.screenW = screenW;
    layout.screenH = screenH;
    layout.padding = 16.0f * s;
    layout.textSize = 30.0f * s;

    layout.canvasW = screenW * 0.46f * s;
    layout.canvasH = screenH * 0.34f * s;
    layout.canvasW = layout.canvasW < 640.0f * s ? layout.canvasW : 640.0f * s;
    layout.canvasH = layout.canvasH < 620.0f * s ? layout.canvasH : 620.0f * s;
    layout.canvasX = layout.padding;
    layout.canvasY = screenH - layout.canvasH - layout.padding - 8.0f * s;
    return layout;
}

} // namespace levicreate::ui
