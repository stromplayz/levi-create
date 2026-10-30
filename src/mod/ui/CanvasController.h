#pragma once

#include <chrono>

#include <pl/Input.hpp>

namespace levicreate {

// Routes Android touch events into the voxel studio when they land on the
// studio canvas: tap = apply tool, drag = orbit (or move when the Move tool
// is selected). Touches outside the canvas pass through to the game.
class CanvasController {
public:
    void enable();
    void disable();

private:
    struct Gesture {
        bool active = false;
        int pointerId = -1;
        float lastX = 0.0f;
        float lastY = 0.0f;
        float startX = 0.0f;
        float startY = 0.0f;
        float totalDx = 0.0f;
        float totalDy = 0.0f;
    };

    bool onTouch(const struct pl::input::TouchEvent &event);

    Gesture mGesture;
    std::chrono::steady_clock::time_point mLastRefresh{};
    bool mEnabled = false;
};

} // namespace levicreate
