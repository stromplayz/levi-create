#include "mod/VoxelStudio.h"

#include <algorithm>
#include <cmath>

#include <pl/Logger.hpp>

namespace levicreate {

namespace {

constexpr std::size_t kMaxUndo = 20;

int clampGrid(int size) {
    return std::clamp(size, kMinGridSize, kMaxGridSize);
}

} // namespace

VoxelStudio &VoxelStudio::instance() {
    static VoxelStudio studio;
    return studio;
}

void VoxelStudio::begin(pl::log::Logger &logger,
                        const std::filesystem::path &dataDir,
                        const ModConfig &config) {
    std::lock_guard lock(mMutex);
    mLogger = &logger;
    mConfig = config;
    mIO = voxel::VoxelIO(dataDir);
    mStructure = voxel::VoxelStructure(clampGrid(config.gridSize));
    mMaterial = std::clamp(config.materialIndex, 0, voxel::kPaletteSize - 1);
    mTool = toolFromIndex(config.toolIndex);
    mYaw = 45.0f;
    mPitch = 28.0f;
    mUndo.clear();
    mDirty = true;

    voxel::VoxelStructure clipboard(16);
    if (mIO.loadClipboard(clipboard)) {
        mClipboard = std::move(clipboard);
        mClipboardValid = true;
    }
}

void VoxelStudio::logAction(const std::string &message) {
    if (mLogger) {
        mLogger->info("levi-create: {}", message);
    }
}

void VoxelStudio::applyConfig(const ModConfig &config) {
    std::lock_guard lock(mMutex);
    const int oldGrid = mStructure.size();
    mConfig = config;
    mMaterial =
        std::clamp(config.materialIndex, 0, voxel::kPaletteSize - 1);
    mTool = toolFromIndex(config.toolIndex);
    const int newGrid = clampGrid(config.gridSize);
    if (newGrid != oldGrid) {
        mStructure.resize(newGrid);
        logAction("grid resized to " + std::to_string(newGrid));
    }
    mDirty = true;
}

ModConfig VoxelStudio::config() const {
    std::lock_guard lock(mMutex);
    return mConfig;
}

void VoxelStudio::resizeGrid(int newSize) {
    std::lock_guard lock(mMutex);
    pushUndo();
    mStructure.resize(clampGrid(newSize));
    mConfig.gridSize = clampGrid(newSize);
    logAction("grid resized to " + std::to_string(mStructure.size()));
    mDirty = true;
}

void VoxelStudio::clearGrid() {
    std::lock_guard lock(mMutex);
    pushUndo();
    mStructure.clearAll(0);
    mBoxAnchorSet = false;
    logAction("canvas cleared");
    mDirty = true;
}

void VoxelStudio::setTool(Tool tool) {
    std::lock_guard lock(mMutex);
    mTool = tool;
    mConfig.toolIndex = toolToIndex(tool);
    if (tool != Tool::Box && tool != Tool::Sphere) {
        mBoxAnchorSet = false;
    }
    mDirty = true;
}

void VoxelStudio::cycleTool() {
    std::lock_guard lock(mMutex);
    const int next =
        (toolToIndex(mTool) + 1) % (static_cast<int>(Tool::Move) + 1);
    mTool = static_cast<Tool>(next);
    mConfig.toolIndex = next;
    mBoxAnchorSet = false;
    logAction(std::string("tool -> ") + toolLabel(mTool));
    mDirty = true;
}

Tool VoxelStudio::tool() const {
    std::lock_guard lock(mMutex);
    return mTool;
}

void VoxelStudio::cycleMaterial() {
    std::lock_guard lock(mMutex);
    mMaterial = (mMaterial + 1) % voxel::kPaletteSize;
    mConfig.materialIndex = mMaterial;
    logAction("material -> " +
              std::string(voxel::paletteName(mMaterial)));
    mDirty = true;
}

int VoxelStudio::material() const {
    std::lock_guard lock(mMutex);
    return mMaterial;
}

void VoxelStudio::rotateY(double degrees) {
    std::lock_guard lock(mMutex);
    pushUndo();
    voxel::transform::rotateYDeg(mStructure, degrees);
    logAction("rotated " + std::to_string(static_cast<int>(degrees)) + " deg");
    mDirty = true;
}

void VoxelStudio::rotateX90() {
    std::lock_guard lock(mMutex);
    pushUndo();
    voxel::transform::rotateX90(mStructure);
    logAction("tilted 90 deg");
    mDirty = true;
}

void VoxelStudio::mirror() {
    std::lock_guard lock(mMutex);
    pushUndo();
    voxel::transform::mirrorX(mStructure);
    logAction("mirrored");
    mDirty = true;
}

void VoxelStudio::scaleStep(int direction) {
    std::lock_guard lock(mMutex);
    pushUndo();
    const double factor = direction >= 0 ? 1.25 : 0.8;
    const double applied = voxel::transform::scaleBy(mStructure, factor);
    mConfig.gridSize = mStructure.size();
    logAction("scaled x" + std::to_string(applied).substr(0, 5) + " -> size " +
              std::to_string(mStructure.size()));
    mDirty = true;
}

void VoxelStudio::nudge(int dx, int dy, int dz) {
    std::lock_guard lock(mMutex);
    pushUndo();
    voxel::transform::translate(mStructure, dx, dy, dz);
    logAction("moved by " + std::to_string(dx) + "," + std::to_string(dy) +
              "," + std::to_string(dz));
    mDirty = true;
}

bool VoxelStudio::copyAll() {
    std::lock_guard lock(mMutex);
    mClipboard = mStructure;
    mClipboardValid = true;
    mIO.saveClipboard(mClipboard);
    logAction("copied " + std::to_string(mStructure.countNonAir()) +
              " voxels to clipboard");
    mDirty = true;
    return true;
}

bool VoxelStudio::pasteAll() {
    std::lock_guard lock(mMutex);
    if (!mClipboardValid) {
        return false;
    }
    pushUndo();
    mStructure = mClipboard;
    mConfig.gridSize = mStructure.size();
    logAction("pasted clipboard");
    mDirty = true;
    return true;
}

bool VoxelStudio::hasClipboard() const {
    std::lock_guard lock(mMutex);
    return mClipboardValid;
}

bool VoxelStudio::saveToSlot(int slot) {
    std::lock_guard lock(mMutex);
    const bool ok = mIO.saveSlot(
        slot, mStructure, std::string(voxel::paletteName(mMaterial)));
    logAction(std::string(ok ? "saved to slot " : "save failed slot ") +
              std::to_string(slot));
    mDirty = true;
    return ok;
}

bool VoxelStudio::loadFromSlot(int slot) {
    std::lock_guard lock(mMutex);
    std::string materialName;
    voxel::VoxelStructure loaded(16);
    if (!mIO.loadSlot(slot, loaded, materialName)) {
        logAction("load failed: slot " + std::to_string(slot) + " empty");
        return false;
    }
    pushUndo();
    mStructure = std::move(loaded);
    mConfig.gridSize = mStructure.size();
    logAction("loaded slot " + std::to_string(slot));
    mDirty = true;
    return true;
}

void VoxelStudio::pushUndo() {
    if (mUndo.size() >= kMaxUndo) {
        mUndo.pop_front();
    }
    mUndo.push_back(mStructure);
}

void VoxelStudio::undo() {
    std::lock_guard lock(mMutex);
    if (mUndo.empty()) {
        return;
    }
    mStructure = std::move(mUndo.back());
    mUndo.pop_back();
    mConfig.gridSize = mStructure.size();
    mBoxAnchorSet = false;
    logAction("undo -> " + std::to_string(mUndo.size()) + " steps left");
    mDirty = true;
}

std::size_t VoxelStudio::undoDepth() const {
    std::lock_guard lock(mMutex);
    return mUndo.size();
}

void VoxelStudio::orbit(float deltaYawDeg, float deltaPitchDeg) {
    std::lock_guard lock(mMutex);
    mYaw = std::fmod(mYaw + deltaYawDeg + 360.0f, 360.0f);
    mPitch = std::clamp(mPitch + deltaPitchDeg, -80.0f, 80.0f);
    mDirty = true;
}

void VoxelStudio::orbitByScreenDelta(float dxPx, float dyPx) {
    const float sensitivity = 0.35f;
    orbit(dxPx * sensitivity, -dyPx * sensitivity);
}

ui::ViewTransform VoxelStudio::view() const {
    std::lock_guard lock(mMutex);
    return mView;
}

void VoxelStudio::setView(const ui::ViewTransform &view) {
    std::lock_guard lock(mMutex);
    mView = view;
}

float VoxelStudio::yaw() const {
    std::lock_guard lock(mMutex);
    return mYaw;
}

float VoxelStudio::pitch() const {
    std::lock_guard lock(mMutex);
    return mPitch;
}

voxel::VoxelStructure VoxelStudio::snapshot() const {
    std::lock_guard lock(mMutex);
    return mStructure;
}

bool VoxelStudio::hasBoxAnchor() const {
    std::lock_guard lock(mMutex);
    return mBoxAnchorSet;
}

int VoxelStudio::boxAnchorX() const {
    std::lock_guard lock(mMutex);
    return mBoxX;
}

int VoxelStudio::boxAnchorY() const {
    std::lock_guard lock(mMutex);
    return mBoxY;
}

int VoxelStudio::boxAnchorZ() const {
    std::lock_guard lock(mMutex);
    return mBoxZ;
}

bool VoxelStudio::insideCanvas(float screenX, float screenY) const {
    std::lock_guard lock(mMutex);
    const auto config = mConfig;
    const float sw = screenWidth();
    const float sh = screenHeight();
    const auto layout =
        ui::computeLayout(sw, sh, config.panelScale);
    return layout.contains(screenX, screenY);
}

ui::StudioLayout VoxelStudio::studioLayout() const {
    std::lock_guard lock(mMutex);
    const auto config = mConfig;
    return ui::computeLayout(screenWidth(), screenHeight(),
                             config.panelScale);
}

void VoxelStudio::noteTouchExtent(float x, float y) {
    std::lock_guard lock(mMutex);
    if (!mConfig.autoDetectScreen) {
        return;
    }
    if (x > mScreenMaxX) {
        mScreenMaxX = x;
    }
    if (y > mScreenMaxY) {
        mScreenMaxY = y;
    }
}

float VoxelStudio::screenWidth() const {
    std::lock_guard lock(mMutex);
    const float detected = mScreenMaxX + 40.0f;
    const float base = static_cast<float>(mConfig.baseScreenWidth);
    return std::max(base, detected);
}

float VoxelStudio::screenHeight() const {
    std::lock_guard lock(mMutex);
    const float detected = mScreenMaxY + 40.0f;
    const float base = static_cast<float>(mConfig.baseScreenHeight);
    return std::max(base, detected);
}

VoxelStudio::PickResult VoxelStudio::pickAt(float screenX,
                                            float screenY) const {
    std::lock_guard lock(mMutex);
    const auto ray = ui::rayThroughPixel(mView, screenX, screenY);
    const int n = mStructure.size();

    // Slab clip of the ray against [0, n]^3.
    float tMin = 0.0f;
    float tMax = 1e9f;
    const float origins[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float dirs[3] = {ray.dir.x, ray.dir.y, ray.dir.z};
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(dirs[axis]) < 1e-6f) {
            if (origins[axis] < 0.0f || origins[axis] > n) {
                PickResult miss;
                return miss;
            }
            continue;
        }
        float t0 = (0.0f - origins[axis]) / dirs[axis];
        float t1 = (static_cast<float>(n) - origins[axis]) / dirs[axis];
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        tMin = std::max(tMin, t0);
        tMax = std::min(tMax, t1);
    }
    if (tMin > tMax) {
        PickResult miss;
        return miss;
    }

