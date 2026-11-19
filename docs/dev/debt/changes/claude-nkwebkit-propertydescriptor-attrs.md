# libSceNKWebKit

## add Functional

- [JSC::PropertyDescriptor](../../../core/libs/prx/libSceNKWebKit/src/PropertyDescriptor.cpp) (libSceNKWebKit) - the 32-byte layout in [PropertyDescriptorAbi.hpp](../../../core/libs/prx/libSceNKWebKit/src/PropertyDescriptorAbi.hpp) follows [PropertyDescriptor.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/PropertyDescriptor.h) and is unverified against the PS5 module. `JSValue` arguments are taken as their encoded 64-bit word. `setDescriptor` and `setAccessorDescriptor` still throw `NotImplemented_nid_no_patch`.
