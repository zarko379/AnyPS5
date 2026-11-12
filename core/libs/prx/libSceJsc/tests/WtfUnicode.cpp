#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

using Jsc::WtfAbi::ConversionResult;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF Unicode check failed at line %d\n", line);
        std::abort();
    }
}

template<typename Exception, typename Function> bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

template<typename T> T* Fake(std::uintptr_t address) {
    return reinterpret_cast<T*>(address);
}

struct Utf16Result {
    bool ok;
    std::u16string text;
    bool ascii;
    bool advanced;
};

Utf16Result ToUTF16(const std::string& input, bool replace, std::size_t capacity = 64) {
    char16_t buffer[80] = {};
    char16_t* target = buffer;
    bool ascii = false;
    const char* begin = input.data();
    const char* end = begin + input.size();
    const bool ok = replace ? _ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb(begin, end, &target, buffer + capacity, &ascii)
                            : _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(begin, end, &target, buffer + capacity, &ascii);
    return {ok, std::u16string(buffer, static_cast<std::size_t>(target - buffer)), ascii, target != buffer};
}

void TestUTF8ToUTF16() {
    Utf16Result result = ToUTF16("Hello", false);
    Check(result.ok && result.text == u"Hello" && result.ascii, __LINE__);
    result = ToUTF16("h\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\xF4\x8F\xBF\xBF", false);
    Check(result.ok && result.text == u"hé€\U0001F600\U0010FFFF" && !result.ascii, __LINE__);
    result = ToUTF16("a\xC2\x80\xC3\xBF", false);
    Check(result.ok && result.text == u"a\u0080ÿ" && !result.ascii, __LINE__);
    result = ToUTF16("\x7F", false);
    Check(result.ok && result.text == u"\u007F" && result.ascii, __LINE__);
    result = ToUTF16("\xEF\xBF\xBF\xDF\xBF\xE0\xA0\x80", false);
    Check(result.ok && result.text == u"￿߿ࠀ", __LINE__);

    const char* invalid[] = {"\xC0\x80", "\xC1\xBF", "\xE0\x80\x80", "\xED\xA0\x80", "\xF0\x80\x80\x80", "\xF4\x90\x80\x80", "\xF5\x80\x80\x80", "\xFF", "\x80",
                             "\xE2\x82", "\xF0\x9F\x98", "\xC3", "ab\xC3" "A"};
    for (const char* text : invalid) {
        result = ToUTF16(std::string("x") + text, false);
        Check(!result.ok && !result.advanced && !result.ascii, __LINE__);
    }

    char16_t buffer[4] = {0x1111, 0x2222, 0x3333, 0x4444};
    char16_t* target = buffer;
    bool ascii = false;
    const char* empty = "";
    Check(_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(empty, empty, &target, buffer + 4, &ascii), __LINE__);
    Check(target == buffer && ascii && buffer[0] == 0x1111, __LINE__);
    ascii = false;
    Check(_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(nullptr, nullptr, &target, buffer + 4, &ascii) && target == buffer && ascii, __LINE__);
    ascii = false;
    Check(_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(empty + 1, empty, &target, buffer + 4, &ascii) && target == buffer && ascii, __LINE__);
    const char* abc = "abc";
    Check(_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(abc, abc + 3, &target, buffer + 4, nullptr), __LINE__);
    Check(target == buffer + 3 && buffer[0] == 'a' && buffer[2] == 'c' && buffer[3] == 0x4444, __LINE__);

    target = buffer;
    const char* emoji = "\xF0\x9F\x98\x80";
    Check(_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(emoji, emoji + 4, &target, buffer + 2, nullptr) && target == buffer + 2, __LINE__);
    target = buffer;
    Check(!_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(emoji, emoji + 4, &target, buffer + 1, nullptr) && target == buffer, __LINE__);
    const char* aEmoji = "a\xF0\x9F\x98\x80";
    buffer[1] = 0x5555;
    Check(!_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(aEmoji, aEmoji + 5, &target, buffer + 1, nullptr) && target == buffer, __LINE__);
    Check(!_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(aEmoji, aEmoji + 5, &target, buffer + 2, nullptr) && target == buffer, __LINE__);
    Check(buffer[0] == 'a' && buffer[1] == 0x5555, __LINE__);
    Check(!_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(emoji, emoji + 4, &target, buffer, nullptr) && target == buffer, __LINE__);
    Check(!_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(emoji, emoji + 4, &target, buffer - 1, nullptr) && target == buffer, __LINE__);

    buffer[2] = 0x6666;
    Check(Throws<std::out_of_range>([&] { _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(abc, abc + 3, &target, buffer + 2, nullptr); }), __LINE__);
    Check(target == buffer && buffer[2] == 0x6666, __LINE__);
    const char* aE9 = "a\xC3\xA9";
    Check(Throws<std::out_of_range>([&] { _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(aE9, aE9 + 3, &target, buffer + 1, nullptr); }), __LINE__);
    Check(Throws<std::out_of_range>([&] { _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(abc, abc + 1, &target, buffer, nullptr); }), __LINE__);

    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(abc, abc + 1, nullptr, buffer, nullptr); }), __LINE__);
    Check(Throws<std::length_error>([&] { _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(Fake<const char>(0x1000), Fake<const char>(0x1000 + 0x80000000ull), nullptr, buffer, nullptr); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(Fake<const char>(0x1000), Fake<const char>(0x1000 + 0x7FFFFFFFull), nullptr, buffer, nullptr); }), __LINE__);
    char16_t* fakeTarget = Fake<char16_t>(0x2000);
    Check(Throws<std::length_error>([&] { _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(nullptr, Fake<const char>(8), &fakeTarget, Fake<char16_t>(0x2000 + 2 * 0x80000000ull), nullptr); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(nullptr, Fake<const char>(8), &fakeTarget, Fake<char16_t>(0x2000 + 2 * 0x7FFFFFFFull), nullptr); }), __LINE__);
    Check(_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(nullptr, nullptr, &fakeTarget, Fake<char16_t>(0x2000 + 2 * 0x7FFFFFFFull), nullptr), __LINE__);
    char16_t* nullTarget = nullptr;
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(abc, abc + 1, &nullTarget, Fake<char16_t>(64), nullptr); }), __LINE__);
}

