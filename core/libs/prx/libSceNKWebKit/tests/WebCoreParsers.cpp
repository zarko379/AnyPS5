#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using NKWebKit::WtfAbi::ByteVector;
using NKWebKit::WtfAbi::StringView;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore parsers check failed at line %u\n", static_cast<unsigned>(location.line()));
        std::abort();
    }
}

template<typename T> void Throws(T action, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try {
        action();
    } catch (const std::exception&) {
        threw = true;
    }
    Check(threw, location);
}

template<typename Function> void ForBothWidths(const char* text, Function function) {
    function(StringView{text, static_cast<unsigned>(std::strlen(text)), true});
    std::u16string wide;
    for (const char* cursor = text; *cursor; ++cursor) wide.push_back(static_cast<unsigned char>(*cursor));
    function(StringView{wide.data(), static_cast<unsigned>(wide.size()), false});
}

void TestXFrameOptions() {
    using Disposition = WebCore::XFrameOptionsDisposition;
    using NKWebKit::WtfAbi::String;
    using NKWebKit::WtfAbi::StringImpl;

    static_assert(static_cast<unsigned>(Disposition::None) == 0);
    static_assert(static_cast<unsigned>(Disposition::Deny) == 1);
    static_assert(static_cast<unsigned>(Disposition::SameOrigin) == 2);
    static_assert(static_cast<unsigned>(Disposition::AllowAll) == 3);
    static_assert(static_cast<unsigned>(Disposition::Invalid) == 4);
    static_assert(static_cast<unsigned>(Disposition::Conflict) == 5);

    constexpr struct {
        const char* text;
        Disposition expected;
    } cases[] = {
        {"", Disposition::None},
        {",,,", Disposition::None},
        {"dEnY", Disposition::Deny},
        {"\vSaMeOrIgIn\r\t", Disposition::SameOrigin},
        {" ALLOWALL ", Disposition::AllowAll},
        {",DENY,,deny,", Disposition::Deny},
        {"deny,sameorigin", Disposition::Conflict},
        {"deny,invalid", Disposition::Conflict},
        {"invalid,deny", Disposition::Conflict},
        {"x,y", Disposition::Invalid},
        {" ", Disposition::Invalid},
        {"deny, ,deny", Disposition::Conflict},
        {"DENY\xa0", Disposition::Invalid},
        {"deny xyz", Disposition::Invalid},
    };
    for (const auto& test : cases) {
        ForBothWidths(test.text, [&](StringView view) {
            StringImpl storage{1, view.length, view.data, view.is8Bit ? 4u : 0u};
            Check(_ZN7WebCore24parseXFrameOptionsHeaderERKN3WTF6StringE(String(&storage)) == test.expected);
        });
    }
    Check(_ZN7WebCore24parseXFrameOptionsHeaderERKN3WTF6StringE(String()) == Disposition::None);

    auto checkWide = [](std::u16string_view text, Disposition expected) {
        StringImpl storage{1, static_cast<unsigned>(text.size()), text.data(), 0};
        Check(_ZN7WebCore24parseXFrameOptionsHeaderERKN3WTF6StringE(String(&storage)) == expected);
    };
    checkWide(u"\u1680DENY\u2028,\u2007deny\u3000", Disposition::Deny);
    checkWide(u"SAMEORIGIN\u2029", Disposition::Invalid);
    checkWide(u"D\u0145NY", Disposition::Invalid);
    checkWide(std::u16string_view(u"DENY\0", 5), Disposition::Invalid);

    StringImpl invalid{1, 1, nullptr, 4};
    Throws([&] { _ZN7WebCore24parseXFrameOptionsHeaderERKN3WTF6StringE(String(&invalid)); });
}

struct IntegerCase {
    long long expected;
    const char* input;
};

