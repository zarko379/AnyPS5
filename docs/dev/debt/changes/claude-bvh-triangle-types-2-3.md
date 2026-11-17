# shader

## add Functional

- [BVH triangle node types 2 and 3](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) - `image_bvh_intersect_ray` and `image_bvh64_intersect_ray` test node types 2 and 3 as the triangles (v2, v3, v4) and (v2, v4, v0) of the five-vertex triangle node, the mapping AMD's GPURT gives for triangle compression ([Common.hlsl](https://github.com/GPUOpen-Drivers/gpurt/blob/7b226d48b46b7e92fec3b9ecc5712e5bf2bf3dd9/src/shadersClean/common/Common.hlsl#L566-L588)), whose own node test handles only types 0 and 1. Deduced from that source and from title BVHs (every type 2 and 3 triangle overlaps its leaf box with exactly these vertex sets), not measured on a console: the vertex order inside each triangle, and so the sign of the denominator, the barycentric numerators and the triangle_id swizzle of types 2 and 3, follow GPURT and types 0 and 1 by assumption.
