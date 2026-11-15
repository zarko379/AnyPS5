#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

using Jsc::WtfAbi::ConversionMode;
using Jsc::WtfAbi::String;
using Jsc::WtfAbi::StringImpl;
using Jsc::WtfAbi::UTF8ConversionError;

using NullStringFunction = const String* (APS5_VABI*)();
using ToUIntStrictFunction = unsigned (APS5_VABI*)(const String*, bool*, int);
using Utf8ImplFunction = UTF8ConversionError (APS5_VABI*)(const char16_t*, unsigned, char*&, std::size_t, ConversionMode);

static_assert(std::is_same_v<decltype(&_ZN3WTF10nullStringEv), NullStringFunction>);
static_assert(std::is_same_v<decltype(&_ZNK3WTF6String12toUIntStrictEPbi), ToUIntStrictFunction>);
static_assert(std::is_same_v<decltype(&_ZN3WTF10StringImpl8utf8ImplEPKDsjRPcmNS_14ConversionModeE), Utf8ImplFunction>);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF string conversion check failed at line %d\n", line);
        std::abort();
    }
}

template <typename Exception, typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

StringImpl Make8(const char* text) {
    return {2, static_cast<unsigned>(std::strlen(text)), text, Jsc::WtfAbi::StringImplFlag8Bit};
}

StringImpl Make16(const std::u16string& text) {
    return {2, static_cast<unsigned>(text.size()), text.data(), 0};
}

struct Parsed {
    unsigned value;
    bool ok;
};

Parsed Parse(const StringImpl& impl, int base = 10) {
    String string{&impl};
    bool ok = true;
    const unsigned value = _ZNK3WTF6String12toUIntStrictEPbi(&string, &ok, base);
    return {value, ok};
}

Parsed Parse8(const char* text, int base = 10) {
    const StringImpl impl = Make8(text);
    return Parse(impl, base);
}

Parsed Parse16(const std::u16string& text, int base = 10) {
    const StringImpl impl = Make16(text);
    return Parse(impl, base);
}

bool Accepts(Parsed parsed, unsigned value) {
    return parsed.ok && parsed.value == value;
}

bool Rejects(Parsed parsed) {
    return !parsed.ok && parsed.value == 0;
}

void TestNullString() {
    const String* first = _ZN3WTF10nullStringEv();
    const String* second = _ZN3WTF10nullStringEv();
    Check(first != nullptr, __LINE__);
    Check(first == second, __LINE__);
    Check(first->impl == nullptr, __LINE__);
}