const IntegerCase kIntegerCases[] = {
    {0LL, "0"},
    {0LL, "-0"},
    {0LL, "+0"},
    {123LL, "123"},
    {123LL, "+123"},
    {-123LL, "-123"},
    {123LL, "  123"},
    {123LL, "123   "},
    {123LL, "   123   "},
    {123LL, "123abc"},
    {-123LL, "-123abc"},
    {123LL, "  +123"},
    {-123LL, "  -123"},
    {12LL, "   12 3"},
    {1LL, "1.0"},
    {1LL, "1."},
    {1LL, "1e1"},
    {123LL, " \t\r\n\f123"},
    {-2147483648LL, "-2147483648"},
    {2147483647LL, "2147483647"}
};

const char* const kIntegerFailures[] = {
    "-2147483649",
    "2147483648",
    "111111111111111111",
    "",
    " ",
    "   ",
    "+",
    "+ 123",
    "-",
    "- 123",
    "a",
    "-a",
    "+-123",
    "-+123",
    "++123",
    "--123",
    "\v123",
    "a123",
    "+a123",
    "-a123",
    ".1",
    "infinity"
};

void TestParseHTMLInteger() {
    for (const auto& testCase : kIntegerCases) {
        ForBothWidths(testCase.input, [&](StringView view) {
            auto result = _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(view);
            Check(result.hasValue);
            Check(result.value == testCase.expected);
        });
    }
    for (const char* input : kIntegerFailures) {
        ForBothWidths(input, [&](StringView view) {
            Check(!_ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(view).hasValue);
        });
    }

    ForBothWidths("-2147483649", [](StringView view) {
        auto result = _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(view);
        Check(!result.hasValue && result.error == WebCore::HTMLIntegerParsingError::NegativeOverflow);
    });
    ForBothWidths("2147483648", [](StringView view) {
        auto result = _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(view);
        Check(!result.hasValue && result.error == WebCore::HTMLIntegerParsingError::PositiveOverflow);
    });
    ForBothWidths("111111111111111111", [](StringView view) {
        auto result = _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(view);
        Check(!result.hasValue && result.error == WebCore::HTMLIntegerParsingError::PositiveOverflow);
    });
    for (const char* input : {"", " ", "+", "-", "a", "\v123", ".1"}) {
        ForBothWidths(input, [](StringView view) {
            auto result = _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(view);
            Check(!result.hasValue && result.error == WebCore::HTMLIntegerParsingError::Other);
        });
    }

    std::u16string wide = u"12\u0663";
    auto result = _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(StringView{wide.data(), static_cast<unsigned>(wide.size()), false});
    Check(result.hasValue && result.value == 12);
    wide = u"\u00a0" u"12";
    Check(!_ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(StringView{wide.data(), static_cast<unsigned>(wide.size()), false}).hasValue);

    Check(!_ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(StringView{nullptr, 0, true}).hasValue);
    Throws([] { _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(StringView{nullptr, 3, true}); });
}

void ExpectPolicy(const char* text, WebCore::CrossOriginResourcePolicy expected, std::source_location location = std::source_location::current()) {
    ForBothWidths(text, [&](StringView view) {
        Check(_ZN7WebCore36parseCrossOriginResourcePolicyHeaderEN3WTF10StringViewE(view) == expected, location);
    });
}

void TestCrossOriginResourcePolicy() {
    using WebCore::CrossOriginResourcePolicy;
    ExpectPolicy("", CrossOriginResourcePolicy::None);
    ExpectPolicy(" \t\r\n ", CrossOriginResourcePolicy::None);
    ExpectPolicy("same-origin", CrossOriginResourcePolicy::SameOrigin);
    ExpectPolicy("  same-origin\t", CrossOriginResourcePolicy::SameOrigin);
    ExpectPolicy("same-site", CrossOriginResourcePolicy::SameSite);
    ExpectPolicy("\r\nsame-site\r\n", CrossOriginResourcePolicy::SameSite);
    ExpectPolicy("cross-origin", CrossOriginResourcePolicy::CrossOrigin);
    ExpectPolicy("Cross-Origin", CrossOriginResourcePolicy::Invalid);
    ExpectPolicy("same-origin, same-site", CrossOriginResourcePolicy::Invalid);
    ExpectPolicy("same-origi", CrossOriginResourcePolicy::Invalid);
    ExpectPolicy("same-origins", CrossOriginResourcePolicy::Invalid);
    ExpectPolicy("\fsame-origin", CrossOriginResourcePolicy::Invalid);
    ExpectPolicy("\vsame-site", CrossOriginResourcePolicy::Invalid);
    Check(_ZN7WebCore36parseCrossOriginResourcePolicyHeaderEN3WTF10StringViewE(StringView{nullptr, 0, true}) == CrossOriginResourcePolicy::None);
    Throws([] { _ZN7WebCore36parseCrossOriginResourcePolicyHeaderEN3WTF10StringViewE(StringView{nullptr, 4, true}); });
}

