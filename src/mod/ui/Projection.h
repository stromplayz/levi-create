#pragma once

namespace levicreate::ui {

// Isometric orbit projection used by both the HUD preview and touch picking.
// Structure space: x/east, y/up, z/south; cube [0, size]^3.
struct ViewTransform {
    float yawDeg = 45.0f;   // rotation around the vertical (Y) axis
    float pitchDeg = 28.0f; // tilt, clamped to [-80, 80]
    float scale = 1.0f;     // pixels per voxel
    float centerX = 0.0f;   // screen position of the structure center
    float centerY = 0.0f;
    int size = 16;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Orthonormal view basis (structure-space unit vectors).
struct ViewBasis {
    Vec3 right;   // +screen x
    Vec3 up;      // -screen y (screen y grows downward)
    Vec3 forward; // toward the viewer
};

[[nodiscard]] ViewBasis basisFor(const ViewTransform &view);

// Projects a structure-space point to screen coordinates.
void projectPoint(const ViewTransform &view, float x, float y, float z,
                  float &outX, float &outY);

// Ray through a screen pixel, expressed in structure space. The orthographic
// ray origin is placed so callers can DDA from the grid entry point.
struct PickRay {
    Vec3 origin;
    Vec3 dir;
};
[[nodiscard]] PickRay rayThroughPixel(const ViewTransform &view, float px,
                                      float py);

// Depth key for painter's algorithm sorting (larger = closer to viewer).
[[nodiscard]] float depthOf(const ViewBasis &basis, float x, float y, float z);

} // namespace levicreate::ui
