#include "prx/libSceIcu/Idna.hpp"
#include <unicode/udata.h>
#include <unicode/uidna.h>
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
constexpr std::int32_t UselessCollator = 0x10102;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "ICU IDNA: %s\n", message);
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

UIDNAInfo Info() {
    UIDNAInfo info = UIDNA_INFO_INITIALIZER;
    return info;
}

using NameFn = std::int32_t (*)(const UIDNA*, const char16_t*, std::int32_t, char16_t*, std::int32_t, UIDNAInfo*, std::int32_t*);
using HostNameFn = std::int32_t (*)(const UIDNA*, const UChar*, std::int32_t, UChar*, std::int32_t, UIDNAInfo*, UErrorCode*);

std::int32_t GuestToAscii(const UIDNA* idna, const char16_t* name, std::int32_t length, char16_t* dest, std::int32_t capacity, UIDNAInfo* info, std::int32_t* status) {
    return uidna_nameToASCII_nid_postfix(idna, name, length, dest, capacity, info, status);
}

std::int32_t GuestToUnicode(const UIDNA* idna, const char16_t* name, std::int32_t length, char16_t* dest, std::int32_t capacity, UIDNAInfo* info, std::int32_t* status) {
    return uidna_nameToUnicode_nid_postfix(idna, name, length, dest, capacity, info, status);
}

std::int32_t HostToAscii(const UIDNA* idna, const UChar* name, std::int32_t length, UChar* dest, std::int32_t capacity, UIDNAInfo* info, UErrorCode* status) {
    return uidna_nameToASCII(idna, name, length, dest, capacity, info, status);
}

std::int32_t HostToUnicode(const UIDNA* idna, const UChar* name, std::int32_t length, UChar* dest, std::int32_t capacity, UIDNAInfo* info, UErrorCode* status) {
    return uidna_nameToUnicode(idna, name, length, dest, capacity, info, status);
}

void CheckOpen() {
    Require(Throws<std::invalid_argument>([] { uidna_openUTS46_nid_postfix(0, nullptr); }), "a null status throws in uidna_openUTS46");
    std::int32_t status = IllegalArgument;
    Require(!uidna_openUTS46_nid_postfix(0, &status) && status == IllegalArgument, "a failed status opens nothing");
    status = Ok;
    UIDNA* idna = uidna_openUTS46_nid_postfix(0, &status);
    Require(idna && status == Ok, "a UTS 46 instance opens");
    uidna_close_nid_postfix(idna);
    uidna_close_nid_postfix(nullptr);
}

