#include "mod/ui/HudPresenter.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "mod/Config.h"
#include "mod/VoxelStudio.h"
#include "mod/ui/StudioLayout.h"
#include "mod/voxel/VoxelTransform.h"
namespace levicreate {

namespace {

// The preloader caps a module's overlay at 4096 draw commands
// (kMaxDrawCommandCount in ModMenuBridge.cpp). Reserve room for the panel,
// frame, status text and pick highlight.
constexpr int kCommandReserve = 96;
constexpr int kCommandsPerVoxel = 6; // top + two sides, 2 triangles each

std::uint32_t accentColorOrDefault(const std::string &value) {
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    if ((value.size() == 7 || value.size() == 9) && value.front() == '#') {
        const std::size_t rgbOffset = value.size() - 6;
        const int r = hex(value[rgbOffset]) * 16 + hex(value[rgbOffset + 1]);
        const int g = hex(value[rgbOffset + 2]) * 16 + hex(value[rgbOffset + 3]);
        const int b = hex(value[rgbOffset + 4]) * 16 + hex(value[rgbOffset + 5]);
        const int a =
            value.size() == 9 ? hex(value[1]) * 16 + hex(value[2]) : 0xFF;
        if (r >= 0 && g >= 0 && b >= 0 && a >= 0) {
            return (static_cast<std::uint32_t>(a) << 24) |
                   (static_cast<std::uint32_t>(r) << 16) |
                   (static_cast<std::uint32_t>(g) << 8) |
                   static_cast<std::uint32_t>(b);
        }
    }
    return 0xFF3FD08Cu;
}

std::uint32_t shadeColor(std::uint32_t color, float factor) {
    factor = std::clamp(factor, 0.15f, 1.0f);
    const auto r = static_cast<std::uint32_t>(((color >> 16) & 0xFF) * factor);
    const auto g = static_cast<std::uint32_t>(((color >> 8) & 0xFF) * factor);
    const auto b = static_cast<std::uint32_t>((color & 0xFF) * factor);
    return (color & 0xFF000000u) | (r << 16) | (g << 8) | b;
}

pl::modmenu::DrawCommand makeRectFilled(float x, float y, float w, float h,
                                        std::uint32_t color) {
    pl::modmenu::DrawCommand command;
    command.type = pl::modmenu::DrawCommandType::RectFilled;
    command.x = x;
    command.y = y;
    command.w = w;
    command.h = h;
    command.color = color;
    return command;
}

pl::modmenu::DrawCommand makeTriangle(float x1, float y1, float x2, float y2,
                                      float x3, float y3,
                                      std::uint32_t color) {
    pl::modmenu::DrawCommand command;
    command.type = pl::modmenu::DrawCommandType::TriangleFilled;
    command.x = x1;
    command.y = y1;
    command.w = x2;
    command.h = y2;
    command.x3 = x3;
    command.y3 = y3;
    command.color = color;
    return command;
}

pl::modmenu::DrawCommand makeText(float x, float y, float size,
                                  std::uint32_t color, std::string text) {
    pl::modmenu::DrawCommand command;
    command.type = pl::modmenu::DrawCommandType::Text;
    command.x = x;
    command.y = y;
    command.size = size;
    command.color = color;
    command.text = std::move(text);
    return command;
}

pl::modmenu::DrawCommand makeLine(float x1, float y1, float x2, float y2,
                                  float thickness, std::uint32_t color) {
    pl::modmenu::DrawCommand command;
    command.type = pl::modmenu::DrawCommandType::Line;
    command.x = x1;
    command.y = y1;
    command.w = x2;
    command.h = y2;
    command.size = thickness;
    command.color = color;
    return command;
}

struct FaceQuad {
    float corners[4][3]; // structure-space corners, CCW seen from outside
    float nx, ny, nz;    // outward normal
    float shade;
};

// Six unit-cube faces for a voxel whose min corner is (x, y, z).
void facesForVoxel(float x, float y, float z, FaceQuad out[6]) {
    const float x1 = x + 1.0f;
    const float y1 = y + 1.0f;
    const float z1 = z + 1.0f;

    // +Y top
    out[0] = { { {x, y1, z}, {x, y1, z1}, {x1, y1, z1}, {x1, y1, z} },
               0, 1, 0, 1.00f };
    // +X
    out[1] = { { {x1, y, z}, {x1, y1, z}, {x1, y1, z1}, {x1, y, z1} },
               1, 0, 0, 0.80f };
    // -X
    out[2] = { { {x, y, z1}, {x, y1, z1}, {x, y1, z}, {x, y, z} },
               -1, 0, 0, 0.62f };
    // +Z
    out[3] = { { {x1, y, z1}, {x1, y1, z1}, {x, y1, z1}, {x, y, z1} },
               0, 0, 1, 0.70f };
    // -Z
    out[4] = { { {x, y, z}, {x, y1, z}, {x1, y1, z}, {x1, y, z} },
               0, 0, -1, 0.52f };
    // -Y bottom
    out[5] = { { {x, y, z1}, {x1, y, z1}, {x1, y, z}, {x, y, z} },
               0, -1, 0, 0.40f };
}

} // namespace

void HudPresenter::appendPreview(
    const ui::StudioLayout &layout,
    std::vector<pl::modmenu::DrawCommand> &commands, int budget) {
    auto &studio = VoxelStudio::instance();
    voxel::VoxelStructure structure = studio.snapshot();

    const auto config = studio.config();
    const auto accent = accentColorOrDefault(config.accentColor);
    const auto textDim = 0xFF9AA7B0u;

    const float innerX = layout.canvasX + layout.padding * 0.5f;
    const float innerY = layout.canvasY + layout.padding * 0.5f;
    const float innerW = layout.canvasW - layout.padding;
    const float innerH = layout.canvasH - layout.padding;

    if (structure.countNonAir() == 0) {
        commands.push_back(
            makeText(innerX + innerW * 0.08f, innerY + innerH * 0.42f,
                     layout.textSize * 0.95f, textDim,
                     "Empty grid - tap to place bits"));
        return;
    }

    // LOD: downsample while the visible faces exceed the draw budget.
    int lod = 1;
    auto surface = structure.collectSurfaceVoxels();
    while (static_cast<int>(surface.size()) * kCommandsPerVoxel > budget &&
           lod < 4) {
        structure = voxel::transform::downsampled(structure, lod + 1);
        surface = structure.collectSurfaceVoxels();
        ++lod;
    }

    const int n = structure.size();
    const float h = n / 2.0f;

    // Build the view transform: fit the projected cube into the canvas.
    const float yaw = studio.yaw();
    const float pitch = studio.pitch();
    ui::ViewTransform view;
    view.yawDeg = yaw;
    view.pitchDeg = pitch;
    view.size = n;
    view.scale = 1.0f;
    view.centerX = 0.0f;
    view.centerY = 0.0f;

    // Projected extents of the unit-scale cube (corners at +/-h).
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (int corner = 0; corner < 8; ++corner) {
        const float cx = (corner & 1) ? h : -h;
        const float cy = (corner & 2) ? h : -h;
        const float cz = (corner & 4) ? h : -h;
        float sx = 0.0f, sy = 0.0f;
        ui::projectPoint(view, cx, cy, cz, sx, sy);
        minX = std::min(minX, sx);
        maxX = std::max(maxX, sx);
        minY = std::min(minY, sy);
        maxY = std::max(maxY, sy);
    }
    const float extentW = std::max(1.0f, maxX - minX);
    const float extentH = std::max(1.0f, maxY - minY);
    const float fit =
        std::min(innerW / extentW, innerH / extentH) *
        static_cast<float>(config.previewScale);
    view.scale = std::max(1.0f, fit);
    view.centerX = innerX + innerW * 0.5f;
    view.centerY = innerY + innerH * 0.5f;

    // Share the exact transform with the touch picker.
    studio.setView(view);

    const auto basis = ui::basisFor(view);

    // Gather visible faces, painter-sorted back to front.
    struct VisibleFace {
        float depth;
        float corners[4][2];
        std::uint32_t color;
    };
    std::vector<VisibleFace> visible;
    visible.reserve(surface.size() * 2);

    for (const auto voxelIndex : surface) {
        const int x = voxelIndex % n;
        const int z = (voxelIndex / n) % n;
        const int y = voxelIndex / (n * n);
        const auto value = structure.get(x, y, z);
        if (value == 0) {
            continue;
        }
        const auto baseColor = voxel::paletteColor(value - 1);

        FaceQuad quads[6];
        facesForVoxel(static_cast<float>(x), static_cast<float>(y),
                      static_cast<float>(z), quads);

        for (const auto &quad : quads) {
            const float facing = quad.nx * basis.forward.x +
                                 quad.ny * basis.forward.y +
                                 quad.nz * basis.forward.z;
            if (facing <= 0.02f) {
                continue;
            }

            VisibleFace face;
            face.depth = 0.0f;
            for (int i = 0; i < 4; ++i) {
                const float wx = quad.corners[i][0] + 0.0f;
                const float wy = quad.corners[i][1];
                const float wz = quad.corners[i][2];
                float sx = 0.0f, sy = 0.0f;
                ui::projectPoint(view, wx, wy, wz, sx, sy);
                face.corners[i][0] = sx;
                face.corners[i][1] = sy;
                face.depth += ui::depthOf(basis, wx, wy, wz) * 0.25f;
            }
            face.color = shadeColor(baseColor, quad.shade);
            visible.push_back(face);
        }
    }

    std::sort(visible.begin(), visible.end(),
              [](const VisibleFace &a, const VisibleFace &b) {
                  return a.depth < b.depth;
              });

    for (const auto &face : visible) {
        commands.push_back(makeTriangle(face.corners[0][0], face.corners[0][1],
                                        face.corners[1][0], face.corners[1][1],
                                        face.corners[2][0], face.corners[2][1],
                                        face.color));
        commands.push_back(makeTriangle(face.corners[0][0], face.corners[0][1],
                                        face.corners[2][0], face.corners[2][1],
                                        face.corners[3][0], face.corners[3][1],
                                        face.color));
    }

    // Box/sphere anchor highlight: wireframe around the anchor cell.
    if (studio.hasBoxAnchor()) {
        const int ax = studio.boxAnchorX();
        const int ay = studio.boxAnchorY();
        const int az = studio.boxAnchorZ();
        float corners[8][2];
        for (int i = 0; i < 8; ++i) {
            float sx = 0.0f, sy = 0.0f;
            ui::projectPoint(view, ax + ((i & 1) ? 1.0f : 0.0f),
                             ay + ((i & 2) ? 1.0f : 0.0f),
                             az + ((i & 4) ? 1.0f : 0.0f), sx, sy);
            corners[i][0] = sx;
            corners[i][1] = sy;
        }
        static const int kEdges[12][2] = {
            {0, 1}, {2, 3}, {4, 5}, {6, 7}, // along x
            {0, 2}, {1, 3}, {4, 6}, {5, 7}, // along y
            {0, 4}, {1, 5}, {2, 6}, {3, 7}, // along z
        };
        for (const auto &edge : kEdges) {
            commands.push_back(makeLine(corners[edge[0]][0],
                                        corners[edge[0]][1],
                                        corners[edge[1]][0],
                                        corners[edge[1]][1], 3.0f, accent));
        }
    }
}

void HudPresenter::buildCommands(
    const ui::StudioLayout &layout,
    std::vector<pl::modmenu::DrawCommand> &commands) {
    auto &studio = VoxelStudio::instance();
    const auto config = studio.config();
    const auto accent = accentColorOrDefault(config.accentColor);
    const auto panelBg = 0xC0101418u;
    const auto textMain = 0xFFF2F5F7u;
    const auto textDim = 0xFF9AA7B0u;

    // Panel background + accent frame.
    commands.push_back(makeRectFilled(layout.canvasX, layout.canvasY,
                                      layout.canvasW, layout.canvasH,
                                      panelBg));
    const float e = 2.0f * static_cast<float>(config.panelScale);
    const float x0 = layout.canvasX;
    const float y0 = layout.canvasY;
    const float x1 = layout.canvasX + layout.canvasW;
    const float y1 = layout.canvasY + layout.canvasH;
    commands.push_back(makeRectFilled(x0, y0, layout.canvasW, e, accent));
    commands.push_back(makeRectFilled(x0, y1 - e, layout.canvasW, e, accent));
    commands.push_back(makeRectFilled(x0, y0, e, layout.canvasH, accent));
    commands.push_back(makeRectFilled(x1 - e, y0, e, layout.canvasH, accent));

    if (config.showStats) {
        const auto status = studio.status();
        float textY = layout.canvasY - layout.textSize * 2.4f;
        if (textY < layout.padding) {
            textY = layout.padding;
        }
        const float lx = layout.canvasX + 4.0f;
        commands.push_back(makeText(lx, textY, layout.textSize, textMain,
                                    status.tool + "  |  " + status.hint));
        commands.push_back(makeText(
            lx, textY + layout.textSize * 1.15f, layout.textSize * 0.92f,
            textDim,
            status.grid + "  |  " + status.voxels + "  |  " + status.material));
        commands.push_back(
            makeText(lx, textY + layout.textSize * 2.2f,
                     layout.textSize * 0.85f, textDim,
                     status.angle + "  |  " + status.slot + "  |  " +
                         status.clipboard));
    }
}

void HudPresenter::submit() {
    auto &studio = VoxelStudio::instance();
    const auto config = studio.config();

    std::vector<pl::modmenu::DrawCommand> commands;
    if (config.showPanel) {
        const auto layout =
            ui::computeLayout(studio.screenWidth(), studio.screenHeight(),
                              config.panelScale);
        commands.reserve(512);
        buildCommands(layout, commands);

        const int used =
            static_cast<int>(commands.size()) + kCommandReserve;
        const int budget =
            std::max(0, config.maxDrawCommands - used);
        appendPreview(layout, commands, budget);
    }

    pl::modmenu::submitDrawCommands("levi-create.studio", commands);
}

void refreshStudioHud() {
    static HudPresenter presenter;
    presenter.submit();
}

} // namespace levicreate
