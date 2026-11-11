#include "prx/libSceIcu/Text.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/ubrk.h>
#include <unicode/utext.h>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::int32_t OpenFlag = 4;

struct Providers {
    const UTextFuncs* chars;
    const UTextFuncs* utf8;
};

const Providers& Known() {
    static const Providers providers = [] {
        UErrorCode status = U_ZERO_ERROR;
        UText chars = UTEXT_INITIALIZER;
        UText utf8 = UTEXT_INITIALIZER;
        utext_openUChars(&chars, u"", 0, &status);
        utext_openUTF8(&utf8, "", 0, &status);
        const Providers result{chars.pFuncs, utf8.pFuncs};
        utext_close(&chars);
        utext_close(&utf8);
        if (U_FAILURE(status) || result.chars == nullptr || result.utf8 == nullptr) throw std::logic_error("ICU 68.2 UText providers are unavailable");
        return result;
    }();
    return providers;
}

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

void Provider(const UText* text) {
    if (text->magic != UTEXT_MAGIC || (text->flags & OpenFlag) == 0) return;
    if (text->pFuncs == nullptr) NotImplemented_nid_no_patch("UText: an open text without a provider function table");
    if (text->pFuncs != Known().chars && text->pFuncs != Known().utf8) NotImplemented_nid_no_patch("UText: a provider function table supplied by the guest");
}

template<typename TText>
TText* Open(TText* text) {
    IcuBackend::Ensure();
    IcuBackend::RequireObject(text);
    if (text->magic != UTEXT_MAGIC || (text->flags & OpenFlag) == 0) APS5_INVALID_ARG_EX;
    Provider(text);
    return text;
}

template<typename TText>
TText* Readable(TText* text) {
    IcuBackend::Ensure();
    IcuBackend::RequireObject(text);
    return text;
}

bool Opens(const void* s, std::int64_t length) {
    return (s != nullptr || length == 0) && length >= -1 && length <= std::numeric_limits<std::int32_t>::max();
}

bool Writable(const UText* text) {
    return (text->providerProperties & (1 << UTEXT_PROVIDER_WRITABLE)) != 0;
}

}

extern "C" {

UText* APS5_VABI utext_openUChars_nid_postfix(UText* ut, const char16_t* s, std::int64_t length, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error) && ut != nullptr && Opens(s, length)) Provider(ut);
    return utext_openUChars(ut, s, length, error);
}

UText* APS5_VABI utext_openUTF8_nid_postfix(UText* ut, const char* s, std::int64_t length, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error) && ut != nullptr && Opens(s, length)) Provider(ut);
    return utext_openUTF8(ut, s, length, error);
}

UText* APS5_VABI utext_setup_nid_postfix(UText* ut, std::int32_t extraSpace, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_SUCCESS(*error) && ut != nullptr) Provider(ut);
    return utext_setup(ut, extraSpace, error);
}

UText* APS5_VABI utext_close_nid_postfix(UText* ut) {
    IcuBackend::Ensure();
    if (ut != nullptr) Provider(ut);
    return utext_close(ut);
}

UText* APS5_VABI utext_clone_nid_postfix(UText* dest, const UText* src, std::int8_t deep, std::int8_t readOnly, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return dest;
    Open(src);
    if (dest != nullptr) Provider(dest);
    return utext_clone(dest, src, deep, readOnly, error);
}

std::int8_t APS5_VABI utext_equals_nid_postfix(const UText* a, const UText* b) {
    IcuBackend::Ensure();
    if (a != nullptr && b != nullptr && a->magic == UTEXT_MAGIC && b->magic == UTEXT_MAGIC && a->pFuncs == b->pFuncs && a->context == b->context) {
        Open(a);
        Open(b);
    }
    return utext_equals(a, b);
}

std::int64_t APS5_VABI utext_nativeLength_nid_postfix(UText* ut) {
    return utext_nativeLength(Open(ut));
}

