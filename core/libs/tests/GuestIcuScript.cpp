#include "prx/libSceIcu/Script.hpp"
#include <unicode/udata.h>
#include <unicode/uscript.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t NotTerminated = -124;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "ICU SCRIPT: %s\n", message);
        std::abort();
    }
}

template<typename TException, typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const TException&) {
        return true;
    }
    return false;
}

bool SameName(const char* guest, const char* host) {
    if (!guest || !host) return guest == host;
    return std::strcmp(guest, host) == 0;
}

std::vector<std::int32_t> GuestCodes(const char* name, std::int32_t capacity, std::int32_t& status, std::int32_t& length) {
    std::vector<std::int32_t> codes(static_cast<std::size_t>(capacity > 0 ? capacity : 1), -7);
    status = Ok;
    length = uscript_getCode_nid_postfix(name, capacity > 0 ? codes.data() : nullptr, capacity, &status);
    return codes;
}

void CheckGetCode() {
    std::int32_t status = Ok;
    std::int32_t length = 0;
    std::vector<std::int32_t> codes = GuestCodes("Latn", 8, status, length);
    Require(length == 1 && status == Ok && codes[0] == USCRIPT_LATIN, "a script abbreviation");
    codes = GuestCodes("Cyrillic", 8, status, length);
    Require(length == 1 && status == Ok && codes[0] == USCRIPT_CYRILLIC, "a long script name");
    codes = GuestCodes("ja", 8, status, length);
    Require(length == 3 && status == Ok && codes[0] == USCRIPT_KATAKANA && codes[1] == USCRIPT_HIRAGANA && codes[2] == USCRIPT_HAN, "Japanese uses three scripts");
    codes = GuestCodes("ja", 1, status, length);
    Require(length == 3 && status == BufferOverflow, "Japanese overflows a single code");
    codes = GuestCodes("zh_Hant", 8, status, length);
    Require(length == 2 && status == Ok && codes[0] == USCRIPT_HAN && codes[1] == USCRIPT_BOPOMOFO, "traditional Chinese");
    codes = GuestCodes("sr-Latn", 8, status, length);
    Require(length == 1 && status == Ok && codes[0] == USCRIPT_LATIN, "an explicit script subtag");
    codes = GuestCodes("ru", 8, status, length);
    Require(length == 1 && status == Ok && codes[0] == USCRIPT_CYRILLIC, "a script from likely subtags");
    codes = GuestCodes("Latn", 0, status, length);
    Require(length == 1 && status == BufferOverflow, "preflight");

    status = Ok;
    Require(uscript_getCode_nid_postfix(nullptr, codes.data(), 1, &status) == 0 && status == IllegalArgument, "a null name");
    status = Ok;
    Require(uscript_getCode_nid_postfix("Latn", nullptr, 1, &status) == 0 && status == IllegalArgument, "a null buffer with capacity");
    status = Ok;
    std::int32_t one = 0;
    Require(uscript_getCode_nid_postfix("Latn", &one, -1, &status) == 0 && status == IllegalArgument, "a negative capacity");
    status = IllegalArgument;
    Require(uscript_getCode_nid_postfix("Latn", &one, 1, &status) == 0 && status == IllegalArgument, "a failed status stays");
    Require(Throws<std::invalid_argument>([] { std::int32_t code = 0; uscript_getCode_nid_postfix("Latn", &code, 1, nullptr); }), "a null status throws");

    const char* names[] = {"Latn", "Latin", "Hani", "Hans", "Hant", "Jpan", "Kore", "ja", "ja_JP", "ko", "zh", "zh_Hant", "zh-TW", "en", "en_US", "ar", "he", "sr", "sr_Latn", "und", "", "x", "Zzzz", "Zyyy", "xyzzy", "de_DE@calendar=buddhist", "Qaai", "mn_Mong"};
    for (const char* name : names) {
        for (std::int32_t capacity : {0, 1, 8}) {
            std::vector<std::int32_t> guest(8, -7);
            std::vector<UScriptCode> host(8, static_cast<UScriptCode>(-7));
            std::int32_t guestStatus = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            const std::int32_t guestLength = uscript_getCode_nid_postfix(name, capacity ? guest.data() : nullptr, capacity, &guestStatus);
            const std::int32_t hostLength = uscript_getCode(name, capacity ? host.data() : nullptr, capacity, &hostStatus);
            Require(guestLength == hostLength && guestStatus == hostStatus, "script codes match ICU");
            for (std::size_t index = 0; index < 8; ++index) Require(guest[index] == host[index], "filled script codes match ICU");
        }
    }
}

