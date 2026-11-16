#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/CollatorExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::WtfAbi::Collator;
using NKWebKit::WtfAbi::StringView;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WTF::Collator check failed at line %u\n", location.line());
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

StringView Utf16(const char16_t* text, unsigned length) {
    return StringView{text, length, false};
}

int Sign(int value) {
    return (value > 0) - (value < 0);
}

void TestCollate() {
    Collator upperFirst{};
    _ZN3WTF8CollatorC1EPKcb(&upperFirst, "en_US", false);
    Require(upperFirst.collator != nullptr);
    Require(upperFirst.locale != nullptr && std::strcmp(upperFirst.locale, "en_US") == 0);
    Require(!upperFirst.shouldSortLowercaseFirst);

    Require(_ZNK3WTF8Collator7collateENS_10StringViewES1_(&upperFirst, Latin1("apple"), Latin1("apple")) == 0);
    Require(Sign(_ZNK3WTF8Collator7collateENS_10StringViewES1_(&upperFirst, Latin1("apple"), Latin1("banana"))) == -1);
    Require(Sign(_ZNK3WTF8Collator7collateENS_10StringViewES1_(&upperFirst, Latin1("banana"), Latin1("apple"))) == 1);
    Require(Sign(_ZNK3WTF8Collator7collateENS_10StringViewES1_(&upperFirst, Latin1("a"), Latin1("A"))) == 1);
    Require(Sign(_ZNK3WTF8Collator7collateENS_10StringViewES1_(&upperFirst, Latin1(""), Latin1("a"))) == -1);
    static const char16_t apple16[] = u"apple";
    Require(_ZNK3WTF8Collator7collateENS_10StringViewES1_(&upperFirst, Utf16(apple16, 5), Latin1("apple")) == 0);
    static const char16_t eAcute16[] = u"é";
    static const char eAcuteLatin1[] = "\xe9";
    Require(_ZNK3WTF8Collator7collateENS_10StringViewES1_(&upperFirst, Utf16(eAcute16, 1), Latin1(eAcuteLatin1)) == 0);
    static const char16_t eCombining16[] = u"é";
    Require(_ZNK3WTF8Collator7collateENS_10StringViewES1_(&upperFirst, Utf16(eCombining16, 2), Latin1(eAcuteLatin1)) == 0);
    Require(Sign(_ZNK3WTF8Collator7collateENS_10StringViewES1_(&upperFirst, Utf16(eAcute16, 1), Latin1("f"))) == -1);

    Require(_ZNK3WTF8Collator11collateUTF8EPKcS2_(&upperFirst, "caf\xc3\xa9", "caf\xc3\xa9") == 0);
    Require(Sign(_ZNK3WTF8Collator11collateUTF8EPKcS2_(&upperFirst, "caf\xc3\xa9", "cafe")) == 1);
    Require(Sign(_ZNK3WTF8Collator11collateUTF8EPKcS2_(&upperFirst, "a", "A")) == 1);

    Collator lowerFirst{};
    _ZN3WTF8CollatorC2EPKcb(&lowerFirst, "en_US", true);
    Require(lowerFirst.collator != nullptr && lowerFirst.collator != upperFirst.collator);
    Require(lowerFirst.shouldSortLowercaseFirst);
    Require(Sign(_ZNK3WTF8Collator7collateENS_10StringViewES1_(&lowerFirst, Latin1("a"), Latin1("A"))) == -1);
    Require(Sign(_ZNK3WTF8Collator11collateUTF8EPKcS2_(&lowerFirst, "a", "A")) == -1);

    auto* upperFirstCollator = upperFirst.collator;
    auto* upperFirstLocale = upperFirst.locale;
    _ZN3WTF8CollatorD1Ev(&upperFirst);
    Collator reused{};
    _ZN3WTF8CollatorC1EPKcb(&reused, "en_US", false);
    Require(reused.collator == upperFirstCollator && reused.locale == upperFirstLocale);

    _ZN3WTF8CollatorD2Ev(&lowerFirst);
    Collator other{};
    _ZN3WTF8CollatorC1EPKcb(&other, "en_US", false);
    Require(other.collator != upperFirstCollator && other.collator != nullptr);
    _ZN3WTF8CollatorD1Ev(&other);
    _ZN3WTF8CollatorD1Ev(&reused);

    Collator root{};
    _ZN3WTF8CollatorC1EPKcb(&root, nullptr, false);
    Require(root.collator != nullptr && root.locale == nullptr);
    Require(Sign(_ZNK3WTF8Collator7collateENS_10StringViewES1_(&root, Latin1("a"), Latin1("b"))) == -1);
    _ZN3WTF8CollatorD1Ev(&root);
}

