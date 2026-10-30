> **ℹ️ v0.2.0 format note:** the shipped model format is now the JSON described at the bottom
> of this file (see also the README). `.lcmodel` was the v0.1.0 proposal; `VoxelModel::toJson()`
> in `src/core/VoxelModel.cpp` is the authoritative serializer in v0.2.0.

# The `.lcmodel` Model Format

LeviCreate's saved voxel models (used by the model library, the clipboard
export path, and future importers) are UTF-8 JSON files stored in
`<dataDir>/models/<sanitized-name>.lcmodel`.

## Example

```json
{
    "format": "levi-create.lcmodel",
    "version": 1,
    "name": "my-tower",
    "size": [8, 16, 8],
    "palette": ["minecraft:stone", "minecraft:oak_planks"],
    "voxels": "rle-base64:AAAAAQQAAQIAAQ..."
}
```

## Fields

| Field     | Type             | Meaning                                                        |
|-----------|------------------|----------------------------------------------------------------|
| `format`  | string           | Must be `levi-create.lcmodel` (readers reject other values).   |
| `version` | int              | Format version. Currently always `1`.                          |
| `name`    | string, optional | Display name (sanitized file stem at save time).               |
| `size`    | `[sx, sy, sz]`   | Model dimensions in voxels. All `>= 0`.                        |
| `palette` | `[string]`       | Block ids, normalized to `minecraft:`-prefixed lower case.     |
| `voxels`  | string           | `rle-base64:` + base64 of the run-length encoded voxel array.  |

## Voxel values

The decoded array has `sx * sy * sz` entries of 16 bits, row-major with
`index = (y * sz + z) * sx + x` (y is up, x is east, z is south — matching
Bedrock `BlockPos`).

| Value                | Meaning                                  |
|----------------------|------------------------------------------|
| `0`                  | Air (stamps as `minecraft:air` unless the paste keeps air). |
| `1 .. palette.size()`| The block `palette[value - 1]`.          |
| `0xFFFF`             | Reserved "keep existing world block" for overlay stamps. |

## RLE encoding

The byte stream before base64 is a sequence of pairs, little-endian:

```
[u32 run_length][u16 voxel_value]
```

`run_length` counts consecutive occurrences of `voxel_value`. Runs never
cross the total voxel count; decoders must clamp and treat truncated streams
as errors (the reference decoder in `core/VoxelModel.cpp` does both).

## Reference implementation

Encoder + decoder: `src/levi_create/core/VoxelModel.cpp`
(`toJson` / `fromJson`). Round-trip and corruption behavior are covered by
the engine test-suite in `docs/feature-voxel-engine.md`.
