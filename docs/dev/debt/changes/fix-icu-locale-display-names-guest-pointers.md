# libSceIcu

## add Functional

- [uldn_openForContext](../../../core/libs/prx/libSceIcu/TextFormat.cpp) reads guest `UErrorCode* status` values via `GuestCopyIn_nid_no_patch` and writes ICU 68.2's changes back via `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting unreadable or unwritable status pointers via `NotImplemented_nid_no_patch(__func__)`.