void TestCreateIterator() {
    const auto latin1 = _ZN3WTF14createIteratorENS_10StringViewE(Latin1("ab\xe9"));
    Require(latin1.length == 3 && latin1.start == 0 && latin1.index == 0 && latin1.limit == 3);
    Require(latin1.reservedField == 0 && latin1.reservedFn == nullptr);
    auto iterator = latin1;
    Require(iterator.hasNext(&iterator) && !iterator.hasPrevious(&iterator));
    Require(iterator.current(&iterator) == 'a');
    Require(iterator.next(&iterator) == 'a' && iterator.next(&iterator) == 'b' && iterator.next(&iterator) == 0xe9);
    Require(iterator.next(&iterator) == -1 && !iterator.hasNext(&iterator));
    Require(iterator.previous(&iterator) == 0xe9);
    Require(iterator.getIndex(&iterator, UITER_CURRENT) == 2 && iterator.getIndex(&iterator, UITER_LENGTH) == 3);
    Require(iterator.move(&iterator, -2, UITER_CURRENT) == 0 && iterator.current(&iterator) == 'a');
    Require(iterator.previous(&iterator) == -1);
    const auto state = iterator.getState(&iterator);
    iterator.move(&iterator, 0, UITER_LIMIT);
    iterator.setState(&iterator, state, nullptr);
    Require(iterator.index == 0);

    static const char16_t text16[] = u"xy";
    auto utf16 = _ZN3WTF14createIteratorENS_10StringViewE(Utf16(text16, 2));
    Require(utf16.length == 2 && utf16.limit == 2 && utf16.context == text16);
    Require(utf16.next(&utf16) == 'x' && utf16.next(&utf16) == 'y' && utf16.next(&utf16) == -1);
}

void TestRefusals() {
    Collator collator{};
    _ZN3WTF8CollatorC1EPKcb(&collator, "en", false);
    RequireThrows<std::invalid_argument>([] { _ZN3WTF8CollatorC1EPKcb(nullptr, "en", false); });
    RequireThrows<std::invalid_argument>([] { _ZN3WTF8CollatorD1Ev(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZNK3WTF8Collator7collateENS_10StringViewES1_(nullptr, Latin1("a"), Latin1("b")); });
    RequireThrows<std::invalid_argument>([&] { _ZNK3WTF8Collator7collateENS_10StringViewES1_(&collator, StringView{nullptr, 1, true}, Latin1("b")); });
    RequireThrows<std::invalid_argument>([&] { _ZNK3WTF8Collator11collateUTF8EPKcS2_(&collator, nullptr, "b"); });
    RequireThrows<std::invalid_argument>([&] { _ZNK3WTF8Collator11collateUTF8EPKcS2_(&collator, "a", nullptr); });
    RequireThrows<std::length_error>([] { _ZN3WTF14createIteratorENS_10StringViewE(StringView{"a", 0x80000000u, true}); });
    Collator empty{};
    RequireThrows<std::invalid_argument>([&] { _ZNK3WTF8Collator11collateUTF8EPKcS2_(&empty, "a", "b"); });
    _ZN3WTF8CollatorD1Ev(&collator);
}

}

int main() {
    TestCollate();
    TestCreateIterator();
    TestRefusals();
    return 0;
}
