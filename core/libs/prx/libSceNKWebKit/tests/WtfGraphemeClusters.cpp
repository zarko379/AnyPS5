#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceIcu/BreakIterator.hpp"
#include "prx/libSceNKWebKit/include/TextBreakIteratorExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::WtfAbi::StringView;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WTF grapheme cluster check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Exception, class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const Exception&) {
        threw = true;
    }
    Require(threw);
}

StringView Latin1(const char* text) {
    return StringView{text, static_cast<unsigned>(std::strlen(text)), true};
}

StringView Utf16(const char16_t* text) {
    unsigned length = 0;
    while (text[length]) ++length;
    return StringView{text, length, false};
}

unsigned Count(StringView string) {
    return _ZN3WTF19numGraphemeClustersENS_10StringViewE(string);
}

unsigned Units(StringView string, unsigned clusters) {
    return _ZN3WTF30numCodeUnitsInGraphemeClustersENS_10StringViewEj(string, clusters);
}

void TestLatin1() {
    Require(Count(Latin1("")) == 0);
    Require(Count(Latin1("abc")) == 3);
    Require(Count(Latin1("ab\r\ncd")) == 5);
    Require(Count(Latin1("\r\n\r\n")) == 2);
    Require(Count(Latin1("\n\r")) == 2);
    Require(Units(Latin1("ab\r\ncd"), 2) == 2);
    Require(Units(Latin1("ab\r\ncd"), 3) == 4);
    Require(Units(Latin1("ab\r\ncd"), 4) == 5);
    Require(Units(Latin1("ab\r\ncd"), 5) == 6);
    Require(Units(Latin1("ab\r\ncd"), 6) == 6);
    Require(Units(Latin1("ab\r\n"), 3) == 4);
    Require(Units(Latin1("abc"), 0) == 0);
    Require(Units(Latin1(""), 0) == 0);
}

void TestUtf16() {
    Require(Count(Utf16(u"")) == 0);
    Require(Count(Utf16(u"abc")) == 3);
    Require(Count(Utf16(u"éx")) == 2);
    Require(Count(Utf16(u"\U0001F600a")) == 2);
    Require(Count(Utf16(u"a\r\nb")) == 3);
    Require(Count(Utf16(u"\U0001F1EB\U0001F1F7")) == 1);
    Require(Units(Utf16(u"éx"), 1) == 2);
    Require(Units(Utf16(u"éx"), 2) == 3);
    Require(Units(Utf16(u"\U0001F600a"), 1) == 2);
    Require(Units(Utf16(u"\U0001F600a"), 0) == 0);
    Require(Units(Utf16(u"\U0001F600a"), 3) == 3);
    Require(Units(Utf16(u"\U0001F1EB\U0001F1F7!"), 1) == 4);
    Require(Units(Utf16(u"\U0001F1EB\U0001F1F7"), 2) == 4);
    Require(Count(Utf16(u"éx")) == 2);
}

void TestWordTextBreak() {
    static const char16_t text[] = u"hello world";
    std::int32_t status = 0;
    auto* iterator = ubrk_open_nid_postfix(1, "en", text, 11, &status);
    Require(status <= 0 && iterator != nullptr);
    Require(ubrk_next_nid_postfix(iterator) == 5);
    Require(_ZN3WTF15isWordTextBreakEP14UBreakIterator(iterator));
    Require(ubrk_next_nid_postfix(iterator) == 6);
    Require(!_ZN3WTF15isWordTextBreakEP14UBreakIterator(iterator));
    Require(ubrk_next_nid_postfix(iterator) == 11);
    Require(_ZN3WTF15isWordTextBreakEP14UBreakIterator(iterator));
    ubrk_close_nid_postfix(iterator);
}

void TestRefusals() {
    RequireThrows<std::invalid_argument>([] { _ZN3WTF15isWordTextBreakEP14UBreakIterator(nullptr); });
    RequireThrows<std::invalid_argument>([] { Count(StringView{nullptr, 2, true}); });
    RequireThrows<std::invalid_argument>([] { Count(StringView{nullptr, 2, false}); });
    RequireThrows<std::invalid_argument>([] { Units(StringView{nullptr, 3, true}, 1); });
    RequireThrows<std::invalid_argument>([] { Units(StringView{nullptr, 3, false}, 1); });
    RequireThrows<std::length_error>([] { Count(StringView{u"a", 0x80000000u, false}); });
    Require(Units(StringView{nullptr, 3, false}, 3) == 3);
}

}

int main() {
    TestLatin1();
    TestUtf16();
    TestWordTextBreak();
    TestRefusals();
    return 0;
}
