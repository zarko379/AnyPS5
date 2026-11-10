#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using Version = std::array<std::uint8_t, 4>;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU version: %s\n", message);
    std::abort();
}

bool ExpectedInvariant(std::uint32_t unit) {
    if (unit > 0x7f) return false;
    for (const std::uint32_t variant : {0x0a, 0x21, 0x23, 0x24, 0x40, 0x5b, 0x5c, 0x5d, 0x5e, 0x60, 0x7b, 0x7c, 0x7d, 0x7e})
        if (unit == variant) return false;
    return true;
}

template<class F>
bool Rejects(F call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

Version FromString(const char* text) {
    Version version{9, 9, 9, 9};
    u_versionFromString_nid_postfix(version.data(), text);
    return version;
}

Version FromUString(const char16_t* text) {
    Version version{9, 9, 9, 9};
    u_versionFromUString_nid_postfix(version.data(), text);
    return version;
}

std::string ToString(Version version) {
    std::array<char, 21> text;
    text.fill('X');
    u_versionToString_nid_postfix(version.data(), text.data());
    const auto length = std::strlen(text.data());
    Require(length <= 15 && text[length + 1] == 'X', "version string overrun");
    return text.data();
}

void CharsToUChars() {
    std::array<char, 256> chars;
    for (std::size_t index = 0; index < chars.size(); ++index) chars[index] = static_cast<char>(index);
    std::array<char16_t, 257> units;
    units.fill(0x5555);
    u_charsToUChars_nid_postfix(chars.data(), units.data(), 256);
    for (std::size_t index = 0; index < chars.size(); ++index) Require(units[index] == index, "bytes widen without sign extension");
    Require(units[256] == 0x5555, "widening stays within the length");
    units.fill(0x5555);
    u_charsToUChars_nid_postfix(chars.data(), units.data(), 0);
    u_charsToUChars_nid_postfix(chars.data(), units.data(), -1);
    Require(units[0] == 0x5555, "nonpositive lengths write nothing");
    u_charsToUChars_nid_postfix(nullptr, nullptr, 0);
    u_charsToUChars_nid_postfix(nullptr, nullptr, -5);
    Require(Rejects([&] { u_charsToUChars_nid_postfix(nullptr, units.data(), 1); }), "null source rejected");
    Require(Rejects([&] { u_charsToUChars_nid_postfix(chars.data(), nullptr, 1); }), "null destination rejected");
}

void UCharsToChars() {
    std::array<char16_t, 0x10000> units;
    for (std::size_t index = 0; index < units.size(); ++index) units[index] = static_cast<char16_t>(index);
    std::array<char, 0x10001> chars;
    chars.fill('X');
    u_UCharsToChars_nid_postfix(units.data(), chars.data(), 0x10000);
    for (std::uint32_t unit = 0; unit < 0x10000; ++unit)
        Require(chars[unit] == (ExpectedInvariant(unit) ? static_cast<char>(unit) : 0), "only invariant characters narrow");
    Require(chars[0x10000] == 'X', "narrowing stays within the length");
    chars.fill('X');
    u_UCharsToChars_nid_postfix(units.data(), chars.data(), 0);
    u_UCharsToChars_nid_postfix(units.data(), chars.data(), -1);
    Require(chars[0] == 'X', "nonpositive lengths write nothing");
    u_UCharsToChars_nid_postfix(nullptr, nullptr, 0);
    Require(Rejects([&] { u_UCharsToChars_nid_postfix(nullptr, chars.data(), 1); }), "null source rejected");
    Require(Rejects([&] { u_UCharsToChars_nid_postfix(units.data(), nullptr, 1); }), "null destination rejected");
}

void AuStrcpy() {
    std::array<char, 8> chars;
    chars.fill('X');
    Require(u_austrcpy_nid_postfix(chars.data(), u"") == chars.data(), "empty copy returns destination");
    Require(chars[0] == 0 && chars[1] == 'X', "empty copy writes only terminator");

    chars.fill('X');
    Require(u_austrcpy_nid_postfix(chars.data(), u"Ab_9") == chars.data(), "ASCII copy returns destination");
    Require(std::memcmp(chars.data(), "Ab_9\0XXX", chars.size()) == 0, "ASCII characters and terminator copy");

    chars.fill('X');
    u_austrcpy_nid_postfix(chars.data(), u"A!#$@B");
    Require(std::memcmp(chars.data(), "A!#$@B\0X", chars.size()) == 0, "punctuation outside the invariant set copies unchanged");

    std::array<char16_t, 128> asciiUnits;
    for (std::size_t index = 0; index < 127; ++index) asciiUnits[index] = static_cast<char16_t>(index + 1);
    asciiUnits[127] = 0;
    std::array<char, 129> asciiChars;
    asciiChars.fill('X');
    Require(u_austrcpy_nid_postfix(asciiChars.data(), asciiUnits.data()) == asciiChars.data(), "full ASCII copy returns destination");
    for (std::size_t index = 0; index < 127; ++index)
        Require(asciiChars[index] == static_cast<char>(index + 1), "every unit in 0x01-0x7f copies unchanged");
    Require(asciiChars[127] == 0 && asciiChars[128] == 'X', "full ASCII copy writes terminator without overrun");

    Require(Rejects([&] { u_austrcpy_nid_postfix(nullptr, u"a"); }), "null destination rejected");
    Require(Rejects([&] { u_austrcpy_nid_postfix(chars.data(), nullptr); }), "null source rejected");
}

void AuStrncpy() {
    std::array<char, 8> chars;
    chars.fill('X');
    Require(u_austrncpy_nid_postfix(chars.data(), u"Abcdef", 3) == chars.data(), "truncated copy returns destination");
    Require(std::memcmp(chars.data(), "AbcXXXXX", chars.size()) == 0, "truncated copy omits terminator");

    chars.fill('X');
    u_austrncpy_nid_postfix(chars.data(), u"Abc", 3);
    Require(std::memcmp(chars.data(), "AbcXXXXX", chars.size()) == 0, "exact-length copy omits terminator");

    chars.fill('X');
    u_austrncpy_nid_postfix(chars.data(), u"Abc", 6);
    Require(std::memcmp(chars.data(), "Abc\0XXXX", chars.size()) == 0, "longer bound writes one terminator without padding");

    chars.fill('X');
    u_austrncpy_nid_postfix(chars.data(), u"A!#$@", 7);
    Require(std::memcmp(chars.data(), "A!#$@\0XX", chars.size()) == 0, "non-invariant ASCII units copy unchanged within bound");

    std::array<char16_t, 128> asciiUnits;
    for (std::size_t index = 0; index < 127; ++index) asciiUnits[index] = static_cast<char16_t>(index + 1);
    asciiUnits[127] = 0;
    std::array<char, 129> asciiChars;
    asciiChars.fill('X');
    Require(u_austrncpy_nid_postfix(asciiChars.data(), asciiUnits.data(), 128) == asciiChars.data(), "bounded full ASCII copy returns destination");
    for (std::size_t index = 0; index < 127; ++index)
        Require(asciiChars[index] == static_cast<char>(index + 1), "every unit in 0x01-0x7f copies unchanged in bounded copy");
    Require(asciiChars[127] == 0 && asciiChars[128] == 'X', "bounded full ASCII copy writes terminator without overrun");

    const std::array<char16_t, 3> unterminated{u'!', u'$', u'@'};
    chars.fill('X');
    u_austrncpy_nid_postfix(chars.data(), unterminated.data(), 3);
    Require(std::memcmp(chars.data(), "!$@XXXXX", chars.size()) == 0, "bounded copy stops at count without reading past it");

    chars.fill('X');
    Require(u_austrncpy_nid_postfix(chars.data(), u"Abc", 0) == chars.data(), "zero count returns destination");
    Require(u_austrncpy_nid_postfix(chars.data(), u"Abc", -2) == chars.data(), "negative count returns destination");
    Require(chars[0] == 'X', "nonpositive counts write nothing");
    Require(u_austrncpy_nid_postfix(nullptr, nullptr, 0) == nullptr, "zero count allows null pointers");
    Require(u_austrncpy_nid_postfix(chars.data(), nullptr, -1) == chars.data(), "negative count allows null source");
    Require(Rejects([&] { u_austrncpy_nid_postfix(nullptr, u"a", 1); }), "null destination with positive count rejected");
    Require(Rejects([&] { u_austrncpy_nid_postfix(chars.data(), nullptr, 1); }), "null source with positive count rejected");
}

void VersionFromString() {
    Require(FromString("1.2.3.4") == Version{1, 2, 3, 4}, "four parts");
    Require(FromString("65.1") == Version{65, 1, 0, 0}, "missing parts are zero");
    Require(FromString("1.2.3.4.5") == Version{1, 2, 3, 4}, "parts after the fourth are ignored");
    Require(FromString("") == Version{0, 0, 0, 0}, "empty string");
    Require(FromString("x") == Version{0, 0, 0, 0}, "no digits");
    Require(FromString("3x.4") == Version{3, 0, 0, 0}, "parsing stops at a non-delimiter");
    Require(FromString("1..2") == Version{1, 0, 0, 0}, "empty part stops parsing");
    Require(FromString("1.2.") == Version{1, 2, 0, 0}, "trailing delimiter");
    Require(FromString(" 7. 8") == Version{7, 8, 0, 0}, "leading whitespace is skipped by strtoul");
    Require(FromString("+5.-1") == Version{5, 255, 0, 0}, "signs follow strtoul");
    Require(FromString("256.300.511") == Version{0, 44, 255, 0}, "parts truncate to eight bits");
    Require(FromString("99999999999999999999999.1") == Version{255, 1, 0, 0}, "overflow saturates before truncation");
    Require(FromString(nullptr) == Version{0, 0, 0, 0}, "null string clears the version");
    u_versionFromString_nid_postfix(nullptr, "1.2");
}

void VersionFromUString() {
    Require(FromUString(u"10.2.1") == Version{10, 2, 1, 0}, "UTF-16 version");
    Require(FromUString(u"") == Version{0, 0, 0, 0}, "empty UTF-16 string");
    Require(FromUString(u"1$2") == Version{1, 0, 0, 0}, "variant character becomes NUL");
    Require(FromUString(u"4.é5") == Version{4, 0, 0, 0}, "non-ASCII character becomes NUL");
    Require(FromUString(u"１.2") == Version{0, 0, 0, 0}, "fullwidth digit is not a digit");
    Require(FromUString((std::u16string(17, u'0') + u"1.2").c_str()) == Version{1, 2, 0, 0}, "twenty units are read");
    Require(FromUString((std::u16string(18, u'0') + u"1.2").c_str()) == Version{1, 0, 0, 0}, "units after the twentieth are ignored");
    Require(FromUString(nullptr) == Version{9, 9, 9, 9}, "null string leaves the version unchanged");
    u_versionFromUString_nid_postfix(nullptr, u"1.2");
}

void VersionToString() {
    Require(ToString({1, 2, 3, 4}) == "1.2.3.4", "four parts");
    Require(ToString({65, 1, 0, 0}) == "65.1", "trailing zero parts are omitted");
    Require(ToString({7, 0, 0, 0}) == "7.0", "at least two parts");
    Require(ToString({0, 0, 0, 0}) == "0.0", "zero version");
    Require(ToString({1, 0, 0, 5}) == "1.0.0.5", "inner zero parts are kept");
    Require(ToString({0, 0, 1, 0}) == "0.0.1", "leading zero parts are kept");
    Require(ToString({255, 199, 99, 10}) == "255.199.99.10", "multi-digit parts");
    Require(ToString({100, 105, 109, 110}) == "10.15.19.110", "a zero tens digit after the hundreds digit is dropped");
    std::array<char, 4> text{'X', 'X', 'X', 'X'};
    u_versionToString_nid_postfix(nullptr, text.data());
    Require(text[0] == 0 && text[1] == 'X', "null version writes an empty string");
    const Version version{1, 2, 0, 0};
    u_versionToString_nid_postfix(version.data(), nullptr);
}

void GetVersion() {
    Version version{9, 9, 9, 9};
    u_getVersion_nid_postfix(version.data());
    Require(version == Version{78, 1, 0, 0}, "library version is 78.1.0.0");
    Require(ToString(version) == "78.1", "library version string");
    u_getVersion_nid_postfix(nullptr);
}

void RoundTrip() {
    const auto written = [](std::uint32_t part) { return part < 100 || part % 100 >= 10; };
    for (std::uint32_t part = 0; part < 256; ++part) {
        if (!written(part) || !written(255 - part)) continue;
        const Version version{static_cast<std::uint8_t>(part), 3, static_cast<std::uint8_t>(255 - part), 1};
        Require(FromString(ToString(version).c_str()) == version, "string round trip");
    }
}

void InvariantStrings() {
    for (std::uint32_t unit = 1; unit <= 0xff; ++unit) {
        const char counted[] = {static_cast<char>(unit)};
        const char terminated[] = {static_cast<char>(unit), 0};
        Require(uprv_isInvariantString_nid_postfix(counted, 1) == (ExpectedInvariant(unit) ? 1 : 0), "counted byte classification");
        Require(uprv_isInvariantString_nid_postfix(terminated, -1) == (ExpectedInvariant(unit) ? 1 : 0), "terminated byte classification");
    }
    for (std::uint32_t unit = 0; unit <= 0xffff; ++unit) {
        const char16_t counted[] = {static_cast<char16_t>(unit)};
        Require(uprv_isInvariantUString_nid_postfix(counted, 1) == (ExpectedInvariant(unit) ? 1 : 0), "counted unit classification");
        if (unit == 0) continue;
        const char16_t terminated[] = {static_cast<char16_t>(unit), 0};
        Require(uprv_isInvariantUString_nid_postfix(terminated, -1) == (ExpectedInvariant(unit) ? 1 : 0), "terminated unit classification");
    }
    Require(uprv_isInvariantString_nid_postfix("ab\0!", 4) == 0, "counted bytes continue past NUL");
    Require(uprv_isInvariantString_nid_postfix("ab\0!", 3) == 1, "counted bytes stop at the length");
    Require(uprv_isInvariantString_nid_postfix("ab\0!", -1) == 1, "terminated bytes stop at NUL");
    Require(uprv_isInvariantString_nid_postfix("ab\0!", -7) == 1, "every negative length means terminated");
    Require(uprv_isInvariantString_nid_postfix("", -1) == 1, "empty string is invariant");
    Require(uprv_isInvariantString_nid_postfix(nullptr, 0) == 1, "zero length reads nothing");
    Require(Rejects([] { uprv_isInvariantString_nid_postfix(nullptr, -1); }), "null terminated bytes rejected");
    Require(Rejects([] { uprv_isInvariantString_nid_postfix(nullptr, 1); }), "null counted bytes rejected");
    Require(uprv_isInvariantUString_nid_postfix(u"ab\0~", 4) == 0, "counted units continue past NUL");
    Require(uprv_isInvariantUString_nid_postfix(u"ab\0~", 3) == 1, "counted units stop at the length");
    Require(uprv_isInvariantUString_nid_postfix(u"ab\0~", -1) == 1, "terminated units stop at NUL");
    Require(uprv_isInvariantUString_nid_postfix(u"aé", -1) == 0, "non-ASCII units are variant");
    Require(uprv_isInvariantUString_nid_postfix(nullptr, 0) == 1, "zero units read nothing");
    Require(Rejects([] { uprv_isInvariantUString_nid_postfix(nullptr, -1); }), "null terminated units rejected");
    Require(Rejects([] { uprv_isInvariantUString_nid_postfix(nullptr, 2); }), "null counted units rejected");
}

char LowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + 32) : value;
}

