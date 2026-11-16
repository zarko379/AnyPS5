# libSceJsc

## add Known export names; ABI prototypes unverified

- [Deprecated::ScriptValue copy, assignment and destructors](../../../core/libs/prx/libSceJsc/src/DeprecatedScriptValue.cpp) (libSceJsc) - not compared with a console module; the version is unverified, and the handle slot is the 8 bytes at offset 0. They follow the `Strong<Unknown>` member of pinned [ScriptValue.h](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/JavaScriptCore/bindings/ScriptValue.h#L47-L57), [Strong.h](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/JavaScriptCore/heap/Strong.h#L54-L134) and [HandleSet.h](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/JavaScriptCore/heap/HandleSet.h#L123-L139) on the `HandleSet` layout of `HandleSet::writeBarrier`. A new handle comes from the free list of the source's set, deduced to be `vm.heap.handleSet()` because a `Heap` owns one set. An empty free list needs `HandleSet::grow` and `fastAlignedMalloc` and throws. Null pointers throw where WebKit faults.

## remove | `_ZN10Deprecated11ScriptValueC1ERKS0_` | `ykyWnXSOang` |

## remove | `_ZN10Deprecated11ScriptValueC2ERKS0_` | `8JDGfjB7ysY` |

## remove | `_ZN10Deprecated11ScriptValueD1Ev` | `ale+MG3sPCU` |

## remove | `_ZN10Deprecated11ScriptValueD2Ev` | `eRd+WZ6MHqY` |

## remove | `_ZN10Deprecated11ScriptValueaSERKS0_` | `35ORvn0jTS8` |