void TestToUIntStrict() {
    String null{nullptr};
    bool ok = true;
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&null, &ok, 10) == 0 && !ok, __LINE__);
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&null, nullptr, 10) == 0, __LINE__);
    ok = true;
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&null, &ok, 0) == 0 && !ok, __LINE__);

    Check(Accepts(Parse8("123"), 123), __LINE__);
    Check(Accepts(Parse8("0"), 0), __LINE__);
    Check(Accepts(Parse8(" \t\n\v\f\r+42 \r\n"), 42), __LINE__);
    Check(Accepts(Parse8("4294967295"), 4294967295u), __LINE__);
    Check(Accepts(Parse8("00004294967295"), 4294967295u), __LINE__);
    Check(Rejects(Parse8("4294967296")), __LINE__);
    Check(Rejects(Parse8("42949672950")), __LINE__);
    Check(Rejects(Parse8("-1")), __LINE__);
    Check(Rejects(Parse8("-0")), __LINE__);
    Check(Rejects(Parse8("+")), __LINE__);
    Check(Rejects(Parse8("++1")), __LINE__);
    Check(Rejects(Parse8("")), __LINE__);
    Check(Rejects(Parse8("   ")), __LINE__);
    Check(Rejects(Parse8("12a")), __LINE__);
    Check(Rejects(Parse8("1 2")), __LINE__);
    Check(Rejects(Parse8("\xA0" "5")), __LINE__);
    Check(Rejects(Parse8("5\x85")), __LINE__);
    Check(Rejects(Parse8("\x1F" "5")), __LINE__);

    Check(Accepts(Parse8("ff", 16), 255), __LINE__);
    Check(Accepts(Parse8("FF", 16), 255), __LINE__);
    Check(Accepts(Parse8("ffffffff", 16), 0xFFFFFFFFu), __LINE__);
    Check(Rejects(Parse8("100000000", 16)), __LINE__);
    Check(Rejects(Parse8("g", 16)), __LINE__);
    Check(Rejects(Parse8("a", 10)), __LINE__);
    Check(Accepts(Parse8("z", 36), 35), __LINE__);
    Check(Accepts(Parse8("Z", 36), 35), __LINE__);
    Check(Accepts(Parse8("z", 40), 35), __LINE__);
    Check(Accepts(Parse8("10", 40), 40), __LINE__);
    Check(Rejects(Parse8("{", 40)), __LINE__);
    Check(Rejects(Parse8("8", 8)), __LINE__);
    Check(Accepts(Parse8("777", 8), 511), __LINE__);
    Check(Accepts(Parse8("11111111111111111111111111111111", 2), 0xFFFFFFFFu), __LINE__);
    Check(Rejects(Parse8("111111111111111111111111111111111", 2)), __LINE__);
    Check(Accepts(Parse8("0", 1), 0), __LINE__);
    Check(Accepts(Parse8("000", 1), 0), __LINE__);
    Check(Rejects(Parse8("1", 1)), __LINE__);
    Check(Rejects(Parse8("1", -1)), __LINE__);
    Check(Rejects(Parse8("0", -10)), __LINE__);

    Check(Accepts(Parse16(u"789"), 789), __LINE__);
    Check(Accepts(Parse16(u"　 +5    "), 5), __LINE__);
    Check(Rejects(Parse16(u" 5")), __LINE__);
    Check(Rejects(Parse16(u" 5")), __LINE__);
    Check(Rejects(Parse16(u"᠎5")), __LINE__);
    Check(Rejects(Parse16(u"5​")), __LINE__);
    Check(Rejects(Parse16(u"５")), __LINE__);
    Check(Rejects(Parse16(u"İ", 36)), __LINE__);
    Check(Accepts(Parse16(u"Ab", 16), 0xAB), __LINE__);

    const StringImpl digits = Make8("17");
    String string{&digits};
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&string, nullptr, 10) == 17, __LINE__);
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&string, nullptr, 16) == 23, __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF6String12toUIntStrictEPbi(&string, nullptr, 0); }), __LINE__);
    Check(Throws<std::invalid_argument>([] { _ZNK3WTF6String12toUIntStrictEPbi(nullptr, nullptr, 10); }), __LINE__);
}

struct Converted {
    UTF8ConversionError error;
    std::string output;
};

Converted Utf8(const std::u16string& input, ConversionMode mode, std::size_t bufferSize = 64) {
    char storage[64];
    std::memset(storage, 0x5A, sizeof(storage));
    char* buffer = storage;
    const UTF8ConversionError error = _ZN3WTF10StringImpl8utf8ImplEPKDsjRPcmNS_14ConversionModeE(input.data(), static_cast<unsigned>(input.size()), buffer,
                                                                                                   bufferSize, mode);
    return {error, std::string(storage, buffer)};
}