void EbcdicCharacters() {
    const std::pair<std::uint8_t, char> pinned[] = {{0xc1, 'A'}, {0xe9, 'Z'}, {0x81, 'a'}, {0xa9, 'z'}, {0xf0, '0'}, {0xf9, '9'},
                                                    {0x40, ' '}, {0x15, '\n'}, {0x25, '\n'}, {0xe0, '\\'}, {0xad, '['}, {0xba, '['},
                                                    {0xbd, ']'}, {0xbb, ']'}, {0x5f, '^'}, {0xb0, '^'}, {0xa1, '~'}, {0x6d, '_'},
                                                    {0x00, 0}, {0x41, 0}, {0xff, 0}, {0x04, 0}, {0x07, 0x7f}};
    for (const auto& [ebcdic, ascii] : pinned) Require(uprv_ebcdicToAscii_nid_postfix(static_cast<char>(ebcdic)) == ascii, "pinned EBCDIC byte");
    for (std::uint32_t unit = 0; unit <= 0xff; ++unit) {
        const char ebcdic = static_cast<char>(unit);
        const char ascii = uprv_ebcdicToAscii_nid_postfix(ebcdic);
        const char lower = uprv_ebcdicToLowercaseAscii_nid_postfix(ebcdic);
        if (unit == 0xe0) Require(ascii == '\\' && lower == '|', "EBCDIC E0 lowercases to a vertical line");
        else Require(lower == LowerAscii(ascii), "lowercase table lowercases the ASCII table");
    }
    for (std::uint32_t unit = 1; unit <= 0x7f; ++unit) {
        const std::uint8_t ascii[] = {static_cast<std::uint8_t>(unit), 0};
        std::uint8_t ebcdic[2] = {0xee, 0xee};
        std::uint8_t back[2] = {0xee, 0xee};
        Require(uprv_eastrncpy_nid_postfix(ebcdic, ascii, 1) == ebcdic, "eastrncpy returns its destination");
        if (!ExpectedInvariant(unit)) {
            Require(ebcdic[0] == 0x6f && ebcdic[1] == 0xee, "variant ASCII becomes the EBCDIC question mark");
            continue;
        }
        Require(ebcdic[0] != 0x6f || unit == '?', "invariant ASCII maps");
        Require(uprv_aestrncpy_nid_postfix(back, ebcdic, 1) == back, "aestrncpy returns its destination");
        Require(back[0] == unit && back[1] == 0xee, "invariant ASCII round-trips through EBCDIC");
        Require(static_cast<std::uint8_t>(uprv_ebcdicToAscii_nid_postfix(static_cast<char>(ebcdic[0]))) == unit, "single character conversion agrees");
    }
    std::uint8_t high[2] = {0xee, 0xee};
    const std::uint8_t highSource[] = {0x80, 0};
    uprv_eastrncpy_nid_postfix(high, highSource, 1);
    Require(high[0] == 0x6f && high[1] == 0xee, "bytes above ASCII become the EBCDIC question mark");
}