std::int8_t APS5_VABI utext_isLengthExpensive_nid_postfix(const UText* ut) {
    return utext_isLengthExpensive(Readable(ut));
}

std::int32_t APS5_VABI utext_char32At_nid_postfix(UText* ut, std::int64_t nativeIndex) {
    return utext_char32At(Open(ut), nativeIndex);
}

std::int32_t APS5_VABI utext_current32_nid_postfix(UText* ut) {
    return utext_current32(Open(ut));
}

std::int32_t APS5_VABI utext_next32_nid_postfix(UText* ut) {
    return utext_next32(Open(ut));
}

std::int32_t APS5_VABI utext_previous32_nid_postfix(UText* ut) {
    return utext_previous32(Open(ut));
}

std::int32_t APS5_VABI utext_next32From_nid_postfix(UText* ut, std::int64_t nativeIndex) {
    return utext_next32From(Open(ut), nativeIndex);
}

std::int32_t APS5_VABI utext_previous32From_nid_postfix(UText* ut, std::int64_t nativeIndex) {
    return utext_previous32From(Open(ut), nativeIndex);
}

std::int64_t APS5_VABI utext_getNativeIndex_nid_postfix(const UText* ut) {
    return utext_getNativeIndex(Open(ut));
}

void APS5_VABI utext_setNativeIndex_nid_postfix(UText* ut, std::int64_t nativeIndex) {
    utext_setNativeIndex(Open(ut), nativeIndex);
}

std::int8_t APS5_VABI utext_moveIndex32_nid_postfix(UText* ut, std::int32_t delta) {
    return utext_moveIndex32(Open(ut), delta);
}

std::int64_t APS5_VABI utext_getPreviousNativeIndex_nid_postfix(UText* ut) {
    return utext_getPreviousNativeIndex(Open(ut));
}

std::int32_t APS5_VABI utext_extract_nid_postfix(UText* ut, std::int64_t nativeStart, std::int64_t nativeLimit, char16_t* dest, std::int32_t destCapacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    return utext_extract(Open(ut), nativeStart, nativeLimit, dest, destCapacity, error);
}

std::int8_t APS5_VABI utext_isWritable_nid_postfix(const UText* ut) {
    return utext_isWritable(Readable(ut));
}

std::int8_t APS5_VABI utext_hasMetaData_nid_postfix(const UText* ut) {
    return utext_hasMetaData(Readable(ut));
}

std::int32_t APS5_VABI utext_replace_nid_postfix(UText* ut, std::int64_t nativeStart, std::int64_t nativeLimit, const char16_t* replacementText, std::int32_t replacementLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return 0;
    if (Writable(Open(ut))) APS5_INVALID_ARG_EX;
    return utext_replace(ut, nativeStart, nativeLimit, replacementText, replacementLength, error);
}

void APS5_VABI utext_copy_nid_postfix(UText* ut, std::int64_t nativeStart, std::int64_t nativeLimit, std::int64_t destIndex, std::int8_t move, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return;
    if (Writable(Open(ut))) APS5_INVALID_ARG_EX;
    utext_copy(ut, nativeStart, nativeLimit, destIndex, move, error);
}

void APS5_VABI utext_freeze_nid_postfix(UText* ut) {
    utext_freeze(Readable(ut));
}

void APS5_VABI ubrk_setUText_nid_postfix(UBreakIterator* bi, UText* text, std::int32_t* status) {
    UErrorCode* error = Status(status);
    IcuBackend::RequireObject(bi);
    if (U_SUCCESS(*error)) Open(text);
    ubrk_setUText(bi, text, error);
}

void APS5_VABI ubrk_refreshUText_nid_postfix(UBreakIterator* bi, UText* text, std::int32_t* status) {
    UErrorCode* error = Status(status);
    IcuBackend::RequireObject(bi);
    if (U_SUCCESS(*error) && text != nullptr) Open(text);
    ubrk_refreshUText(bi, text, error);
}

}