void TestUTF8ToUTF16ReplacingInvalidSequences() {
    Utf16Result result = ToUTF16("Hello", true);
    Check(result.ok && result.text == u"Hello" && result.ascii, __LINE__);
    result = ToUTF16("h\xC3\xA9\xF0\x9F\x98\x80", true);
    Check(result.ok && result.text == u"hé\U0001F600" && !result.ascii, __LINE__);
    result = ToUTF16("\xE0\x80", true);
    Check(result.ok && result.text == u"��" && !result.ascii, __LINE__);
    result = ToUTF16("\xF0\x90\x80\x41", true);
    Check(result.ok && result.text == u"�A", __LINE__);
    result = ToUTF16("\xC0\x80", true);
    Check(result.ok && result.text == u"��", __LINE__);
    result = ToUTF16("\xED\xA0\x80", true);
    Check(result.ok && result.text == u"���", __LINE__);
    result = ToUTF16("\xED\x9F\xBF", true);
    Check(result.ok && result.text == u"퟿", __LINE__);
    result = ToUTF16("\xF4\x90\x80\x80", true);
    Check(result.ok && result.text == u"����", __LINE__);
    result = ToUTF16("\xF5z", true);
    Check(result.ok && result.text == u"�z", __LINE__);
    result = ToUTF16("a\xE2\x82", true);
    Check(result.ok && result.text == u"a�", __LINE__);
    result = ToUTF16("\xF0\x9F\x98", true);
    Check(result.ok && result.text == u"�", __LINE__);
    result = ToUTF16("\xE2\x82z", true);
    Check(result.ok && result.text == u"�z", __LINE__);
    result = ToUTF16("\xF0\x9F\x98\x80", true, 1);
    Check(!result.ok && !result.advanced, __LINE__);
    char16_t buffer[2] = {};
    char16_t* target = buffer;
    const char* bad = "\xFF\xFF\xFF";
    Check(Throws<std::out_of_range>([&] { _ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb(bad, bad + 3, &target, buffer + 2, nullptr); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb(bad, bad + 1, nullptr, buffer, nullptr); }), __LINE__);
    Check(Throws<std::length_error>([&] { _ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb(Fake<const char>(0x1000), Fake<const char>(0x1000 + 0x80000000ull), nullptr, buffer, nullptr); }), __LINE__);
}

