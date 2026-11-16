# libSceIcu

## add Functional

- [ufmtval_nextPosition](../../../core/libs/prx/libSceIcu/FormattedValue.cpp) and [ufmtval_getString](../../../core/libs/prx/libSceIcu/TextFormat.cpp) read and update guest `UErrorCode* ec` pointers via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting unreadable or unwritable error pointers via `NotImplemented_nid_no_patch(__func__)`.
