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
}

}

int main() {
    TestWordTextBreak();
    TestRefusals();
    return 0;
}