void CheckScriptData() {
    for (std::int32_t script = -2; script <= USCRIPT_CODE_LIMIT + 2; ++script) {
        Require(SameName(uscript_getName_nid_postfix(script), uscript_getName(static_cast<UScriptCode>(script))), "long script names match ICU");
        Require(SameName(uscript_getShortName_nid_postfix(script), uscript_getShortName(static_cast<UScriptCode>(script))), "short script names match ICU");
        Require(uscript_getUsage_nid_postfix(script) == uscript_getUsage(static_cast<UScriptCode>(script)), "usage matches ICU");
        Require(uscript_isRightToLeft_nid_postfix(script) == uscript_isRightToLeft(static_cast<UScriptCode>(script)), "direction matches ICU");
        Require(uscript_breaksBetweenLetters_nid_postfix(script) == uscript_breaksBetweenLetters(static_cast<UScriptCode>(script)), "letter breaks match ICU");
        Require(uscript_isCased_nid_postfix(script) == uscript_isCased(static_cast<UScriptCode>(script)), "casing matches ICU");
        char16_t guest[4]{};
        char16_t host[4]{};
        std::int32_t guestStatus = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        Require(uscript_getSampleString_nid_postfix(script, guest, 4, &guestStatus) == uscript_getSampleString(static_cast<UScriptCode>(script), host, 4, &hostStatus) && guestStatus == hostStatus && std::memcmp(guest, host, sizeof(guest)) == 0, "sample strings match ICU");
    }
    Require(std::strcmp(uscript_getName_nid_postfix(USCRIPT_LATIN), "Latin") == 0 && std::strcmp(uscript_getShortName_nid_postfix(USCRIPT_LATIN), "Latn") == 0, "Latin names");
    Require(uscript_getName_nid_postfix(USCRIPT_CODE_LIMIT + 50) == nullptr && uscript_getShortName_nid_postfix(-1) == nullptr, "unknown scripts have no names");
    Require(uscript_getUsage_nid_postfix(USCRIPT_LATIN) == USCRIPT_USAGE_RECOMMENDED && uscript_getUsage_nid_postfix(USCRIPT_GOTHIC) == USCRIPT_USAGE_EXCLUDED && uscript_getUsage_nid_postfix(-1) == USCRIPT_USAGE_NOT_ENCODED, "script usage");
    Require(uscript_isRightToLeft_nid_postfix(USCRIPT_ARABIC) && !uscript_isRightToLeft_nid_postfix(USCRIPT_LATIN), "right-to-left scripts");
    Require(uscript_breaksBetweenLetters_nid_postfix(USCRIPT_HAN) && !uscript_breaksBetweenLetters_nid_postfix(USCRIPT_LATIN), "scripts that break between letters");
    Require(uscript_isCased_nid_postfix(USCRIPT_GREEK) && !uscript_isCased_nid_postfix(USCRIPT_ARABIC), "cased scripts");

    char16_t sample[4]{0x7777, 0x7777, 0x7777, 0x7777};
    std::int32_t status = Ok;
    Require(uscript_getSampleString_nid_postfix(USCRIPT_LATIN, sample, 4, &status) == 1 && status == Ok && sample[0] == 0x4C && sample[1] == 0, "the Latin sample");
    status = Ok;
    Require(uscript_getSampleString_nid_postfix(USCRIPT_DESERET, sample, 4, &status) == 2 && status == Ok && sample[0] == 0xD801 && sample[1] == 0xDC14, "a supplementary sample");
    status = Ok;
    Require(uscript_getSampleString_nid_postfix(USCRIPT_DESERET, nullptr, 0, &status) == 2 && status == BufferOverflow, "sample preflight");
    status = Ok;
    sample[1] = 0x7777;
    Require(uscript_getSampleString_nid_postfix(USCRIPT_LATIN, sample, 1, &status) == 1 && status == NotTerminated && sample[1] == 0x7777, "an exact sample fit is not terminated");
    status = Ok;
    Require(uscript_getSampleString_nid_postfix(USCRIPT_LATIN, sample, -1, &status) == 0 && status == IllegalArgument, "a negative sample capacity");
    status = Ok;
    Require(uscript_getSampleString_nid_postfix(USCRIPT_LATIN, nullptr, 2, &status) == 0 && status == IllegalArgument, "a null sample buffer");
    status = BufferOverflow;
    Require(uscript_getSampleString_nid_postfix(USCRIPT_LATIN, sample, 4, &status) == 0 && status == BufferOverflow, "a failed sample status stays");
    Require(Throws<std::invalid_argument>([&] { uscript_getSampleString_nid_postfix(USCRIPT_LATIN, sample, 4, nullptr); }), "a null sample status throws");
}

