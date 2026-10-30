#pragma once

#include <deque>
#include <filesystem>
#include <mutex>
#include <string>

#include "mod/Config.h"
#include "mod/ui/Projection.h"
#include "mod/ui/StudioLayout.h"
#include "mod/voxel/Palette.h"
#include "mod/voxel/VoxelIO.h"
#include "mod/voxel/VoxelStructure.h"
#include "mod/voxel/VoxelTransform.h"

namespace pl::log {
class Logger;
}

namespace levicreate {

// Central shared studio state. All access is serialized through the internal
// mutex because touch callbacks, button events and Mod Menu config callbacks
// arrive on different threads.
class VoxelStudio {
public:
    static VoxelStudio &instance();

    VoxelStudio(const VoxelStudio &) = delete;
    VoxelStudio &operator=(const VoxelStudio &) = delete;

    void begin(pl::log::Logger &logger, const std::filesystem::path &dataDir,
               const ModConfig &config);

    // Config ----------------------------------------------------------------
    void applyConfig(const ModConfig &config);
    [[nodiscard]] ModConfig config() const;

    // Grid ------------------------------------------------------------------
    void resizeGrid(int newSize);
    void clearGrid();

    // Tools -----------------------------------------------------------------
    void setTool(Tool tool);
    void cycleTool();
    [[nodiscard]] Tool tool() const;
    void cycleMaterial();
    [[nodiscard]] int material() const;

    // Transforms ------------------------------------------------------------
    void rotateY(double degrees); // any-degree (Axiom style)
    void rotateX90();
    void mirror();
    void scaleStep(int direction);
    void nudge(int dx, int dy, int dz); // 1-voxel translate

    // Clipboard -------------------------------------------------------------
    [[nodiscard]] bool copyAll();
    [[nodiscard]] bool pasteAll();
    [[nodiscard]] bool hasClipboard() const;

    // Slots -----------------------------------------------------------------
    [[nodiscard]] bool saveToSlot(int slot);
    [[nodiscard]] bool loadFromSlot(int slot);

    // Undo ------------------------------------------------------------------
    void pushUndo();
    void undo();
    [[nodiscard]] std::size_t undoDepth() const;

    // Camera / view ---------------------------------------------------------
    void orbit(float deltaYawDeg, float deltaPitchDeg);
    void orbitByScreenDelta(float dxPx, float dyPx);
    [[nodiscard]] ui::ViewTransform view() const;
    void setView(const ui::ViewTransform &view);
    [[nodiscard]] float yaw() const;
    [[nodiscard]] float pitch() const;

    // Rendering support ------------------------------------------------------
    [[nodiscard]] voxel::VoxelStructure snapshot() const;
    [[nodiscard]] bool hasBoxAnchor() const;
    [[nodiscard]] int boxAnchorX() const;
    [[nodiscard]] int boxAnchorY() const;
    [[nodiscard]] int boxAnchorZ() const;
    [[nodiscard]] bool insideCanvas(float screenX, float screenY) const;
    [[nodiscard]] ui::StudioLayout studioLayout() const;

    // Screen bounds auto-detection ------------------------------------------
    void noteTouchExtent(float x, float y);
    [[nodiscard]] float screenWidth() const;
    [[nodiscard]] float screenHeight() const;

    // Canvas interaction -----------------------------------------------------
    struct PickResult {
        bool hit = false;
        int x = 0, y = 0, z = 0;    // solid cell (or target cell when empty)
        int px = -1, py = -1, pz = -1; // last empty cell before the hit
        bool insideGrid = false;
    };

    // Applies the active tool to the canvas cell under the screen point.
    // Returns true when the structure changed.
    [[nodiscard]] bool applyToolAt(float screenX, float screenY,
                                   bool isDrag);
    [[nodiscard]] PickResult pickAt(float screenX, float screenY) const;
    void moveByScreenDelta(float dxPx, float dyPx);
    void cancelBoxAnchor();

    // Redraw bookkeeping -----------------------------------------------------
    [[nodiscard]] bool consumeDirty();
    void markDirty();

    // Stats for the HUD ------------------------------------------------------
    struct Status {
        std::string tool;
        std::string grid;
        std::string voxels;
        std::string material;
        std::string angle;
        std::string slot;
        std::string clipboard;
        std::string hint;
    };
    [[nodiscard]] Status status() const;

private:
    VoxelStudio() = default;

    mutable std::recursive_mutex mMutex;
    pl::log::Logger *mLogger = nullptr;
    ModConfig mConfig{};
    voxel::VoxelStructure mStructure{16};
    voxel::VoxelIO mIO{std::filesystem::path("data")};
    std::deque<voxel::VoxelStructure> mUndo;
    voxel::VoxelStructure mClipboard{16};
    bool mClipboardValid = false;

    Tool mTool = Tool::Build;
    int mMaterial = 3;
    float mYaw = 45.0f;
    float mPitch = 28.0f;
    ui::ViewTransform mView{};

    bool mBoxAnchorSet = false;
    int mBoxX = 0, mBoxY = 0, mBoxZ = 0;
    float mMoveRemainderX = 0.0f;
    float mMoveRemainderY = 0.0f;

    float mScreenMaxX = 0.0f;
    float mScreenMaxY = 0.0f;

    bool mDirty = true;

    void logAction(const std::string &message);
};

} // namespace levicreate