struct Utf8Result {
    ConversionResult result;
    std::string text;
    std::ptrdiff_t consumed;
};

Utf8Result ToUTF8(const std::u16string& input, bool strict, std::size_t capacity = 64) {
    char buffer[80] = {};
    char* target = buffer;
    const char16_t* source = input.data();
    const ConversionResult result = _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, input.data() + input.size(), &target, buffer + capacity, strict);
    return {result, std::string(buffer, static_cast<std::size_t>(target - buffer)), source - input.data()};
}

void TestUTF16ToUTF8() {
    Utf8Result result = ToUTF8(u"Hé€\U0001F600߿ࠀ￿\U0010FFFF", true);
    Check(result.result == ConversionResult::ConversionOK && result.consumed == 10, __LINE__);
    Check(result.text == "H\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\xDF\xBF\xE0\xA0\x80\xEF\xBF\xBF\xF4\x8F\xBF\xBF", __LINE__);
    for (bool strict : {true, false}) {
        result = ToUTF8(std::u16string(u"a") + char16_t(0xD800), strict);
        Check(result.result == ConversionResult::SourceExhausted && result.consumed == 1 && result.text == "a", __LINE__);
        result = ToUTF8(std::u16string(u"a") + char16_t(0xDBFF), strict);
        Check(result.result == ConversionResult::SourceExhausted && result.consumed == 1 && result.text == "a", __LINE__);
    }
    const std::u16string leadThenB = std::u16string(1, char16_t(0xD800)) + u"b";
    result = ToUTF8(leadThenB, true);
    Check(result.result == ConversionResult::SourceIllegal && result.consumed == 0 && result.text.empty(), __LINE__);
    result = ToUTF8(leadThenB, false);
    Check(result.result == ConversionResult::ConversionOK && result.consumed == 2 && result.text == "\xEF\xBF\xBD" "b", __LINE__);
    const std::u16string loneTrail = u"x" + std::u16string(1, char16_t(0xDC00));
    result = ToUTF8(loneTrail, true);
    Check(result.result == ConversionResult::SourceIllegal && result.consumed == 1 && result.text == "x", __LINE__);
    result = ToUTF8(loneTrail, false);
    Check(result.result == ConversionResult::ConversionOK && result.consumed == 2 && result.text == "x\xEF\xBF\xBD", __LINE__);
    const std::u16string twoLeads = {char16_t(0xD800), char16_t(0xD800), char16_t(0xDC00)};
    result = ToUTF8(twoLeads, false);
    Check(result.result == ConversionResult::ConversionOK && result.consumed == 3 && result.text == "\xEF\xBF\xBD\xF0\x90\x80\x80", __LINE__);

    result = ToUTF8(u"aé", true, 2);
    Check(result.result == ConversionResult::TargetExhausted && result.consumed == 1 && result.text == "a", __LINE__);
    result = ToUTF8(u"aé", true, 3);
    Check(result.result == ConversionResult::ConversionOK && result.consumed == 2, __LINE__);
    result = ToUTF8(u"€", true, 2);
    Check(result.result == ConversionResult::TargetExhausted && result.consumed == 0, __LINE__);
    result = ToUTF8(u"\U0001F600", true, 3);
    Check(result.result == ConversionResult::TargetExhausted && result.consumed == 0, __LINE__);
    result = ToUTF8(u"\U0001F600", true, 4);
    Check(result.result == ConversionResult::ConversionOK && result.consumed == 2, __LINE__);
    result = ToUTF8(std::u16string(1, char16_t(0xDC00)), false, 2);
    Check(result.result == ConversionResult::TargetExhausted && result.consumed == 0, __LINE__);
    result = ToUTF8(u"", true, 0);
    Check(result.result == ConversionResult::ConversionOK && result.consumed == 0 && result.text.empty(), __LINE__);

    char buffer[4] = {'#', '#', '#', '#'};
    char* target = buffer;
    const char16_t* text = u"abc";
    const char16_t* source = text;
    Check(Throws<std::out_of_range>([&] { _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, text + 3, &target, buffer + 2, true); }), __LINE__);
    Check(buffer[2] == '#', __LINE__);
    source = text;
    target = buffer;
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(nullptr, text + 3, &target, buffer + 4, true); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, text + 3, nullptr, buffer + 4, true); }), __LINE__);
    const char16_t* nullSource = nullptr;
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&nullSource, Fake<const char16_t>(8), &target, buffer + 4, true); }), __LINE__);
    Check(_ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&nullSource, nullptr, &target, buffer + 4, true) == ConversionResult::ConversionOK && target == buffer, __LINE__);
    char* nullTarget = nullptr;
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, text + 3, &nullTarget, Fake<char>(16), true); }), __LINE__);
    source = text + 3;
    Check(_ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(&source, text, &target, buffer, true) == ConversionResult::ConversionOK && source == text + 3, __LINE__);
}

