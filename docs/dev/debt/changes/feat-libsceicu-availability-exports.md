# libSceIcu

## add Functional

- [Locale availability, locale name and string hash exports](../../../core/libs/prx/libSceIcu/Availability.cpp) (libSceIcu) - they call ICU 78.1 through its C entry points and pass the guest pointers to it unchanged, except that a null locale given to `uloc_getName` becomes this library's default locale instead of the host's, and report whatever `UErrorCode` ICU writes into the guest's status. `uloc_countAvailable`, `uloc_getAvailable`, `uloc_getName`, `ustr_hashUCharsN`, `ustr_hashCharsN` and `ustr_hashICharsN` are data-free, and the new test covers them; `udata_setCommonData` loads data into ICU 78.1 only, not into the ICU 68.2 backend. An unwritable guest pointer makes the wrapper return the function's failure value instead of faulting: the console faults there, which is not measured.