void EbcdicCopies() {
    const std::uint8_t hello[] = {0xc8, 0x85, 0x93, 0x93, 0x96, 0};
    std::array<std::uint8_t, 10> out{};
    out.fill(0xee);
    Require(uprv_aestrncpy_nid_postfix(out.data(), hello, -1) == out.data(), "aestrncpy returns its destination");
    Require(std::memcmp(out.data(), "Hello\0", 6) == 0 && out[6] == 0xee, "minus one copies the terminator");
    out.fill(0xee);
    uprv_aestrncpy_nid_postfix(out.data(), hello, 3);
    Require(std::memcmp(out.data(), "Hel", 3) == 0 && out[3] == 0xee, "a short count does not terminate");
    out.fill(0xee);
    uprv_aestrncpy_nid_postfix(out.data(), hello, 8);
    Require(std::memcmp(out.data(), "Hello\0\0\0", 8) == 0 && out[8] == 0xee, "a long count pads with zeros");
    out.fill(0xee);
    for (const std::int32_t count : {0, -2, std::numeric_limits<std::int32_t>::min()}) {
        Require(uprv_aestrncpy_nid_postfix(out.data(), hello, count) == out.data(), "nonpositive count returns the destination");
        Require(uprv_eastrncpy_nid_postfix(out.data(), hello, count) == out.data(), "nonpositive count returns the destination");
        Require(out[0] == 0xee, "only minus one means terminated");
    }
    const std::uint8_t unmapped[] = {0x41, 0xc1, 0};
    uprv_aestrncpy_nid_postfix(out.data(), unmapped, -1);
    Require(out[0] == 0 && out[1] == 'A' && out[2] == 0, "unmapped EBCDIC bytes become zero without stopping the copy");
    const std::uint8_t hi[] = {'H', 'i', '!', 0};
    out.fill(0xee);
    uprv_eastrncpy_nid_postfix(out.data(), hi, 5);
    Require(out[0] == 0xc8 && out[1] == 0x89 && out[2] == 0x6f && out[3] == 0 && out[4] == 0 && out[5] == 0xee, "eastrncpy maps, substitutes and pads");
    out.fill(0xee);
    uprv_eastrncpy_nid_postfix(out.data(), hi, -1);
    Require(out[2] == 0x6f && out[3] == 0 && out[4] == 0xee, "eastrncpy minus one copies the terminator");
    Require(uprv_aestrncpy_nid_postfix(nullptr, hello, 0) == nullptr, "a null destination is not written for a zero count");
    Require(Rejects([&] { uprv_aestrncpy_nid_postfix(out.data(), nullptr, 0); }), "a null source is always read");
    Require(Rejects([&] { uprv_eastrncpy_nid_postfix(out.data(), nullptr, -2); }), "a null source is always read");
    Require(Rejects([&] { uprv_aestrncpy_nid_postfix(nullptr, hello, 1); }), "a null destination is rejected when written");
    Require(Rejects([&] { uprv_eastrncpy_nid_postfix(nullptr, hi, -1); }), "a null destination is rejected when written");
}

