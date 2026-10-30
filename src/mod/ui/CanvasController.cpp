#include "mod/ui/CanvasController.h"

#include <algorithm>
#include <cmath>

#include <pl/Input.hpp>
#include <pl/Mod.hpp>

#include "mod/Config.h"
#include "mod/VoxelStudio.h"
#include "mod/ui/HudPresenter.h"

namespace levicreate {

namespace {

// Android MotionEvent actions used by the input bridge.
constexpr int kActionDown = 0;
constexpr int kActionUp = 1;
constexpr int kActionMove = 2;
constexpr int kActionCancel = 3;

constexpr float kTapSlopPx = 14.0f;
constexpr int kRefreshMinMs = 16;

bool shouldRefreshNow(std::chrono::steady_clock::time_point &last) {
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last)
            .count() < kRefreshMinMs) {
        return false;
    }
    last = now;
    return true;
}

} // namespace

void CanvasController::enable() {
    mEnabled = true;
    pl::input::registerTouchCallback(
        [this](const pl::input::TouchEvent &event) { return onTouch(event); });
}

void CanvasController::disable() {
    mEnabled = false;
    mGesture = Gesture{};
    // Replace with a no-op so the studio stops consuming touches.
    pl::input::registerTouchCallback(
        [](const pl::input::TouchEvent &) { return false; });
}

bool CanvasController::onTouch(const pl::input::TouchEvent &event) {
    auto &studio = VoxelStudio::instance();
    const auto config = studio.config();

    // Feed the auto screen-size detection with every touch we see.
    studio.noteTouchExtent(event.x, event.y);

    if (!mEnabled || !config.showPanel || !config.captureCanvas) {
        return false;
    }

    switch (event.action) {
    case kActionDown: {
        if (!studio.insideCanvas(event.x, event.y)) {
            return false;
        }
        mGesture.active = true;
        mGesture.pointerId = event.pointerId;
        mGesture.lastX = event.x;
        mGesture.lastY = event.y;
        mGesture.startX = event.x;
        mGesture.startY = event.y;
        mGesture.totalDx = 0.0f;
        mGesture.totalDy = 0.0f;
        return true; // consume: the game must not react to studio edits
    }
    case kActionMove: {
        if (!mGesture.active || event.pointerId != mGesture.pointerId) {
            return mGesture.active;
        }
        const float dx = event.x - mGesture.lastX;
        const float dy = event.y - mGesture.lastY;
        mGesture.lastX = event.x;
        mGesture.lastY = event.y;
        mGesture.totalDx += dx;
        mGesture.totalDy += dy;

        const float total = std::hypot(mGesture.totalDx, mGesture.totalDy);
        if (total > kTapSlopPx) {
            const auto tool = studio.tool();
            if (tool == Tool::Move) {
                studio.moveByScreenDelta(dx, dy);
            } else {
                studio.orbitByScreenDelta(dx, dy);
            }
            if (shouldRefreshNow(mLastRefresh)) {
                refreshStudioHud();
            }
        }
        return true;
    }
    case kActionUp: {
        if (!mGesture.active || event.pointerId != mGesture.pointerId) {
            return mGesture.active;
        }
        mGesture.active = false;
        const float total =
            std::hypot(event.x - mGesture.startX, event.y - mGesture.startY);
        if (total <= kTapSlopPx) {
            studio.applyToolAt(event.x, event.y, false);
        }
        refreshStudioHud();
        return true;
    }
    case kActionCancel: {
        const bool wasActive = mGesture.active;
        mGesture.active = false;
        if (wasActive) {
            refreshStudioHud();
        }
        return wasActive;
    }
    default:
        return mGesture.active;
    }
}

} // namespace levicreate