void CheckKnownValues() {
    static const char16_t fA_sharps16[] = {0x66, 0x41, 0xdf, 0};
    static const char16_t fa_sharps16[] = {0x66, 0x61, 0xdf, 0};
    static const char16_t fass16[] = {0x66, 0x61, 0x73, 0x73, 0};
    static const char16_t fA_BEL[] = {0x66, 0x41, 7, 0};
    static const char16_t fa_FFFD[] = {0x66, 0x61, 0xfffd, 0};
    char16_t dest16[10];
    std::int32_t status = Ok;
    UIDNA* uts46 = uidna_openUTS46_nid_postfix(UIDNA_USE_STD3_RULES | UIDNA_NONTRANSITIONAL_TO_UNICODE, &status);
    Require(uts46 && status == Ok, "the ICU test instance opens");
    UIDNAInfo info = Info();
    std::int32_t length = uidna_nameToASCII_nid_postfix(uts46, fA_sharps16, 3, dest16, 4, &info, &status);
    Require(status == NotTerminated && length == 4 && std::memcmp(dest16, fass16, 8) == 0 && info.isTransitionalDifferent && info.errors == 0, "nameToASCII maps sharp s transitionally");
    status = Ok;
    length = uidna_nameToUnicode_nid_postfix(uts46, fA_sharps16, -1, dest16, 3, &info, &status);
    Require(status == NotTerminated && length == 3 && std::memcmp(dest16, fa_sharps16, 6) == 0 && info.isTransitionalDifferent && info.errors == 0, "nameToUnicode keeps sharp s");
    status = Ok;
    length = uidna_nameToASCII_nid_postfix(uts46, nullptr, 0, dest16, 0, &info, &status);
    Require(status == NotTerminated && length == 0 && !info.isTransitionalDifferent && info.errors == UIDNA_ERROR_EMPTY_LABEL, "an empty name is an empty label");
    status = Ok;
    length = uidna_nameToUnicode_nid_postfix(uts46, fA_BEL, -1, dest16, 3, &info, &status);
    Require(status == NotTerminated && length == 3 && std::memcmp(dest16, fa_FFFD, 6) == 0 && !info.isTransitionalDifferent && info.errors != 0, "a disallowed character becomes U+FFFD with an error");
    status = Ok;
    length = uidna_nameToUnicode_nid_postfix(uts46, fA_sharps16, -1, dest16, 2, &info, &status);
    Require(status == BufferOverflow && length == 3, "a short buffer overflows");
    status = UselessCollator;
    Require(uidna_nameToASCII_nid_postfix(uts46, fA_sharps16, -1, dest16, 10, &info, &status) == 0 && status == UselessCollator, "a failed status is kept");
    status = Ok;
    uidna_nameToUnicode_nid_postfix(uts46, fA_sharps16, -1, dest16, 10, nullptr, &status);
    Require(status == IllegalArgument, "a null info is an illegal argument");
    UIDNAInfo small = Info();
    small.size = 15;
    status = Ok;
    uidna_nameToUnicode_nid_postfix(uts46, fA_sharps16, -1, dest16, 10, &small, &status);
    Require(status == IllegalArgument, "an info size below 16 is an illegal argument");
    status = Ok;
    uidna_nameToASCII_nid_postfix(uts46, nullptr, 3, dest16, 4, &info, &status);
    Require(status == IllegalArgument, "a null name with a length is an illegal argument");
    status = Ok;
    uidna_nameToUnicode_nid_postfix(uts46, fA_sharps16, -2, dest16, 3, &info, &status);
    Require(status == IllegalArgument, "a length below -1 is an illegal argument");
    status = Ok;
    uidna_nameToASCII_nid_postfix(uts46, fA_sharps16, -1, nullptr, 4, &info, &status);
    Require(status == IllegalArgument, "a null buffer with a capacity is an illegal argument");
    status = Ok;
    uidna_nameToASCII_nid_postfix(uts46, fA_sharps16, -1, dest16, -1, &info, &status);
    Require(status == IllegalArgument, "a negative capacity is an illegal argument");
    status = Ok;
    std::memcpy(dest16, fA_sharps16, sizeof(fA_sharps16));
    uidna_nameToASCII_nid_postfix(uts46, dest16, -1, dest16, 10, &info, &status);
    Require(status == IllegalArgument, "a buffer that is also the name is an illegal argument");
    struct LargeInfo {
        UIDNAInfo info;
        std::uint8_t tail[8];
    } large{};
    large.info.size = static_cast<std::int16_t>(sizeof(UIDNAInfo) + 4);
    std::fill(std::begin(large.tail), std::end(large.tail), std::uint8_t{0x77});
    large.info.reservedI3 = 9;
    status = Ok;
    uidna_nameToASCII_nid_postfix(uts46, fA_sharps16, -1, dest16, 10, &large.info, &status);
    Require(status == Ok && large.info.size == sizeof(UIDNAInfo) + 4 && large.info.reservedI3 == 0 && large.tail[0] == 0 && large.tail[3] == 0 && large.tail[4] == 0x77, "ICU clears the info bytes the size field covers");
    uidna_close_nid_postfix(uts46);
}

void CheckNullInstance() {
    char16_t dest16[10];
    UIDNAInfo info = Info();
    for (NameFn fn : {GuestToAscii, GuestToUnicode}) {
        Require(Throws<std::invalid_argument>([&] { fn(nullptr, u"a", -1, dest16, 10, &info, nullptr); }), "a null status throws");
        Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; fn(nullptr, u"a", -1, dest16, 10, &info, &ok); }), "a null instance that ICU would use throws");
        std::int32_t status = IllegalArgument;
        Require(fn(nullptr, u"a", -1, dest16, 10, &info, &status) == 0 && status == IllegalArgument, "a failed status returns before the instance is read");
        status = Ok;
        Require(fn(nullptr, u"a", -1, dest16, 10, nullptr, &status) == 0 && status == IllegalArgument, "a null info is checked before the instance");
        UIDNAInfo small = Info();
        small.size = 8;
        status = Ok;
        Require(fn(nullptr, u"a", -1, dest16, 10, &small, &status) == 0 && status == IllegalArgument, "a short info is checked before the instance");
        status = Ok;
        Require(fn(nullptr, nullptr, 1, dest16, 10, &info, &status) == 0 && status == IllegalArgument, "a null name is checked before the instance");
        status = Ok;
        Require(fn(nullptr, u"a", -3, dest16, 10, &info, &status) == 0 && status == IllegalArgument, "a bad length is checked before the instance");
        status = Ok;
        Require(fn(nullptr, u"a", -1, nullptr, 10, &info, &status) == 0 && status == IllegalArgument, "a null buffer is checked before the instance");
        status = Ok;
        Require(fn(nullptr, u"a", -1, dest16, -2, &info, &status) == 0 && status == IllegalArgument, "a bad capacity is checked before the instance");
        status = Ok;
        Require(fn(nullptr, dest16, -1, dest16, 10, &info, &status) == 0 && status == IllegalArgument, "an aliased buffer is checked before the instance");
        Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; fn(nullptr, nullptr, 0, nullptr, 0, &info, &ok); }), "a null empty name with a null instance throws");
    }
}