    // DDA (Amanatides & Woo) from the grid entry point.
    float px = ray.origin.x + ray.dir.x * tMin;
    float py = ray.origin.y + ray.dir.y * tMin;
    float pz = ray.origin.z + ray.dir.z * tMin;
    int x = std::clamp(static_cast<int>(px), 0, n - 1);
    int y = std::clamp(static_cast<int>(py), 0, n - 1);
    int z = std::clamp(static_cast<int>(pz), 0, n - 1);

    const int stepX = ray.dir.x > 0 ? 1 : -1;
    const int stepY = ray.dir.y > 0 ? 1 : -1;
    const int stepZ = ray.dir.z > 0 ? 1 : -1;

    auto boundary = [](int cell, float direction) {
        return direction > 0 ? static_cast<float>(cell + 1)
                             : static_cast<float>(cell);
    };
    auto tDelta = [](float direction) {
        return std::fabs(direction) < 1e-6f
                   ? 1e9f
                   : std::fabs(1.0f / direction);
    };

    const float tDeltaX = tDelta(ray.dir.x);
    const float tDeltaY = tDelta(ray.dir.y);
    const float tDeltaZ = tDelta(ray.dir.z);

    float tMaxX = tDeltaX <= 1e8f
                      ? (boundary(x, ray.dir.x) - px) / (ray.dir.x == 0 ? 1e-6f
                                                                        : ray.dir.x)
                      : 1e9f;
    float tMaxY = tDeltaY <= 1e8f
                      ? (boundary(y, ray.dir.y) - py) / (ray.dir.y == 0 ? 1e-6f
                                                                        : ray.dir.y)
                      : 1e9f;
    float tMaxZ = tDeltaZ <= 1e8f
                      ? (boundary(z, ray.dir.z) - pz) / (ray.dir.z == 0 ? 1e-6f
                                                                        : ray.dir.z)
                      : 1e9f;
    if (tMaxX <= 0) tMaxX = 1e9f;
    if (tMaxY <= 0) tMaxY = 1e9f;
    if (tMaxZ <= 0) tMaxZ = 1e9f;