ByteVector Make(std::initializer_list<unsigned char> bytes) {
    ByteVector vector;
    if (bytes.size()) {
        vector.buffer = static_cast<unsigned char*>(_ZN3WTF10fastMallocEm(bytes.size()));
        std::memcpy(vector.buffer, bytes.begin(), bytes.size());
        vector.capacity = static_cast<unsigned>(bytes.size());
        vector.size = static_cast<unsigned>(bytes.size());
    }
    return vector;
}

std::string Stringify(const ByteVector& vector) {
    std::string out;
    for (unsigned i = 0; i < vector.size; ++i) {
        switch (vector.buffer[i]) {
        case '\0': out += "<0>"; break;
        case '\r': out += "<CR>"; break;
        case '\n': out += "<LF>"; break;
        default: out.push_back(static_cast<char>(vector.buffer[i]));
        }
    }
    return out;
}

constexpr unsigned char null = 0;
constexpr unsigned char CR = '\r';
constexpr unsigned char LF = '\n';
constexpr unsigned char letterA = 'a';
constexpr unsigned char letterB = 'b';

std::string ToLF(std::initializer_list<unsigned char> bytes) {
    ByteVector input = Make(bytes);
    ByteVector output = _ZN3WTF24normalizeLineEndingsToLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(std::move(input));
    Check(input.buffer == nullptr && input.size == 0 && input.capacity == 0);
    return Stringify(output);
}

std::string ToCRLF(std::initializer_list<unsigned char> bytes) {
    ByteVector input = Make(bytes);
    ByteVector output = _ZN3WTF26normalizeLineEndingsToCRLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(std::move(input));
    return Stringify(output);
}

std::string ToNative(std::initializer_list<unsigned char> bytes) {
    ByteVector input = Make(bytes);
    ByteVector output = _ZN3WTF28normalizeLineEndingsToNativeEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(std::move(input));
    return Stringify(output);
}

void TestLineEndings() {
    Check(ToLF({}) == "");
    Check(ToLF({null}) == "<0>");
    Check(ToLF({letterA}) == "a");
    Check(ToLF({CR}) == "<LF>");
    Check(ToLF({LF}) == "<LF>");
    Check(ToLF({CR, LF}) == "<LF>");
    Check(ToLF({LF, LF}) == "<LF><LF>");
    Check(ToLF({CR, CR}) == "<LF><LF>");
    Check(ToLF({LF, CR}) == "<LF><LF>");
    Check(ToLF({letterA, CR}) == "a<LF>");
    Check(ToLF({letterA, LF}) == "a<LF>");
    Check(ToLF({letterA, CR, LF}) == "a<LF>");
    Check(ToLF({letterA, LF, CR}) == "a<LF><LF>");
    Check(ToLF({letterA, CR, letterB}) == "a<LF>b");
    Check(ToLF({letterA, LF, letterB}) == "a<LF>b");
    Check(ToLF({letterA, CR, LF, letterB}) == "a<LF>b");
    Check(ToLF({letterA, LF, CR, letterB}) == "a<LF><LF>b");

    Check(ToCRLF({}) == "");
    Check(ToCRLF({null}) == "<0>");
    Check(ToCRLF({letterA}) == "a");
    Check(ToCRLF({CR}) == "<CR><LF>");
    Check(ToCRLF({LF}) == "<CR><LF>");
    Check(ToCRLF({CR, LF}) == "<CR><LF>");
    Check(ToCRLF({LF, LF}) == "<CR><LF><CR><LF>");
    Check(ToCRLF({CR, CR}) == "<CR><LF><CR><LF>");
    Check(ToCRLF({LF, CR}) == "<CR><LF><CR><LF>");
    Check(ToCRLF({letterA, CR}) == "a<CR><LF>");
    Check(ToCRLF({letterA, LF}) == "a<CR><LF>");
    Check(ToCRLF({letterA, CR, LF}) == "a<CR><LF>");
    Check(ToCRLF({letterA, LF, CR}) == "a<CR><LF><CR><LF>");
    Check(ToCRLF({letterA, CR, letterB}) == "a<CR><LF>b");
    Check(ToCRLF({letterA, LF, letterB}) == "a<CR><LF>b");
    Check(ToCRLF({letterA, CR, LF, letterB}) == "a<CR><LF>b");
    Check(ToCRLF({letterA, LF, CR, letterB}) == "a<CR><LF><CR><LF>b");

    Check(ToNative({letterA, CR, LF, letterB, CR, CR}) == "a<LF>b<LF><LF>");
    Check(ToNative({}) == "");
}

