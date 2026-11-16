# libSceIcu

## add Functional

- [Default locale](../../../core/libs/prx/libSceIcu/DefaultLocale.cpp) - the PS5 default locale is not measured. Until the guest sets one, `uprv_getDefaultLocaleID` and `uloc_getDefault` follow the POSIX branch of ICU 68.2 [putil.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/putil.cpp) and [locid.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/locid.cpp): guest `setlocale(6, NULL)`, then `LC_ALL`, `LC_MESSAGES`, `LANG`, else `en_US_POSIX`, read once. Unverified: that the console build takes that branch, `LC_MESSAGES` = 6 (FreeBSD) and the console environment. The ID goes through this module's `uloc_canonicalize`, not `ulocimp_canonicalize`. `uloc_setDefault` with an ID sets ICU 68.2's default, which `uloc_getDefault` then returns until a NULL ID or `u_cleanup` restores the POSIX default. Closes when: compared with the console.
