#include "mod/ui/Projection.h"

#include <algorithm>
#include <cmath>

namespace levicreate::ui {

namespace {

constexpr float kPi = 3.14159265358979323846f;

float degToRad(float degrees) { return degrees * kPi / 180.0f; }

} // namespace

ViewBasis basisFor(const ViewTransform &view) {
    const float yaw = degToRad(view.yawDeg);
    const float pitch = degToRad(std::clamp(view.pitchDeg, -80.0f, 80.0f));

    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);

    ViewBasis basis;
    // Camera looks toward -forward; positive pitch places the viewer above.
    basis.right = {cy, 0.0f, -sy};
    basis.up = {-sy * sp, cp, -cy * sp};
    basis.forward = {sy * cp, sp, cy * cp};
    return basis;
}

void projectPoint(const ViewTransform &view, float x, float y, float z,
                  float &outX, float &outY) {
    const ViewBasis basis = basisFor(view);
    const float c = (view.size - 1) / 2.0f;
    const float dx = x - c;
    const float dy = y - c;
    const float dz = z - c;

    outX = view.centerX +
           (dx * basis.right.x + dy * basis.right.y + dz * basis.right.z) *
               view.scale;
    outY = view.centerY -
           (dx * basis.up.x + dy * basis.up.y + dz * basis.up.z) * view.scale;
}

PickRay rayThroughPixel(const ViewTransform &view, float px, float py) {
    const ViewBasis basis = basisFor(view);
    const float dx = (px - view.centerX) / view.scale;
    const float dy = (view.centerY - py) / view.scale;

    // dir = forward + dx*right + dy*up  (orthographic ray direction).
    PickRay ray;
    ray.dir = {
        basis.forward.x + dx * basis.right.x + dy * basis.up.x,
        basis.forward.y + dx * basis.right.y + dy * basis.up.y,
        basis.forward.z + dx * basis.right.z + dy * basis.up.z,
    };

    // Start far outside the grid along -dir so the DDA enters the cube.
    const float c = (view.size - 1) / 2.0f;
    const float big = static_cast<float>(view.size) * 4.0f + 8.0f;
    ray.origin = {
        c - ray.dir.x * big,
        c - ray.dir.y * big,
        c - ray.dir.z * big,
    };
    return ray;
}

float depthOf(const ViewBasis &basis, float x, float y, float z) {
    return x * basis.forward.x + y * basis.forward.y + z * basis.forward.z;
}

} // namespace levicreate::ui