std::int32_t EbcdicKey(std::uint8_t unit) {
    if (unit == 0) return 0;
    const auto ascii = static_cast<std::uint8_t>(uprv_ebcdicToAscii_nid_postfix(static_cast<char>(unit)));
    return ascii != 0 && ExpectedInvariant(ascii) ? ascii : -static_cast<std::int32_t>(unit);
}

void EbcdicComparison() {
    for (std::uint32_t first = 0; first <= 0xff; ++first) {
        for (std::uint32_t second = 0; second <= 0xff; ++second) {
            const char left[] = {static_cast<char>(first), 0};
            const char right[] = {static_cast<char>(second), 0};
            const std::int32_t expected = first == second ? 0 : EbcdicKey(first) - EbcdicKey(second);
            Require(uprv_compareInvEbcdicAsAscii_nid_postfix(left, right) == expected, "single EBCDIC bytes compare as invariant ASCII");
        }
    }
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix("\xc1", "\xc2") == -1, "A sorts before B");
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix("\xc1", "\x81") == 'A' - 'a', "uppercase sorts before lowercase as in ASCII");
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix("\xc1", "") == 'A', "a longer string is larger by its next character");
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix("\x5a", "\xc1") == -0x5a - 'A', "variant characters sort below others");
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix("\x41", "\x42") == 1, "unmapped bytes compare by negated value");
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix("\xc1\xc2", "\xc1\xc3") == -1, "the first difference decides");
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix("\xc1\xc2", "\xc1\xc2") == 0, "equal strings");
    Require(Rejects([] { uprv_compareInvEbcdicAsAscii_nid_postfix(nullptr, ""); }), "null first string rejected");
    Require(Rejects([] { uprv_compareInvEbcdicAsAscii_nid_postfix("", nullptr); }), "null second string rejected");
}

