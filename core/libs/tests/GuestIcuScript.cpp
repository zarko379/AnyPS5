#include "prx/libSceIcu/Script.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include <unicode/udata.h>
#include <unicode/uloc.h>
#include <unicode/uscript.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;

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

void CompareCodes(const char* name, const char* what) {
    for (std::int32_t capacity : {0, 1, 8}) {
        std::vector<std::int32_t> guest(8, -7);
        std::vector<UScriptCode> host(8, static_cast<UScriptCode>(-7));
        std::int32_t guestStatus = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        const std::int32_t guestLength = uscript_getCode_nid_postfix(name, capacity ? guest.data() : nullptr, capacity, &guestStatus);
        const std::int32_t hostLength = uscript_getCode(name, capacity ? host.data() : nullptr, capacity, &hostStatus);
        Require(guestLength == hostLength && guestStatus == hostStatus, what);
        for (std::size_t index = 0; index < 8; ++index) Require(guest[index] == host[index], what);
    }
}

void CheckIDefault() {
    for (const char* name : {"i-default@x=y", "i-default_US@x=y", "i-default-x@a=b", "i-default.utf8"}) {
        for (std::int32_t capacity : {0, 8}) {
            Require(Throws<std::runtime_error>([&] { std::int32_t status = Ok; std::int32_t length = 0; GuestCodes(name, capacity, status, length); }), "an i-default locale whose likely subtags ICU takes from the default throws");
        }
        std::int32_t status = Ok;
        std::int32_t one = 0;
        Require(uscript_getCode_nid_postfix(name, nullptr, 1, &status) == 0 && status == IllegalArgument, "a null buffer with capacity never reaches the likely subtags");
        status = Ok;
        Require(uscript_getCode_nid_postfix(name, &one, -1, &status) == 0 && status == IllegalArgument, "a negative capacity never reaches the likely subtags");
        status = BufferOverflow;
        Require(uscript_getCode_nid_postfix(name, &one, 1, &status) == 0 && status == BufferOverflow, "a failed status never reaches the likely subtags");
    }
    for (const char* name : {"i-default", "I-DEFAULT@x=y", "i_default@x=y", "ru@x=y"}) CompareCodes(name, "a name ICU resolves without the default matches ICU");
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    uloc_setDefault_nid_postfix("ru", &status);
    uloc_setDefault("ru", &hostStatus);
    for (const char* name : {"i-default@x=y", "i-default_US@x=y", "i-default.utf8"}) CompareCodes(name, "an i-default locale takes the guest default's likely script");
    std::int32_t length = 0;
    std::vector<std::int32_t> codes = GuestCodes("i-default@x=y", 8, status, length);
    Require(length == 1 && status == Ok && codes[0] == USCRIPT_CYRILLIC, "the guest default ru gives Cyrillic");
    uloc_setDefault_nid_postfix(nullptr, &status);
}

}

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    CheckGetCode();
    CheckIDefault();
    return 0;
}