void TestLineEndingStorage() {
    ByteVector input = Make({letterA, CR, LF, letterB});
    unsigned char* buffer = input.buffer;
    ByteVector lf = _ZN3WTF24normalizeLineEndingsToLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(std::move(input));
    Check(lf.buffer == buffer && lf.size == 3 && lf.capacity == 4);

    ByteVector clean = Make({letterA, letterB});
    unsigned char* cleanBuffer = clean.buffer;
    ByteVector same = _ZN3WTF26normalizeLineEndingsToCRLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(std::move(clean));
    Check(same.buffer == cleanBuffer && same.size == 2 && clean.buffer == nullptr);

    ByteVector grown = Make({LF, letterA});
    unsigned char* original = grown.buffer;
    ByteVector crlf = _ZN3WTF26normalizeLineEndingsToCRLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(std::move(grown));
    Check(crlf.buffer != original && crlf.size == 3 && crlf.capacity == 3);
    Check(grown.buffer == original && grown.size == 2);
}

struct RangeCase {
    const char* text;
    bool result;
    long long offset;
    long long end;
    long long suffix;
};

void TestParseRange() {
    using NKWebKit::WtfAbi::String;
    using NKWebKit::WtfAbi::StringImpl;
    constexpr long long int64Max = std::numeric_limits<long long>::max();
    constexpr long long int64Min = std::numeric_limits<long long>::min();
    const RangeCase cases[] = {
        {"bytes=0-499", true, 0, 499, -1},
        {"BYTES=500-", true, 500, -1, -1},
        {"bYtEs=-500", true, -1, -1, 500},
        {"bytes=-abc", true, -1, -1, -1},
        {"bytes=--5", true, -1, -1, -5},
        {"bytes=-5-", true, -1, -1, -1},
        {"bytes=- 7 ", true, -1, -1, 7},
        {"bytes=-0", true, -1, -1, 0},
        {"bytes= 10 - 20 ", true, 10, 20, -1},
        {"bytes=\t10\r-\n20\f", true, 10, 20, -1},
        {"bytes=+10-+20", true, 10, 20, -1},
        {"bytes=5-5", true, 5, 5, -1},
        {"bytes=20-10", false, -1, -1, -1},
        {"bytes=a-5", false, -1, -1, -1},
        {"bytes=5-x", false, -1, -1, -1},
        {"bytes=0-1,2-3", false, -1, -1, -1},
        {"bytes= -5-", false, -1, -1, -1},
        {"bytes=1 2-3", false, -1, -1, -1},
        {"bytes 0-1", false, -1, -1, -1},
        {"bytes\x1d" "0-1", true, 0, 1, -1},
        {"bytes=", false, -1, -1, -1},
        {"bytes=5", false, -1, -1, -1},
        {"byte", false, -1, -1, -1},
        {"", false, -1, -1, -1},
        {"bytes=9223372036854775807-", true, int64Max, -1, -1},
        {"bytes=9223372036854775808-", false, -1, -1, -1},
        {"bytes=0-9223372036854775808", false, -1, -1, -1},
        {"bytes=--9223372036854775808", true, -1, -1, int64Min},
        {"bytes=-9223372036854775808", true, -1, -1, -1},
        {"bytes=1\xa0-2", false, -1, -1, -1},
        {"bytes=\v1-2", true, 1, 2, -1},
    };
    for (const auto& test : cases) {
        ForBothWidths(test.text, [&](StringView view) {
            StringImpl storage{1, view.length, view.data, view.is8Bit ? 4u : 0u};
            long long offset = 7;
            long long end = 8;
            long long suffix = 9;
            Check(_ZN7WebCore10parseRangeERKN3WTF6StringERxS4_S4_(String(&storage), offset, end, suffix) == test.result);
            Check(offset == test.offset && end == test.end && suffix == test.suffix);
        });
    }

    long long offset = 7;
    long long end = 8;
    long long suffix = 9;
    Check(!_ZN7WebCore10parseRangeERKN3WTF6StringERxS4_S4_(String(), offset, end, suffix));
    Check(offset == -1 && end == -1 && suffix == -1);

    auto checkWide = [](std::u16string_view text, bool result, long long expectedOffset, long long expectedEnd, long long expectedSuffix) {
        StringImpl storage{1, static_cast<unsigned>(text.size()), text.data(), 0};
        long long wideOffset = 7;
        long long wideEnd = 8;
        long long wideSuffix = 9;
        Check(_ZN7WebCore10parseRangeERKN3WTF6StringERxS4_S4_(String(&storage), wideOffset, wideEnd, wideSuffix) == result);
        Check(wideOffset == expectedOffset && wideEnd == expectedEnd && wideSuffix == expectedSuffix);
    };
    checkWide(u"bytes=　12- 13 ", true, 12, 13, -1);
    checkWide(u"bytes=- " u"40", true, -1, -1, 40);
    checkWide(u"bytes=1\u0085-2", false, -1, -1, -1);
    checkWide(u"bytes=1 -2", false, -1, -1, -1);
    checkWide(u"bytesĽ" u"1-2", false, -1, -1, -1);
    checkWide(u"bytes=１-2", false, -1, -1, -1);

    StringImpl invalid{1, 3, nullptr, 4};
    Throws([&] {
        long long a = 0;
        long long b = 0;
        long long c = 0;
        _ZN7WebCore10parseRangeERKN3WTF6StringERxS4_S4_(String(&invalid), a, b, c);
    });
}