    PickResult result;
    int lastEmptyX = x, lastEmptyY = y, lastEmptyZ = z;

    // Guard against pathological loops.
    for (int steps = 0; steps < 512; ++steps) {
        if (mStructure.inBounds(x, y, z) && mStructure.isSolid(x, y, z)) {
            result.hit = true;
            result.x = x;
            result.y = y;
            result.z = z;
            result.px = lastEmptyX;
            result.py = lastEmptyY;
            result.pz = lastEmptyZ;
            result.insideGrid = true;
            return result;
        }
        lastEmptyX = x;
        lastEmptyY = y;
        lastEmptyZ = z;

        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            x += stepX;
            tMaxX += tDeltaX;
        } else if (tMaxY < tMaxZ) {
            y += stepY;
            tMaxY += tDeltaY;
        } else {
            z += stepZ;
            tMaxZ += tDeltaZ;
        }

        if (x < 0 || y < 0 || z < 0 || x >= n || y >= n || z >= n) {
            break;
        }
    }

    // No solid hit: target the deepest empty cell visited inside the grid so
    // Build can still place in mid-air grids.
    if (mStructure.inBounds(lastEmptyX, lastEmptyY, lastEmptyZ)) {
        result.hit = false;
        result.x = lastEmptyX;
        result.y = lastEmptyY;
        result.z = lastEmptyZ;
        result.px = lastEmptyX;
        result.py = lastEmptyY;
        result.pz = lastEmptyZ;
        result.insideGrid = true;
    }
    return result;
}