void CheckSweep() {
    const std::vector<std::u16string> names{
        u"", u".", u"..", u"a", u"a.", u".a", u"a..b", u"example.com", u"EXAMPLE.COM", u"fa\u00DF.de", u"xn--fa-hia.de", u"B\u00FCcher.example",
        u"xn--bcher-kva.example", u"xn--BCHER-KVA.example", u"\u65E5\u672C\u8A9E.jp", u"\u00D6BB.at", u"-a.b", u"a-.b", u"ab--c.d", u"xn--a", u"xn--ls8h",
        u"\u0080", u"a\u200Cb", u"a\u094D\u200Cb", u"\u0627\u0644\u0639\u0631\u0628\u064A\u0629.\u0645\u0635\u0631", u"a\u3002b\uFF0Ec\uFF61d", u"1.2.3.4",
        u"under_score.com", u"sp ace.com", u"\U0001F600.example", u"a\u0301.com", u"\u0301a.com", u"\u03C2.gr", u"\u03A3.gr", u"\u0BB9\u0BCD\u200D",
        u"xn--xn--a.b", u"xn---.b", u"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.com", u"\uFFFD", std::u16string(u"\0a", 2), u"a/b",
        u"\u2488.com", u"\u05D0\u05D1.c", u"a\u05D0.b", u"\u00B7l", u"l\u00B7l", u"\u30FB\u30A2"};
    const std::uint32_t optionSets[] = {
        UIDNA_DEFAULT, UIDNA_USE_STD3_RULES, UIDNA_CHECK_BIDI, UIDNA_CHECK_CONTEXTJ, UIDNA_CHECK_CONTEXTO,
        UIDNA_NONTRANSITIONAL_TO_ASCII | UIDNA_NONTRANSITIONAL_TO_UNICODE,
        UIDNA_USE_STD3_RULES | UIDNA_CHECK_BIDI | UIDNA_CHECK_CONTEXTJ | UIDNA_CHECK_CONTEXTO | UIDNA_NONTRANSITIONAL_TO_ASCII | UIDNA_NONTRANSITIONAL_TO_UNICODE};
    for (std::uint32_t options : optionSets) {
        std::int32_t status = Ok;
        UIDNA* guest = uidna_openUTS46_nid_postfix(options, &status);
        UErrorCode hostStatus = U_ZERO_ERROR;
        UIDNA* host = uidna_openUTS46(options, &hostStatus);
        Require(guest && host && status == Ok && U_SUCCESS(hostStatus), "instances open");
        for (int direction = 0; direction < 2; ++direction) {
            const NameFn guestFn = direction ? GuestToUnicode : GuestToAscii;
            const HostNameFn hostFn = direction ? HostToUnicode : HostToAscii;
            for (const auto& name : names) {
                for (std::int32_t capacity : {0, 1, 4, 64, 200}) {
                    for (bool terminated : {false, true}) {
                        char16_t guestDest[200];
                        char16_t hostDest[200];
                        std::fill(std::begin(guestDest), std::end(guestDest), 0x5555);
                        std::fill(std::begin(hostDest), std::end(hostDest), 0x5555);
                        UIDNAInfo guestInfo = Info();
                        UIDNAInfo hostInfo = Info();
                        std::int32_t guestStatus = Ok;
                        UErrorCode expectedStatus = U_ZERO_ERROR;
                        const std::int32_t length = terminated ? -1 : static_cast<std::int32_t>(name.size());
                        const std::int32_t guestLength = guestFn(guest, name.c_str(), length, capacity ? guestDest : nullptr, capacity, &guestInfo, &guestStatus);
                        const std::int32_t hostLength = hostFn(host, name.c_str(), length, capacity ? hostDest : nullptr, capacity, &hostInfo, &expectedStatus);
                        Require(guestLength == hostLength && guestStatus == expectedStatus && std::memcmp(guestDest, hostDest, sizeof(guestDest)) == 0, "names match ICU");
                        Require(std::memcmp(&guestInfo, &hostInfo, sizeof(UIDNAInfo)) == 0, "the info matches ICU");
                    }
                }
            }
        }
        uidna_close(host);
        uidna_close_nid_postfix(guest);
    }
}

}

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    CheckOpen();
    CheckKnownValues();
    CheckNullInstance();
    CheckSweep();
    return 0;
}
