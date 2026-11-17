# libSceAgcDriver

## add Functional

- [Store over untracked tests without the rewatch under images on Windows](../../../core/libs/prx/libSceAgcDriver/tests/execution/StoreOverUntracked.cpp) (libSceAgcDriver) - on Windows the store over untracked test binary sets `APS5_NO_REWATCH_UNDER_IMAGES=1`, so its cases do not run the default path where a settled host import under a live storage image returns to the write watch. With that path the import lookup inside the surface's own write-back can return it to the watch at a time the test cannot control: the write-back then saves no guest bytes, and the second store over a surface outside the write watch no longer runs outside it. Remove the switch once the cases can hold their surface outside the watch with the rewatch enabled.
