#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Jsc::JscAbi {

struct FreeCell {
    std::uint64_t preservedBitsForCrashAnalysis;
    std::uintptr_t scrambledNext;
};

struct FreeList {
    std::uintptr_t scrambledHead;
    std::uintptr_t secret;
    char* payloadEnd;
    unsigned remaining;
    unsigned originalSize;
    unsigned cellSize;
};

struct HashTable {
    int numberOfValues;
    int indexMask;
    bool hasSetterOrReadonlyProperties;
    const void* classForThis;
    const void* values;
    const void* index;
};

struct ClassInfo {
    const char* className;
    const ClassInfo* parentClass;
    const HashTable* staticPropHashTable;
};

enum class CallDataType : std::uint8_t { None, Native, JS };

struct CallData {
    CallDataType type;
    std::uintptr_t first;
    std::uintptr_t second;
};

struct InternalFunction {
    std::uint64_t cellHeader;
    const void* butterfly;
    std::uintptr_t functionForCall;
    std::uintptr_t functionForConstruct;
    const void* originalName;
    const void* globalObject;
};

struct YarrRegularExpressionPrivate {
    unsigned refCount;
    std::uint8_t constructionErrorCode;
    void* regexAllocatorHead;
    int lastMatchLength;
    unsigned numSubpatterns;
    void* byteCode;
};

struct YarrRegularExpression {
    YarrRegularExpressionPrivate* d;
};

static_assert(sizeof(FreeCell) == 16);
static_assert(offsetof(FreeCell, scrambledNext) == 8);
static_assert(sizeof(FreeList) == 40);
static_assert(alignof(FreeList) == 8);
static_assert(offsetof(FreeList, scrambledHead) == 0);
static_assert(offsetof(FreeList, secret) == 8);
static_assert(offsetof(FreeList, payloadEnd) == 16);
static_assert(offsetof(FreeList, remaining) == 24);
static_assert(offsetof(FreeList, originalSize) == 28);
static_assert(offsetof(FreeList, cellSize) == 32);
static_assert(offsetof(HashTable, numberOfValues) == 0);
static_assert(offsetof(HashTable, indexMask) == 4);
static_assert(offsetof(HashTable, hasSetterOrReadonlyProperties) == 8);
static_assert(offsetof(HashTable, classForThis) == 16);
static_assert(offsetof(ClassInfo, className) == 0);
static_assert(offsetof(ClassInfo, parentClass) == 8);
static_assert(offsetof(ClassInfo, staticPropHashTable) == 16);
static_assert(sizeof(CallData) == 24);
static_assert(offsetof(CallData, type) == 0);
static_assert(offsetof(CallData, first) == 8);
static_assert(offsetof(CallData, second) == 16);
static_assert(std::is_trivially_copyable_v<CallData>);
static_assert(sizeof(InternalFunction) == 48);
static_assert(offsetof(InternalFunction, functionForCall) == 16);
static_assert(offsetof(InternalFunction, functionForConstruct) == 24);
static_assert(sizeof(YarrRegularExpressionPrivate) == 32);
static_assert(offsetof(YarrRegularExpressionPrivate, constructionErrorCode) == 4);
static_assert(offsetof(YarrRegularExpressionPrivate, regexAllocatorHead) == 8);
static_assert(offsetof(YarrRegularExpressionPrivate, lastMatchLength) == 16);
static_assert(offsetof(YarrRegularExpressionPrivate, numSubpatterns) == 20);
static_assert(offsetof(YarrRegularExpressionPrivate, byteCode) == 24);
static_assert(sizeof(YarrRegularExpression) == 8);

} // namespace Jsc::JscAbi

extern "C" {

void APS5_VABI _ZN3JSC8FreeList14initializeBumpEPcj(Jsc::JscAbi::FreeList* self, char* payloadEnd, unsigned remaining);
void APS5_VABI _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(Jsc::JscAbi::FreeList* self, Jsc::JscAbi::FreeCell* head,
                                                                  std::uintptr_t secret, unsigned bytes);
bool APS5_VABI _ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(const Jsc::JscAbi::ClassInfo* self);
Jsc::JscAbi::CallData APS5_VABI _ZN3JSC6JSCell11getCallDataEPS0_(void* cell);
Jsc::JscAbi::CallData APS5_VABI _ZN3JSC6JSCell16getConstructDataEPS0_(void* cell);
Jsc::JscAbi::CallData APS5_VABI _ZN3JSC16InternalFunction11getCallDataEPNS_6JSCellE(Jsc::JscAbi::InternalFunction* cell);
Jsc::JscAbi::CallData APS5_VABI _ZN3JSC16InternalFunction16getConstructDataEPNS_6JSCellE(Jsc::JscAbi::InternalFunction* cell);
void APS5_VABI _ZN3JSC6JSCell7destroyEPS0_(void* cell);
void APS5_VABI _ZN3JSC6JSCell11analyzeHeapEPS0_RNS_12HeapAnalyzerE(void* cell, void* analyzer);
void APS5_VABI _ZN3JSC7JSProxy25getStructurePropertyNamesEPNS_8JSObjectEPNS_14JSGlobalObjectERNS_17PropertyNameArrayENS_15EnumerationModeE(
    void* object, void* globalObject, void* propertyNames, std::uint64_t mode);
bool APS5_VABI _ZNK3JSC4Yarr17RegularExpression7isValidEv(const Jsc::JscAbi::YarrRegularExpression* self);
std::uint64_t APS5_VABI _ZN3JSC20speculationFromValueENS_7JSValueE(std::uint64_t value);
std::uint64_t APS5_VABI _ZN3JSC8JSObject6toThisEPNS_6JSCellEPNS_14JSGlobalObjectENS_8ECMAModeE(void* cell, void* globalObject, std::uint8_t ecmaMode);
std::uint64_t APS5_VABI _ZN3JSC7JSScope6toThisEPNS_6JSCellEPNS_14JSGlobalObjectENS_8ECMAModeE(void* cell, void* globalObject, std::uint8_t ecmaMode);
std::uint64_t APS5_VABI _ZNK3JSC7JSValue14toThisSlowCaseEPNS_14JSGlobalObjectENS_8ECMAModeE(const std::uint64_t* self, void* globalObject, std::uint8_t ecmaMode);

}