void PropertyNames() {
    for (std::uint32_t unit = 1; unit <= 0xff; ++unit) {
        const char name[] = {static_cast<char>(unit), 0};
        const bool asciiIgnored = unit == '-' || unit == '_' || unit == ' ' || (unit >= 0x09 && unit <= 0x0d);
        const std::int32_t asciiLower = unit >= 'A' && unit <= 'Z' ? static_cast<std::int32_t>(unit + 32) : static_cast<std::int32_t>(unit);
        Require(uprv_compareASCIIPropertyNames_nid_postfix(name, "") == (asciiIgnored ? 0 : asciiLower), "ASCII name byte");
        Require(uprv_compareASCIIPropertyNames_nid_postfix("", name) == (asciiIgnored ? 0 : -asciiLower), "ASCII name byte on the right");
        const bool ebcdicIgnored = unit == 0x60 || unit == 0x6d || unit == 0x40 || unit == 0x05 || unit == 0x15 || unit == 0x25 ||
                                   (unit >= 0x0b && unit <= 0x0d);
        const bool ebcdicUpper = (unit >= 0xc1 && unit <= 0xc9) || (unit >= 0xd1 && unit <= 0xd9) || (unit >= 0xe2 && unit <= 0xe9);
        const std::int32_t ebcdicLower = ebcdicUpper ? static_cast<std::int32_t>(unit - 0x40) : static_cast<std::int32_t>(unit);
        Require(uprv_compareEBCDICPropertyNames_nid_postfix(name, "") == (ebcdicIgnored ? 0 : ebcdicLower), "EBCDIC name byte");
    }
    Require(uprv_compareASCIIPropertyNames_nid_postfix("General_Category", "generalcategory") == 0, "loose ASCII match");
    Require(uprv_compareASCIIPropertyNames_nid_postfix(" -a_B\t\n\v\f\rc ", "ABC") == 0, "ASCII delimiters and white space are ignored");
    Require(uprv_compareASCIIPropertyNames_nid_postfix("--a", "a") == 0, "different skip counts still match");
    Require(uprv_compareASCIIPropertyNames_nid_postfix("a", "b") == -1, "lowercased difference");
    Require(uprv_compareASCIIPropertyNames_nid_postfix("ab", "A") == 'b', "longer ASCII name is larger by its next character");
    Require(uprv_compareASCIIPropertyNames_nid_postfix("Z", "[") == 'z' - '[', "ASCII letters compare lowercased");
    Require(uprv_compareASCIIPropertyNames_nid_postfix("\xff", "a") == 0xff - 'a', "high bytes compare unsigned");
    Require(uprv_compareASCIIPropertyNames_nid_postfix("\x85", "") == 0x85, "NEL is not ignored in ASCII");
    Require(uprv_compareASCIIPropertyNames_nid_postfix("", "-_ ") == 0, "only delimiters equal empty");
    Require(uprv_compareEBCDICPropertyNames_nid_postfix("\xc1\x6d\xc2\x40\x60\x05\x15\x25\x0b\x0c\x0d\xc3", "\x81\x82\x83") == 0, "loose EBCDIC match");
    Require(uprv_compareEBCDICPropertyNames_nid_postfix("\xc1", "\x82") == -1, "EBCDIC letters compare lowercased");
    Require(uprv_compareEBCDICPropertyNames_nid_postfix("-", "") == '-', "ASCII hyphen is not ignored in EBCDIC");
    Require(uprv_compareEBCDICPropertyNames_nid_postfix("\x09", "") == 9, "ASCII tab is not ignored in EBCDIC");
    Require(Rejects([] { uprv_compareASCIIPropertyNames_nid_postfix(nullptr, "a"); }), "null ASCII name rejected");
    Require(Rejects([] { uprv_compareASCIIPropertyNames_nid_postfix("a", nullptr); }), "null ASCII name rejected");
    Require(Rejects([] { uprv_compareEBCDICPropertyNames_nid_postfix(nullptr, "a"); }), "null EBCDIC name rejected");
    Require(Rejects([] { uprv_compareEBCDICPropertyNames_nid_postfix("a", nullptr); }), "null EBCDIC name rejected");
}

}

int main() {
    CharsToUChars();
    UCharsToChars();
    AuStrcpy();
    AuStrncpy();
    VersionFromString();
    VersionFromUString();
    VersionToString();
    GetVersion();
    RoundTrip();
    InvariantStrings();
    EbcdicCharacters();
    EbcdicCopies();
    EbcdicComparison();
    PropertyNames();
    return 0;
}