bool VoxelStudio::applyToolAt(float screenX, float screenY, bool isDrag) {
    std::lock_guard lock(mMutex);
    const auto pick = pickAt(screenX, screenY);
    if (!pick.insideGrid) {
        return false;
    }

    const auto value = static_cast<std::uint16_t>(mMaterial + 1);
    bool changed = false;

    switch (mTool) {
    case Tool::Build:
    { // tap places a bit; drag paints a continuous line of bits
        const int tx = pick.hit ? pick.px : pick.x;
        const int ty = pick.hit ? pick.py : pick.y;
        const int tz = pick.hit ? pick.pz : pick.z;
        if (!mStructure.isSolid(tx, ty, tz)) {
            pushUndo();
            mStructure.set(tx, ty, tz, value);
            changed = true;
        }
    }
    break;
    case Tool::Erase:
        if (pick.hit) {
            pushUndo();
            mStructure.set(pick.x, pick.y, pick.z, 0);
            changed = true;
        }
        break;
    case Tool::Paint:
        if (pick.hit &&
            mStructure.get(pick.x, pick.y, pick.z) != value) {
            pushUndo();
            mStructure.set(pick.x, pick.y, pick.z, value);
            changed = true;
        }
        break;
    case Tool::Box:
        if (!isDrag) {
            if (!mBoxAnchorSet) {
                mBoxAnchorSet = true;
                mBoxX = pick.x;
                mBoxY = pick.y;
                mBoxZ = pick.z;
                logAction("box anchor set at " + std::to_string(mBoxX) + "," +
                          std::to_string(mBoxY) + "," +
                          std::to_string(mBoxZ));
            } else {
                pushUndo();
                mStructure.fillBox(mBoxX, mBoxY, mBoxZ, pick.x, pick.y,
                                   pick.z, value);
                mBoxAnchorSet = false;
                changed = true;
                logAction("box filled");
            }
        }
        break;
    case Tool::Sphere:
        if (!isDrag) {
            if (!mBoxAnchorSet) {
                mBoxAnchorSet = true;
                mBoxX = pick.x;
                mBoxY = pick.y;
                mBoxZ = pick.z;
                logAction("sphere center set");
            } else {
                const float radius = std::clamp(
                    std::sqrt(static_cast<float>(
                        (pick.x - mBoxX) * (pick.x - mBoxX) +
                        (pick.y - mBoxY) * (pick.y - mBoxY) +
                        (pick.z - mBoxZ) * (pick.z - mBoxZ))),
                    1.0f, static_cast<float>(mStructure.size()));
                pushUndo();
                mStructure.fillSphere(mBoxX + 0.5f, mBoxY + 0.5f,
                                      mBoxZ + 0.5f, radius, value);
                mBoxAnchorSet = false;
                changed = true;
                logAction("sphere filled r=" +
                          std::to_string(static_cast<int>(radius)));
            }
        }
        break;
    case Tool::Move:
        // Movement is handled by moveByScreenDelta during drags.
        break;
    }

    if (changed) {
        mDirty = true;
    }
    return changed;
}