void TestAutofillFieldName() {
    using NKWebKit::WtfAbi::String;
    using NKWebKit::WtfAbi::StringImpl;
    const std::pair<const char*, int> cases[] = {
        {"off", 0}, {"on", 0}, {"OFF", 0}, {"On", 0}, {"", 0}, {"nme", 0}, {"name ", 0}, {" name", 0},
        {"name", 1}, {"honorific-prefix", 2}, {"given-name", 3}, {"new-password", 9}, {"current-password", 10},
        {"address-line1", 14}, {"address-level4", 17}, {"address-level1", 20}, {"postal-code", 23},
        {"cc-name", 24}, {"cc-type", 33}, {"transaction-amount", 35}, {"bday-year", 40}, {"url", 42},
        {"photo", 43}, {"tel", 44}, {"tel-local", 48}, {"tel-extension", 51}, {"email", 52}, {"impp", 53},
        {"section-x", 0}, {"shipping", 0}, {"web-authn", 0},
    };
    for (const auto& [text, expected] : cases) {
        ForBothWidths(text, [&](StringView view) {
            StringImpl storage{1, view.length, view.data, view.is8Bit ? 4u : 0u};
            Check(_ZN7WebCore19toAutofillFieldNameERKN3WTF10AtomStringE(String(&storage)) == expected);
        });
    }
    for (const char* divergent : {"Name", "EMAIL", "Tel-Local", "webauthn", "WebAuthn", "address\rline1", "cc\rnumber"}) {
        ForBothWidths(divergent, [&](StringView view) {
            StringImpl storage{1, view.length, view.data, view.is8Bit ? 4u : 0u};
            Throws([&] { _ZN7WebCore19toAutofillFieldNameERKN3WTF10AtomStringE(String(&storage)); });
        });
    }
    std::u16string_view wideName(u"nšme");
    StringImpl wide{1, static_cast<unsigned>(wideName.size()), wideName.data(), 0};
    Check(_ZN7WebCore19toAutofillFieldNameERKN3WTF10AtomStringE(String(&wide)) == 0);
    Throws([] { _ZN7WebCore19toAutofillFieldNameERKN3WTF10AtomStringE(String()); });
    StringImpl invalid{1, 2, nullptr, 4};
    Throws([&] { _ZN7WebCore19toAutofillFieldNameERKN3WTF10AtomStringE(String(&invalid)); });
}

