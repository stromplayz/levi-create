#pragma once

#include "mod/voxel/VoxelStructure.h"

namespace levicreate::voxel::transform {

// Rotates the structure around the vertical (Y) axis by an arbitrary angle,
// re-rasterizing voxel centers with nearest-neighbor sampling. This is the
// "any degree" rotation (Axiom style); 90/180/270 are exact.
void rotateYDeg(VoxelStructure &structure, double degrees);

// Exact 90-degree rotations around the horizontal axes.
void rotateX90(VoxelStructure &structure);
void rotateZ90(VoxelStructure &structure);

// Mirror along an axis (Axiom-style flip).
void mirrorX(VoxelStructure &structure);
void mirrorZ(VoxelStructure &structure);

// Scales by a factor (0.25 - 4.0) with nearest-neighbor resampling and a
// clamped grid size. Returns the applied factor.
double scaleBy(VoxelStructure &structure, double factor);

// Moves all voxels by the given offsets; voxels pushed out of the grid are
// dropped (clamped translate).
void translate(VoxelStructure &structure, int dx, int dy, int dz);

// Reduces the grid resolution by the given integer factor (LOD helper).
VoxelStructure downsampled(const VoxelStructure &structure, int factor);

} // namespace levicreate::voxel::transform
