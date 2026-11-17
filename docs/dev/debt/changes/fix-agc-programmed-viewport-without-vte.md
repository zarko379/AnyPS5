# libSceAgcDriver

## add Functional

- [Programmed viewport with PA_CL_VTE_CNTL 0](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver) - a draw whose `PA_CL_VTE_CNTL` is still 0 but whose viewport scale and offset (context `0x10f`-`0x112`) are finite with a positive X scale and a nonzero Y scale is decoded as with `0x43f`, Y sign included, and Z, scale and depth clamp come from `0x113`-`0x114` and `0xb4`-`0xb5` when programmed; a programmed Z scale of 0 gives every fragment the Z offset as its depth, as with `0x43f`. Deduced from Grand Theft Auto V (PPSA04264), which never writes `PA_CL_VTE_CNTL` before its first draws yet programs a Y-up viewport for every target and renders clip-space positions, and from libSceAgc's public register-default tables, which use `0x43f`; not measured on a console. A draw with no programmed viewport keeps the full-target viewport.