void TestUtf8Impl() {
    const auto lenient = Jsc::WtfAbi::LenientConversion;
    const auto strict = Jsc::WtfAbi::StrictConversion;
    const auto replace = Jsc::WtfAbi::StrictConversionReplacingUnpairedSurrogatesWithFFFD;

    Converted result = Utf8(u"abc", lenient);
    Check(result.error == UTF8ConversionError::None && result.output == "abc", __LINE__);
    result = Utf8(u"é€\U0001F600", strict);
    Check(result.error == UTF8ConversionError::None && result.output == "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", __LINE__);

    result = Utf8(u"a\xDC00" u"b", lenient);
    Check(result.error == UTF8ConversionError::None && result.output == "a\xEF\xBF\xBD" "b", __LINE__);
    result = Utf8(u"a\xD800" u"b", lenient);
    Check(result.error == UTF8ConversionError::None && result.output == "a\xEF\xBF\xBD" "b", __LINE__);
    result = Utf8(u"a\xD800", lenient);
    Check(result.error == UTF8ConversionError::None && result.output == "a\xED\xA0\x80", __LINE__);
    result = Utf8(u"a\xDBFF", lenient);
    Check(result.error == UTF8ConversionError::None && result.output == "a\xED\xAF\xBF", __LINE__);
    result = Utf8(u"a\xDC00", lenient);
    Check(result.error == UTF8ConversionError::None && result.output == "a\xEF\xBF\xBD", __LINE__);

    result = Utf8(u"a\xDC00" u"b", strict);
    Check(result.error == UTF8ConversionError::IllegalSource && result.output == "a", __LINE__);
    result = Utf8(u"ab\xD800" u"c", strict);
    Check(result.error == UTF8ConversionError::IllegalSource && result.output == "ab", __LINE__);
    result = Utf8(u"a\xD800", strict);
    Check(result.error == UTF8ConversionError::SourceExhausted && result.output == "a", __LINE__);

    result = Utf8(u"a\xDC00" u"b\xD800", replace);
    Check(result.error == UTF8ConversionError::None && result.output == "a\xEF\xBF\xBD" "b\xEF\xBF\xBD", __LINE__);
    result = Utf8(u"\xD800x\xD83D\xDE00", replace);
    Check(result.error == UTF8ConversionError::None && result.output == "\xEF\xBF\xBDx\xF0\x9F\x98\x80", __LINE__);
    result = Utf8(u"\xDC00\xDC00", replace);
    Check(result.error == UTF8ConversionError::None && result.output == "\xEF\xBF\xBD\xEF\xBF\xBD", __LINE__);

    result = Utf8(u"a\xD800", static_cast<ConversionMode>(7));
    Check(result.error == UTF8ConversionError::None && result.output == "a\xED\xA0\x80", __LINE__);
    result = Utf8(u"a\xDC00", static_cast<ConversionMode>(-1));
    Check(result.error == UTF8ConversionError::None && result.output == "a\xEF\xBF\xBD", __LINE__);

    char* untouched = nullptr;
    Check(_ZN3WTF10StringImpl8utf8ImplEPKDsjRPcmNS_14ConversionModeE(nullptr, 0, untouched, 0, strict) == UTF8ConversionError::None, __LINE__);
    Check(untouched == nullptr, __LINE__);
    result = Utf8(u"", replace, 0);
    Check(result.error == UTF8ConversionError::None && result.output.empty(), __LINE__);
    result = Utf8(u"é", lenient, 2);
    Check(result.error == UTF8ConversionError::None && result.output == "\xC3\xA9", __LINE__);
    result = Utf8(u"a\xD800", lenient, 4);
    Check(result.error == UTF8ConversionError::None && result.output == "a\xED\xA0\x80", __LINE__);

    Check(Throws<std::logic_error>([] { Utf8(u"é", lenient, 1); }), __LINE__);
    Check(Throws<std::logic_error>([] { Utf8(u"€", strict, 2); }), __LINE__);
    Check(Throws<std::logic_error>([] { Utf8(u"a", strict, 0); }), __LINE__);
    Check(Throws<std::logic_error>([] { Utf8(u"\xDC00", replace, 2); }), __LINE__);
    Check(Throws<std::logic_error>([] { Utf8(u"a\xD800", lenient, 3); }), __LINE__);
}

}

int main() {
    TestNullString();
    TestToUIntStrict();
    TestUtf8Impl();
    std::puts("WTF string conversion tests passed");
    return 0;
}