void VoxelStudio::moveByScreenDelta(float dxPx, float dyPx) {
    std::lock_guard lock(mMutex);
    if (mTool != Tool::Move || mView.scale <= 0.0f) {
        return;
    }
    const auto basis = ui::basisFor(mView);
    const float wx = (dxPx / mView.scale) * basis.right.x +
                     (-dyPx / mView.scale) * basis.up.x;
    const float wy = (dxPx / mView.scale) * basis.right.y +
                     (-dyPx / mView.scale) * basis.up.y;
    const float wz = (dxPx / mView.scale) * basis.right.z +
                     (-dyPx / mView.scale) * basis.up.z;

    mMoveRemainderX += wx;
    mMoveRemainderY += wy;
    const float wzRemainder = wz;

    int ix = static_cast<int>(std::nearbyint(mMoveRemainderX));
    int iy = static_cast<int>(std::nearbyint(mMoveRemainderY));
    int iz = static_cast<int>(std::nearbyint(wzRemainder));

    if (ix == 0 && iy == 0 && iz == 0) {
        return;
    }

    // Consume the applied integer portion, keep fractional remainder.
    mMoveRemainderX -= static_cast<float>(ix);
    mMoveRemainderY -= static_cast<float>(iy);

    // Recompute integer translation for z directly (no remainder needed at
    // this granularity, fractional z accumulates through x/y rounding).
    if (ix != 0 || iy != 0 || iz != 0) {
        voxel::transform::translate(mStructure, ix, iy, iz);
        mDirty = true;
    }
}

void VoxelStudio::cancelBoxAnchor() {
    std::lock_guard lock(mMutex);
    mBoxAnchorSet = false;
    mDirty = true;
}

bool VoxelStudio::consumeDirty() {
    std::lock_guard lock(mMutex);
    const bool dirty = mDirty;
    mDirty = false;
    return dirty;
}

void VoxelStudio::markDirty() {
    std::lock_guard lock(mMutex);
    mDirty = true;
}

VoxelStudio::Status VoxelStudio::status() const {
    std::lock_guard lock(mMutex);
    Status status;
    status.tool = std::string("Tool: ") + toolLabel(mTool) +
                  (mBoxAnchorSet && (mTool == Tool::Box || mTool == Tool::Sphere)
                       ? " (anchor set)"
                       : "");
    status.grid = "Grid: " + std::to_string(mStructure.size()) + " (" +
                  std::to_string(mStructure.size()) + "x" +
                  std::to_string(mStructure.size()) + "x" +
                  std::to_string(mStructure.size()) + ")";
    status.voxels = "Bits: " + std::to_string(mStructure.countNonAir());
    status.material = "Mat: " +
                      std::string(voxel::paletteName(mMaterial));
    status.angle = "View: " + std::to_string(static_cast<int>(mYaw)) + "/" +
                   std::to_string(static_cast<int>(mPitch));
    status.slot = "Slot: " + std::to_string(mConfig.saveSlot) +
                  "  Undo: " + std::to_string(mUndo.size());
    status.clipboard = mClipboardValid ? "Clipboard: full" : "Clipboard: empty";
    switch (mTool) {
    case Tool::Build:
        status.hint = "tap: place bit";
        break;
    case Tool::Erase:
        status.hint = "tap: remove bit";
        break;
    case Tool::Paint:
        status.hint = "tap: recolor";
        break;
    case Tool::Box:
        status.hint = "tap two corners";
        break;
    case Tool::Sphere:
        status.hint = "tap center then edge";
        break;
    case Tool::Move:
        status.hint = "drag to move";
        break;
    }
    return status;
}

} // namespace levicreate
