# libSceAgcDriver

## add Functional

- [Guest BVH decoder](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestBvh.cpp) - reads `PSR_BVHL` top and bottom structures into instances and triangles. The header fields, the 8-byte leaf records (bit 31 ends a list, bit 30 opaque) and the 128-byte instance record were deduced from one title's ray-traced capture, not measured on a console. That title's executable contains the shaders that build them, so the format may belong to that title alone. Any other format word, size or pointer type is refused. A child of node type 6 or 7 is skipped because the emulated node test reports it invalid. What the instance flags and word 29 mean is not known.
