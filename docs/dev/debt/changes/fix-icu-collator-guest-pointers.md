# libSceIcu

## add Functional

- [ucol_getAttribute, ucol_getReorderCodes, ucol_getTailoredSet, ucol_open, and ucol_setAttribute](../../../core/libs/prx/libSceIcu/Collation.cpp) read guest `UErrorCode* status` values via `GuestCopyIn_nid_no_patch` and write ICU 68.2's changes back via `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting unreadable or unwritable status pointers via `NotImplemented_nid_no_patch(__func__)`.