void TestLatin1ToUTF8() {
    const unsigned char text[] = {'A', 0xE9, 0xFF, 0x80, 0x7F};
    char buffer[16];
    std::memset(buffer, '#', sizeof(buffer));
    char* target = buffer;
    const unsigned char* source = text;
    Check(_ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&source, text + 5, &target, buffer + 16), __LINE__);
    Check(source == text + 5 && target == buffer + 8 && std::memcmp(buffer, "A\xC3\xA9\xC3\xBF\xC2\x80\x7F#", 9) == 0, __LINE__);

    source = text;
    target = buffer;
    Check(!_ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&source, text + 2, &target, buffer + 2), __LINE__);
    Check(source == text && target == buffer, __LINE__);
    Check(_ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&source, text + 2, &target, buffer + 3), __LINE__);
    Check(source == text + 2 && target == buffer + 3, __LINE__);
    source = text + 1;
    target = buffer;
    Check(!_ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&source, text + 2, &target, buffer + 1), __LINE__);
    Check(!_ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&source, text + 2, &target, buffer - 1), __LINE__);
    Check(source == text + 1 && target == buffer, __LINE__);

    std::memset(buffer, '#', sizeof(buffer));
    source = text;
    Check(Throws<std::out_of_range>([&] { _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&source, text + 1, &target, buffer); }), __LINE__);
    Check(buffer[0] == '#' && source == text && target == buffer, __LINE__);
    source = text + 3;
    Check(Throws<std::out_of_range>([&] { _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&source, text + 5, &target, buffer + 2); }), __LINE__);
    Check(buffer[2] == '#', __LINE__);

    source = text;
    Check(_ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&source, text, &target, buffer) && source == text && target == buffer, __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&source, text + 1, nullptr, buffer + 4); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(nullptr, text + 1, &target, buffer + 4); }), __LINE__);
    const unsigned char* nullSource = nullptr;
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&nullSource, Fake<const unsigned char>(4), &target, buffer + 4); }), __LINE__);
    char* nullTarget = nullptr;
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(&source, text + 1, &nullTarget, Fake<char>(16)); }), __LINE__);
}

} // namespace

int main() {
    TestUTF8ToUTF16();
    TestUTF8ToUTF16ReplacingInvalidSequences();
    TestUTF16ToUTF8();
    TestLatin1ToUTF8();
    return 0;
}
