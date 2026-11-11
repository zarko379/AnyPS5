#include "prx/libSceIcu/CaseProperties.hpp"
#include <unicode/uchar.h>
#include <unicode/udata.h>
#include "ucase.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "ICU CASE PROPERTIES: %s\n", message.c_str());
        std::abort();
    }
}

template<typename TAction>
bool Invalid(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

const char16_t* const Sentinel = u"sentinel";

struct Context {
    std::vector<std::int32_t> backward;
    std::vector<std::int32_t> forward;
    std::size_t position = 0;
    std::int8_t direction = 0;
    std::vector<std::int8_t> dirs;
};

std::int32_t APS5_VABI GuestIterator(void* raw, std::int8_t dir) {
    auto* context = static_cast<Context*>(raw);
    context->dirs.push_back(dir);
    if (dir != 0) {
        context->direction = dir;
        context->position = 0;
    }
    const auto& list = context->direction < 0 ? context->backward : context->forward;
    return context->position < list.size() ? list[context->position++] : -1;
}

UChar32 U_CALLCONV HostIterator(void* raw, int8_t dir) {
    auto* context = static_cast<Context*>(raw);
    if (dir != 0) {
        context->direction = dir;
        context->position = 0;
    }
    const auto& list = context->direction < 0 ? context->backward : context->forward;
    return context->position < list.size() ? list[context->position++] : -1;
}

std::u16string Text(std::int32_t result, const char16_t* string) {
    if (result < 0 || result > UCASE_MAX_STRING_LENGTH) return std::u16string();
    return std::u16string(string, static_cast<std::size_t>(result));
}

bool SameMapping(std::int32_t guestResult, const char16_t* guestString, std::int32_t hostResult, const UChar* hostString) {
    if (guestResult != hostResult) return false;
    if (guestString == Sentinel || hostString == Sentinel) return guestString == hostString;
    if (guestResult >= 0 && guestResult <= UCASE_MAX_STRING_LENGTH) return Text(guestResult, guestString) == Text(hostResult, hostString);
    return false;
}

using GuestFull = std::int32_t (APS5_VABI*)(std::int32_t, GuestCaseContextIterator, void*, const char16_t**, std::int32_t);
using HostFull = int32_t (*)(UChar32, UCaseContextIterator*, void*, const UChar**, int32_t);

void CheckFull(GuestFull guest, HostFull host, std::int32_t c, std::int32_t locale, const Context& base, const std::string& name) {
    Context guestContext = base;
    Context hostContext = base;
    const char16_t* guestString = Sentinel;
    const UChar* hostString = Sentinel;
    const std::int32_t guestResult = guest(c, GuestIterator, &guestContext, &guestString, locale);
    const std::int32_t hostResult = host(c, HostIterator, &hostContext, &hostString, locale);
    if (!SameMapping(guestResult, guestString, hostResult, hostString)) Require(false, name + " against ICU at " + std::to_string(c) + " locale " + std::to_string(locale) + " results " + std::to_string(guestResult) + " " + std::to_string(hostResult) + " sentinel " + std::to_string(guestString == Sentinel) + std::to_string(hostString == Sentinel) + " dirs " + std::to_string(guestContext.dirs.size()));
    const char16_t* plainGuest = Sentinel;
    const UChar* plainHost = Sentinel;
    const std::int32_t plainGuestResult = guest(c, nullptr, nullptr, &plainGuest, locale);
    const std::int32_t plainHostResult = host(c, nullptr, nullptr, &plainHost, locale);
    if (!SameMapping(plainGuestResult, plainGuest, plainHostResult, plainHost)) Require(false, name + " without context at " + std::to_string(c));
}

void TestKnownValues() {
    Require(ucase_tolower_nid_postfix(0x41) == 0x61 && ucase_toupper_nid_postfix(0x61) == 0x41 && ucase_totitle_nid_postfix(0x1c6) == 0x1c5, "simple mappings");
    Require(ucase_tolower_nid_postfix(0x130) == 0x69 && ucase_toupper_nid_postfix(0x131) == 0x49 && ucase_toupper_nid_postfix(0xdf) == 0xdf, "simple special mappings");
    Require(ucase_tolower_nid_postfix(-1) == -1 && ucase_toupper_nid_postfix(0x110000) == 0x110000, "simple mappings out of range");
    Require(ucase_fold_nid_postfix(0x49, U_FOLD_CASE_DEFAULT) == 0x69 && ucase_fold_nid_postfix(0x49, U_FOLD_CASE_EXCLUDE_SPECIAL_I) == 0x131, "fold I");
    Require(ucase_fold_nid_postfix(0x130, U_FOLD_CASE_DEFAULT) == 0x130 && ucase_fold_nid_postfix(0x130, U_FOLD_CASE_EXCLUDE_SPECIAL_I) == 0x69, "fold dotted I");
    Require(ucase_fold_nid_postfix(0x3a3, U_FOLD_CASE_DEFAULT) == 0x3c3 && ucase_fold_nid_postfix(0x1e9e, U_FOLD_CASE_DEFAULT) == 0xdf, "fold sigma and sharp s");
    Require(ucase_getType_nid_postfix(0x41) == UCASE_UPPER && ucase_getType_nid_postfix(0x61) == UCASE_LOWER && ucase_getType_nid_postfix(0x1c5) == UCASE_TITLE && ucase_getType_nid_postfix(0x30) == UCASE_NONE, "getType");
    Require(ucase_getTypeOrIgnorable_nid_postfix(0x27) == 4 && ucase_getTypeOrIgnorable_nid_postfix(0x300) == 4 && ucase_getTypeOrIgnorable_nid_postfix(0x61) == UCASE_LOWER, "getTypeOrIgnorable");
    Require(ucase_isSoftDotted_nid_postfix(0x69) == 1 && ucase_isSoftDotted_nid_postfix(0x6a) == 1 && ucase_isSoftDotted_nid_postfix(0x49) == 0, "isSoftDotted");
    Require(ucase_isCaseSensitive_nid_postfix(0x41) == 1 && ucase_isCaseSensitive_nid_postfix(0x30) == 0 && ucase_isCaseSensitive_nid_postfix(0x10ffff) == 0, "isCaseSensitive");

    const char16_t* string = Sentinel;
    Require(ucase_toFullUpper_nid_postfix(0xdf, nullptr, nullptr, &string, UCASE_LOC_ROOT) == 2 && Text(2, string) == u"SS", "toFullUpper sharp s");
    string = Sentinel;
    Require(ucase_toFullLower_nid_postfix(0x130, nullptr, nullptr, &string, UCASE_LOC_ROOT) == 2 && Text(2, string) == std::u16string{0x69, 0x307}, "toFullLower dotted I");
    string = Sentinel;
    Require(ucase_toFullLower_nid_postfix(0x49, nullptr, nullptr, &string, UCASE_LOC_TURKISH) == 0x131 && string == Sentinel, "toFullLower Turkish I");
    Require(ucase_toFullUpper_nid_postfix(0x69, nullptr, nullptr, &string, UCASE_LOC_TURKISH) == 0x130 && string == Sentinel, "toFullUpper Turkish i");
    Require(ucase_toFullTitle_nid_postfix(0x1c6, nullptr, nullptr, &string, UCASE_LOC_ROOT) == 0x1c5 && string == Sentinel, "toFullTitle dz");
    Require(ucase_toFullLower_nid_postfix(0x61, nullptr, nullptr, &string, UCASE_LOC_ROOT) == ~0x61 && string == Sentinel, "toFullLower unchanged");
    Require(ucase_toFullFolding_nid_postfix(0xdf, &string, U_FOLD_CASE_DEFAULT) == 2 && Text(2, string) == u"ss", "toFullFolding sharp s");
    string = Sentinel;
    Require(ucase_toFullFolding_nid_postfix(0x130, &string, U_FOLD_CASE_EXCLUDE_SPECIAL_I) == 0x69 && string == Sentinel, "toFullFolding Turkic dotted I");
    Require(ucase_toFullFolding_nid_postfix(0x41, nullptr, U_FOLD_CASE_DEFAULT) == 0x61, "toFullFolding without string");

    Context moreAbove;
    moreAbove.forward = {0x300};
    string = Sentinel;
    Require(ucase_toFullLower_nid_postfix(0x49, GuestIterator, &moreAbove, &string, UCASE_LOC_LITHUANIAN) == 2 && Text(2, string) == std::u16string{0x69, 0x307}, "Lithuanian More_Above");
    Require(!moreAbove.dirs.empty() && moreAbove.dirs.front() == 1, "Lithuanian iterator direction");
    Context noAbove;
    noAbove.forward = {0x41};
    string = Sentinel;
    Require(ucase_toFullLower_nid_postfix(0x49, GuestIterator, &noAbove, &string, UCASE_LOC_LITHUANIAN) == 0x69 && string == Sentinel, "Lithuanian without More_Above");
    Context softDotted;
    softDotted.backward = {0x69};
    string = Sentinel;
    Require(ucase_toFullUpper_nid_postfix(0x307, GuestIterator, &softDotted, &string, UCASE_LOC_LITHUANIAN) == 0 && string == nullptr, "Lithuanian After_Soft_Dotted");
    Require(Invalid([&] { ucase_toFullTitle_nid_postfix(0x307, GuestIterator, &softDotted, nullptr, UCASE_LOC_LITHUANIAN); }), "Lithuanian After_Soft_Dotted writes NULL through the string");
    Context noSoftDotted;
    noSoftDotted.backward = {0x41};
    string = Sentinel;
    Require(ucase_toFullUpper_nid_postfix(0x307, GuestIterator, &noSoftDotted, nullptr, UCASE_LOC_LITHUANIAN) == ~0x307 && ucase_toFullUpper_nid_postfix(0x307, GuestIterator, &noSoftDotted, &string, UCASE_LOC_LITHUANIAN) == ~0x307 && string == Sentinel, "Lithuanian dot above after another letter");
    Context finalSigma;
    finalSigma.backward = {0x300, 0x391};
    finalSigma.forward = {0x20};
    Require(ucase_toFullLower_nid_postfix(0x3a3, GuestIterator, &finalSigma, &string, UCASE_LOC_ROOT) == 0x3c2, "final sigma");
    std::vector<std::int8_t> expectedDirs{1, -1, 0};
    Require(finalSigma.dirs == expectedDirs, "final sigma iterator calls");
    Context medialSigma;
    medialSigma.backward = {0x391};
    medialSigma.forward = {0x27, 0x391};
    Require(ucase_toFullLower_nid_postfix(0x3a3, GuestIterator, &medialSigma, &string, UCASE_LOC_ROOT) == 0x3c3, "medial sigma");
    Require(ucase_toFullLower_nid_postfix(0x3a3, nullptr, &medialSigma, &string, UCASE_LOC_ROOT) == 0x3c3, "sigma without iterator");
}

void TestErrors() {
    const char16_t* string = Sentinel;
    Require(Invalid([&] { ucase_toFullLower_nid_postfix(-1, nullptr, nullptr, &string, UCASE_LOC_ROOT); }), "toFullLower negative");
    Require(Invalid([&] { ucase_toFullUpper_nid_postfix(-2, nullptr, nullptr, &string, UCASE_LOC_ROOT); }), "toFullUpper negative");
    Require(Invalid([&] { ucase_toFullTitle_nid_postfix(INT32_MIN, nullptr, nullptr, &string, UCASE_LOC_ROOT); }), "toFullTitle negative");
    Require(Invalid([&] { ucase_toFullFolding_nid_postfix(-1, &string, 0); }), "toFullFolding negative");
    Require(string == Sentinel, "negative input writes nothing");
    Require(Invalid([] { ucase_toFullUpper_nid_postfix(0xdf, nullptr, nullptr, nullptr, UCASE_LOC_ROOT); }), "toFullUpper NULL string");
    Require(Invalid([] { ucase_toFullLower_nid_postfix(0x130, nullptr, nullptr, nullptr, UCASE_LOC_ROOT); }), "toFullLower NULL string");
    Require(Invalid([] { ucase_toFullTitle_nid_postfix(0xfb01, nullptr, nullptr, nullptr, UCASE_LOC_ROOT); }), "toFullTitle NULL string");
    Require(Invalid([] { ucase_toFullFolding_nid_postfix(0xdf, nullptr, U_FOLD_CASE_DEFAULT); }), "toFullFolding NULL string");
    Require(ucase_toFullLower_nid_postfix(0x41, nullptr, nullptr, nullptr, UCASE_LOC_ROOT) == 0x61 && ucase_toFullUpper_nid_postfix(0x110000, nullptr, nullptr, nullptr, UCASE_LOC_ROOT) == ~0x110000, "NULL string without a string result");
}

void TestSweep() {
    Context context;
    context.backward = {0x69, 0x391};
    context.forward = {0x300, 0x41};
    for (UChar32 c = -2; c <= 0x110001; ++c) {
        if (ucase_tolower_nid_postfix(c) != ucase_tolower(c) || ucase_toupper_nid_postfix(c) != ucase_toupper(c) || ucase_totitle_nid_postfix(c) != ucase_totitle(c)) Require(false, "simple mapping at " + std::to_string(c));
        if (ucase_fold_nid_postfix(c, U_FOLD_CASE_DEFAULT) != ucase_fold(c, U_FOLD_CASE_DEFAULT) || ucase_fold_nid_postfix(c, U_FOLD_CASE_EXCLUDE_SPECIAL_I) != ucase_fold(c, U_FOLD_CASE_EXCLUDE_SPECIAL_I)) Require(false, "fold at " + std::to_string(c));
        if (ucase_getType_nid_postfix(c) != ucase_getType(c) || ucase_getTypeOrIgnorable_nid_postfix(c) != ucase_getTypeOrIgnorable(c)) Require(false, "type at " + std::to_string(c));
        if (ucase_isSoftDotted_nid_postfix(c) != ucase_isSoftDotted(c) || ucase_isCaseSensitive_nid_postfix(c) != ucase_isCaseSensitive(c)) Require(false, "flags at " + std::to_string(c));
        if (c < 0) continue;
        const std::int32_t locale = c % 3 == 0 ? UCASE_LOC_LITHUANIAN : c % 3 == 1 ? UCASE_LOC_TURKISH : UCASE_LOC_ROOT;
        CheckFull(ucase_toFullLower_nid_postfix, ucase_toFullLower, c, locale, context, "toFullLower");
        CheckFull(ucase_toFullUpper_nid_postfix, ucase_toFullUpper, c, locale, context, "toFullUpper");
        CheckFull(ucase_toFullTitle_nid_postfix, ucase_toFullTitle, c, locale, context, "toFullTitle");
        for (std::uint32_t options : {static_cast<std::uint32_t>(U_FOLD_CASE_DEFAULT), static_cast<std::uint32_t>(U_FOLD_CASE_EXCLUDE_SPECIAL_I)}) {
            const char16_t* guestString = Sentinel;
            const UChar* hostString = Sentinel;
            const std::int32_t guestResult = ucase_toFullFolding_nid_postfix(c, &guestString, options);
            const std::int32_t hostResult = ucase_toFullFolding(c, &hostString, options);
            if (!SameMapping(guestResult, guestString, hostResult, hostString)) Require(false, "toFullFolding at " + std::to_string(c));
        }
    }
    for (std::int32_t locale = 0; locale <= UCASE_LOC_ARMENIAN + 1; ++locale) {
        for (UChar32 c : {0x49, 0x69, 0x130, 0x131, 0x307, 0x3a3, 0x587, 0xcc, 0x12e, 0x4a}) {
            CheckFull(ucase_toFullLower_nid_postfix, ucase_toFullLower, c, locale, context, "toFullLower locale");
            CheckFull(ucase_toFullUpper_nid_postfix, ucase_toFullUpper, c, locale, context, "toFullUpper locale");
            CheckFull(ucase_toFullTitle_nid_postfix, ucase_toFullTitle, c, locale, context, "toFullTitle locale");
        }
    }
}

}

int main() {
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    TestKnownValues();
    TestErrors();
    TestSweep();
    std::puts("ICU CASE PROPERTIES TESTS PASSED");
    return 0;
}