void CheckCodePoints() {
    for (std::int32_t c = -2; c <= 0x110001; ++c) {
        std::int32_t guestStatus = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        if (uscript_getScript_nid_postfix(c, &guestStatus) != uscript_getScript(c, &hostStatus) || guestStatus != hostStatus) {
            std::fprintf(stderr, "ICU SCRIPT: uscript_getScript differs at %X\n", static_cast<unsigned>(c));
            std::abort();
        }
        std::int32_t guest[32];
        UScriptCode host[32];
        std::fill(std::begin(guest), std::end(guest), -7);
        std::fill(std::begin(host), std::end(host), static_cast<UScriptCode>(-7));
        guestStatus = Ok;
        hostStatus = U_ZERO_ERROR;
        const std::int32_t capacity = c % 3 == 0 ? 1 : 32;
        if (uscript_getScriptExtensions_nid_postfix(c, guest, capacity, &guestStatus) != uscript_getScriptExtensions(c, host, capacity, &hostStatus) || guestStatus != hostStatus || std::memcmp(guest, host, sizeof(guest)) != 0) {
            std::fprintf(stderr, "ICU SCRIPT: uscript_getScriptExtensions differs at %X\n", static_cast<unsigned>(c));
            std::abort();
        }
        if (c % 5 == 0) {
            for (std::int32_t script : {static_cast<std::int32_t>(USCRIPT_LATIN), static_cast<std::int32_t>(USCRIPT_COMMON), static_cast<std::int32_t>(USCRIPT_INHERITED), static_cast<std::int32_t>(USCRIPT_HAN), static_cast<std::int32_t>(USCRIPT_ARABIC), static_cast<std::int32_t>(USCRIPT_DEVANAGARI), static_cast<std::int32_t>(USCRIPT_SYRIAC), -1, 0x8000}) {
                if (uscript_hasScript_nid_postfix(c, script) != uscript_hasScript(c, static_cast<UScriptCode>(script))) {
                    std::fprintf(stderr, "ICU SCRIPT: uscript_hasScript %d differs at %X\n", script, static_cast<unsigned>(c));
                    std::abort();
                }
            }
        }
    }
    std::int32_t status = Ok;
    Require(uscript_getScript_nid_postfix(0x0915, &status) == USCRIPT_DEVANAGARI && uscript_getScript_nid_postfix(0x0640, &status) == USCRIPT_COMMON && uscript_getScript_nid_postfix(0x0301, &status) == USCRIPT_INHERITED && status == Ok, "scripts of code points");
    Require(uscript_getScript_nid_postfix(0x110000, &status) == USCRIPT_INVALID_CODE && status == IllegalArgument, "a code point beyond U+10FFFF is an illegal argument");
    status = IllegalArgument;
    Require(uscript_getScript_nid_postfix(0x41, &status) == USCRIPT_INVALID_CODE && status == IllegalArgument, "a failed script status stays");
    Require(uscript_getScript_nid_postfix(0x41, nullptr) == USCRIPT_INVALID_CODE, "a null script status returns USCRIPT_INVALID_CODE as in ICU");
    Require(uscript_hasScript_nid_postfix(0x0640, USCRIPT_ARABIC) && uscript_hasScript_nid_postfix(0x0640, USCRIPT_SYRIAC) && !uscript_hasScript_nid_postfix(0x0640, USCRIPT_COMMON) && !uscript_hasScript_nid_postfix(0x0640, 0x8000), "script extensions of the tatweel");

    std::int32_t scripts[16]{};
    status = Ok;
    const std::int32_t count = uscript_getScriptExtensions_nid_postfix(0x0640, scripts, 16, &status);
    Require(count > 2 && status == Ok && scripts[0] == USCRIPT_ARABIC, "the tatweel extensions start with Arabic");
    status = Ok;
    Require(uscript_getScriptExtensions_nid_postfix(0x0640, nullptr, 0, &status) == count && status == BufferOverflow, "extension preflight");
    status = Ok;
    Require(uscript_getScriptExtensions_nid_postfix(0x41, scripts, 16, &status) == 1 && status == Ok && scripts[0] == USCRIPT_LATIN, "a single extension");
    status = Ok;
    Require(uscript_getScriptExtensions_nid_postfix(0x41, scripts, -1, &status) == 0 && status == IllegalArgument, "a negative extension capacity");
    status = Ok;
    Require(uscript_getScriptExtensions_nid_postfix(0x41, nullptr, 1, &status) == 0 && status == IllegalArgument, "a null extension buffer");
    status = BufferOverflow;
    Require(uscript_getScriptExtensions_nid_postfix(0x41, scripts, 16, &status) == 0 && status == BufferOverflow, "a failed extension status stays");
    Require(uscript_getScriptExtensions_nid_postfix(0x41, scripts, 16, nullptr) == 0, "a null extension status returns 0 as in ICU");
}

}

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    CheckGetCode();
    CheckScriptData();
    CheckCodePoints();
    return 0;
}