void TestStripHTMLSpaces() {
    using NKWebKit::WtfAbi::String;
    using NKWebKit::WtfAbi::StringImpl;
    auto strip = [](const String& value) { return _ZN7WebCore33stripLeadingAndTrailingHTMLSpacesERKN3WTF6StringE(value); };
    auto text8 = [](const String& value) {
        return std::string(static_cast<const char*>(value.impl->data), value.impl->length);
    };

    String null = strip(String());
    Check(null.impl == nullptr);

    StringImpl emptyStorage{1, 0, "", 4};
    String empty = strip(String(&emptyStorage));
    Check(empty.impl && empty.impl->length == 0 && empty.impl != &emptyStorage);

    StringImpl plainStorage{1, 3, "abc", 4};
    String plain = strip(String(&plainStorage));
    Check(plain.impl == &plainStorage && plainStorage.refCount == 1);

    const char padded[] = "  \t\nabc\r\f ";
    StringImpl paddedStorage{1, static_cast<unsigned>(sizeof(padded) - 1), padded, 4};
    String stripped = strip(String(&paddedStorage));
    Check(stripped.impl != &paddedStorage && (stripped.impl->hashAndFlags & 4) && text8(stripped) == "abc");

    const char vertical[] = "\v abc";
    StringImpl verticalStorage{1, static_cast<unsigned>(sizeof(vertical) - 1), vertical, 4};
    Check(strip(String(&verticalStorage)).impl == &verticalStorage);

    StringImpl spacesStorage{1, 3, "   ", 4};
    String spaces = strip(String(&spacesStorage));
    Check(spaces.impl && spaces.impl->length == 0 && spaces.impl != &spacesStorage);

    std::u16string_view unicode(u"\u00a0 x \u3000");
    StringImpl unicodeStorage{1, static_cast<unsigned>(unicode.size()), unicode.data(), 0};
    Check(strip(String(&unicodeStorage)).impl == &unicodeStorage);

    std::u16string_view wide(u" \u0101b\n");
    StringImpl wideStorage{1, static_cast<unsigned>(wide.size()), wide.data(), 0};
    String wideStripped = strip(String(&wideStorage));
    Check(wideStripped.impl->length == 2 && !(wideStripped.impl->hashAndFlags & 4));
    const char16_t* wideData = static_cast<const char16_t*>(wideStripped.impl->data);
    Check(wideData[0] == u'\u0101' && wideData[1] == u'b');

    StringImpl invalid{1, 2, nullptr, 4};
    Throws([&] { strip(String(&invalid)); });
}

}  // namespace

int main() {
    TestParseHTMLInteger();
    TestCrossOriginResourcePolicy();
    TestXFrameOptions();
    TestLineEndings();
    TestLineEndingStorage();
    TestParseRange();
    TestAutofillFieldName();
    TestStripHTMLSpaces();
    return 0;
}
