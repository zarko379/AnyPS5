#include "prx/libSceIcu/CoreClasses.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libc/include/General.hpp"
#include "patternprops.h"
#include "umutex.h"
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

template<typename TObject>
TObject* Object(TObject* object) {
    if (!object) APS5_INVALID_ARG_EX;
    return object;
}

char At(const Icu::StringPiece& piece, std::int32_t index) {
    if (!piece.data) APS5_INVALID_ARG_EX;
    return piece.data[index];
}

void Construct(Icu::StringPiece* self, const char* text) {
    Object(self);
    self->data = text;
    self->length = text ? static_cast<std::int32_t>(std::strlen(text)) : 0;
}

void ConstructSuffix(Icu::StringPiece* self, const Icu::StringPiece* other, std::int32_t position) {
    const Icu::StringPiece source = *Object(other);
    Object(self);
    if (position < 0) position = 0;
    else if (position > source.length) position = source.length;
    self->data = source.data + position;
    self->length = source.length - position;
}

void ConstructRange(Icu::StringPiece* self, const Icu::StringPiece* other, std::int32_t position, std::int32_t length) {
    const Icu::StringPiece source = *Object(other);
    Object(self);
    if (position < 0) position = 0;
    else if (position > source.length) position = source.length;
    if (length < 0) length = 0;
    else if (length > source.length - position) length = source.length - position;
    self->data = source.data + position;
    self->length = length;
}

const char16_t* Text(const char16_t* text, std::int32_t length) {
    if (length > 0 && !text) APS5_INVALID_ARG_EX;
    return text;
}

icu::UInitOnce& Once(Icu::InitOnce* once) {
    static_assert(sizeof(icu::UInitOnce) == sizeof(Icu::InitOnce));
    return *reinterpret_cast<icu::UInitOnce*>(Object(once));
}

} // namespace

extern "C" {

void* APS5_VABI _ZN6icu_687UMemorynwEm_nid_postfix(std::size_t size) {
    return uprv_malloc_nid_postfix(size);
}

void* APS5_VABI _ZN6icu_687UMemorynaEm_nid_postfix(std::size_t size) {
    return uprv_malloc_nid_postfix(size);
}

void APS5_VABI _ZN6icu_687UMemorydlEPv_nid_postfix(void* memory) {
    if (memory) IcuReleaseMemory_nid_no_patch(memory);
}

void APS5_VABI _ZN6icu_687UMemorydaEPv_nid_postfix(void* memory) {
    if (memory) IcuReleaseMemory_nid_no_patch(memory);
}

void APS5_VABI _ZN6icu_687UObjectD0Ev_nid_postfix(void* self) {
    _ZN6icu_687UMemorydlEPv_nid_postfix(Object(self));
}

void APS5_VABI _ZN6icu_687UObjectD1Ev_nid_postfix(void* self) {
    Object(self);
}

void APS5_VABI _ZN6icu_687UObjectD2Ev_nid_postfix(void* self) {
    Object(self);
}

const void* APS5_VABI _ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix(const void* self) {
    Object(self);
    return nullptr;
}

void APS5_VABI _ZN6icu_6811StringPieceC1EPKc_nid_postfix(Icu::StringPiece* self, const char* text) {
    Construct(self, text);
}

void APS5_VABI _ZN6icu_6811StringPieceC2EPKc_nid_postfix(Icu::StringPiece* self, const char* text) {
    Construct(self, text);
}

void APS5_VABI _ZN6icu_6811StringPieceC1ERKS0_i_nid_postfix(Icu::StringPiece* self, const Icu::StringPiece* other, std::int32_t position) {
    ConstructSuffix(self, other, position);
}

void APS5_VABI _ZN6icu_6811StringPieceC2ERKS0_i_nid_postfix(Icu::StringPiece* self, const Icu::StringPiece* other, std::int32_t position) {
    ConstructSuffix(self, other, position);
}

void APS5_VABI _ZN6icu_6811StringPieceC1ERKS0_ii_nid_postfix(Icu::StringPiece* self, const Icu::StringPiece* other, std::int32_t position,
                                                            std::int32_t length) {
    ConstructRange(self, other, position, length);
}

void APS5_VABI _ZN6icu_6811StringPieceC2ERKS0_ii_nid_postfix(Icu::StringPiece* self, const Icu::StringPiece* other, std::int32_t position,
                                                            std::int32_t length) {
    ConstructRange(self, other, position, length);
}

void APS5_VABI _ZN6icu_6811StringPiece3setEPKc_nid_postfix(Icu::StringPiece* self, const char* text) {
    Construct(self, text);
}

std::int32_t APS5_VABI _ZN6icu_6811StringPiece4findES0_i_nid_postfix(Icu::StringPiece* self, Icu::StringPiece needle, std::int32_t offset) {
    const Icu::StringPiece haystack = *Object(self);
    if (haystack.length == 0 && needle.length == 0) return 0;
    for (std::int32_t i = offset; i < haystack.length; i++) {
        std::int32_t j = 0;
        bool matched = true;
        for (; j < needle.length; i++, j++) {
            if (At(haystack, i) != At(needle, j)) {
                i -= j;
                matched = false;
                break;
            }
        }
        if (matched) return i - j;
    }
    return -1;
}

std::int32_t APS5_VABI _ZN6icu_6811StringPiece7compareES0__nid_postfix(Icu::StringPiece* self, Icu::StringPiece other) {
    const Icu::StringPiece piece = *Object(self);
    std::int32_t i = 0;
    for (; i < piece.length; i++) {
        if (i == other.length) return 1;
        const char a = At(piece, i);
        const char b = At(other, i);
        if (a < b) return -1;
        if (a > b) return 1;
    }
    if (i < other.length) return -1;
    return 0;
}

std::int8_t APS5_VABI _ZN6icu_68eqERKNS_11StringPieceES2__nid_postfix(const Icu::StringPiece* x, const Icu::StringPiece* y) {
    const Icu::StringPiece left = *Object(x);
    const Icu::StringPiece right = *Object(y);
    std::int32_t length = left.length;
    if (length != right.length) return 0;
    if (length == 0) return 1;
    --length;
    if (At(left, length) != At(right, length)) return 0;
    return std::memcmp(left.data, right.data, static_cast<std::size_t>(length)) == 0;
}

std::int8_t APS5_VABI _ZN6icu_6812PatternProps8isSyntaxEi_nid_postfix(std::int32_t c) {
    return icu::PatternProps::isSyntax(c);
}

std::int8_t APS5_VABI _ZN6icu_6812PatternProps20isSyntaxOrWhiteSpaceEi_nid_postfix(std::int32_t c) {
    return icu::PatternProps::isSyntaxOrWhiteSpace(c);
}

std::int8_t APS5_VABI _ZN6icu_6812PatternProps12isWhiteSpaceEi_nid_postfix(std::int32_t c) {
    return icu::PatternProps::isWhiteSpace(c);
}

const char16_t* APS5_VABI _ZN6icu_6812PatternProps14skipWhiteSpaceEPKDsi_nid_postfix(const char16_t* text, std::int32_t length) {
    return icu::PatternProps::skipWhiteSpace(Text(text, length), length);
}

const char16_t* APS5_VABI _ZN6icu_6812PatternProps14trimWhiteSpaceEPKDsRi_nid_postfix(const char16_t* text, std::int32_t* length) {
    return icu::PatternProps::trimWhiteSpace(Text(text, *Object(length)), *length);
}

std::int8_t APS5_VABI _ZN6icu_6812PatternProps12isIdentifierEPKDsi_nid_postfix(const char16_t* text, std::int32_t length) {
    return icu::PatternProps::isIdentifier(Text(text, length), length);
}

const char16_t* APS5_VABI _ZN6icu_6812PatternProps14skipIdentifierEPKDsi_nid_postfix(const char16_t* text, std::int32_t length) {
    return icu::PatternProps::skipIdentifier(Text(text, length), length);
}

std::int8_t APS5_VABI _ZN6icu_6820umtx_initImplPreInitERNS_9UInitOnceE_nid_postfix(Icu::InitOnce* once) {
    return icu::umtx_initImplPreInit(Once(once));
}

void APS5_VABI _ZN6icu_6821umtx_initImplPostInitERNS_9UInitOnceE_nid_postfix(Icu::InitOnce* once) {
    icu::umtx_initImplPostInit(Once(once));
}

std::int8_t APS5_VABI uprv_pathIsAbsolute_nid_postfix(const char* path) {
    if (!path || !*path) return 0;
    return *path == '/';
}

}
