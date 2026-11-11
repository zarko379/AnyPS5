#include "prx/libSceIcu/Collation.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include "prx/libSceIcu/Set.hpp"
#include <unicode/ucol.h>
#include <unicode/udata.h>
#include <unicode/uiter.h>
#include <unicode/uloc.h>
#include <unicode/uniset.h>
#include <unicode/uset.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

void* GuestStringIterator(const char16_t* text, std::int32_t length);
void* GuestUtf8Iterator(const char* text, std::int32_t length);
std::int32_t GuestIteratorIndex(const void* iterator);
void FreeGuestIterator(void* iterator);

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t UsingDefault = -127;
constexpr std::int32_t Less = -1;
constexpr std::int32_t Equal = 0;
constexpr std::int32_t Greater = 1;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "ICU COLLATION: %s\n", message);
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

UCollator* Open(const char* locale) {
    std::int32_t status = Ok;
    UCollator* collator = ucol_open_nid_postfix(locale, &status);
    Require(collator && status <= Ok, "a collator opens");
    return collator;
}

UCollator* HostOpen(const char* locale) {
    UErrorCode status = U_ZERO_ERROR;
    UCollator* collator = ucol_open(locale, &status);
    Require(collator && U_SUCCESS(status), "a host collator opens");
    return collator;
}

std::int32_t Compare(const UCollator* collator, const char16_t* left, const char16_t* right) {
    return ucol_strcoll_nid_postfix(collator, left, -1, right, -1);
}

const std::vector<std::u16string>& Strings() {
    static const std::vector<std::u16string> strings{
        u"", u"a", u"A", u"b", u"ab", u"aB", u"Ab", u"abc", u"ABC", u"a b", u"a-b", u"ab-", u"-ab", u"z", u"Z",
        u"\u00E4", u"a\u0308", u"\u00F6", u"o\u0308", u"\u00C5", u"\u00E5", u"ch", u"cz", u"c", u"d", u"ll", u"lz",
        u"\u00F1", u"n\u0303", u"nz", u"1", u"2", u"10", u"9", u"x10", u"x9", u"\u0130", u"i", u"I", u"\u0131",
        u"\u4E00", u"\u4E8C", u"\uAC00", u"\u3042", u"\u30A2", u"\u03B1", u"\u0391", u"\u0430", u"\u05D0", u"\u0915",
        u"\U0001F600", std::u16string(1, static_cast<char16_t>(0xD800)), std::u16string(1, static_cast<char16_t>(0xDC00)), std::u16string(u"a\0b", 3), u"\uFFFD", u"\u00DF", u"ss", u"\u00C6", u"AE", u"\u0153",
        u"\u00E4b", u"\u00E4\u0323", u"a\u0323\u0308", u"a\u0308\u0323x", u"\u1EA1\u0308", u"\u0F73", u"\u0F71\u0F72", u"\u00C5\u0327b"};
    return strings;
}

std::string Utf8(const std::u16string& text) {
    std::string result;
    for (std::size_t index = 0; index < text.size(); ++index) {
        std::uint32_t c = text[index];
        if (c >= 0xD800 && c <= 0xDBFF && index + 1 < text.size() && text[index + 1] >= 0xDC00 && text[index + 1] <= 0xDFFF) {
            c = 0x10000 + ((c - 0xD800) << 10) + (text[++index] - 0xDC00);
        } else if (c >= 0xD800 && c <= 0xDFFF) {
            c = 0xFFFD;
        }
        if (c < 0x80) {
            result += static_cast<char>(c);
        } else if (c < 0x800) {
            result += static_cast<char>(0xC0 | (c >> 6));
            result += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            result += static_cast<char>(0xE0 | (c >> 12));
            result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            result += static_cast<char>(0xF0 | (c >> 18));
            result += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return result;
}

void CheckOpen() {
    Require(Throws<std::invalid_argument>([] { ucol_open_nid_postfix("", nullptr); }), "a null status throws");
    std::int32_t status = IllegalArgument;
    Require(!ucol_open_nid_postfix(nullptr, &status) && status == IllegalArgument, "a failed status returns NULL without reading the default locale");
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; ucol_open_nid_postfix(nullptr, &ok); }), "the console's default locale is unknown");
    status = Ok;
    UCollator* root = ucol_open_nid_postfix("", &status);
    Require(root && status == Ok, "the root collator opens without a warning");
    status = Ok;
    UCollator* unknown = ucol_open_nid_postfix("xx", &status);
    Require(unknown && status == UsingDefault, "an unknown locale falls back to root without consulting the default locale");
    UErrorCode hostStatus = U_ZERO_ERROR;
    Require(std::strcmp(ucol_getLocaleByType(unknown, ULOC_VALID_LOCALE, &hostStatus), "root") == 0, "the fallback is root");
    ucol_close_nid_postfix(unknown);
    ucol_close_nid_postfix(root);
    ucol_close_nid_postfix(nullptr);
    for (const char* locale : {"en_XX", "de@collation=phonebook", "de@collation=bogus", "es@collation=traditional", "zh_Hant_TW", "sr_Latn"}) {
        std::int32_t guestStatus = Ok;
        UErrorCode expected = U_ZERO_ERROR;
        UCollator* guest = ucol_open_nid_postfix(locale, &guestStatus);
        UCollator* host = ucol_open(locale, &expected);
        Require(guest && host && guestStatus == expected, "open statuses match ICU");
        UErrorCode first = U_ZERO_ERROR;
        UErrorCode second = U_ZERO_ERROR;
        Require(std::strcmp(ucol_getLocaleByType(guest, ULOC_ACTUAL_LOCALE, &first), ucol_getLocaleByType(host, ULOC_ACTUAL_LOCALE, &second)) == 0, "the same tailoring opens");
        ucol_close_nid_postfix(guest);
        ucol_close(host);
    }
    std::int32_t setStatus = Ok;
    uloc_setDefault_nid_postfix("sv", &setStatus);
    status = Ok;
    UCollator* swedish = ucol_open_nid_postfix(nullptr, &status);
    Require(swedish && status == Ok, "a guest default locale opens");
    Require(Compare(swedish, u"z", u"\u00F6") == Less, "the guest default locale is used");
    ucol_close_nid_postfix(swedish);
    uloc_setDefault_nid_postfix(nullptr, &setStatus);
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; ucol_open_nid_postfix(nullptr, &ok); }), "the default is unknown again");
}

void CheckIDefault() {
    char tag[32];
    UErrorCode tagStatus = U_ZERO_ERROR;
    Require(uloc_forLanguageTag("i-default", tag, sizeof(tag), nullptr, &tagStatus) > 0 && std::strcmp(tag, "en@x=i-default") == 0, "ICU converts a plain i-default as a grandfathered language tag");
    for (const char* locale : {"i-default@collation=phonebook", "i-default_US@x=y", "i-default-x@a=b", "i-default.utf8", "I-DEFAULT@collation=phonebook", "i_default@collation=phonebook"}) {
        Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; ucol_open_nid_postfix(locale, &ok); }), "an i-default locale that ICU canonicalizes to the default throws");
        std::int32_t status = IllegalArgument;
        Require(!ucol_open_nid_postfix(locale, &status) && status == IllegalArgument, "a failed status returns before the locale");
    }
    for (const char* locale : {"i-default", "I-DEFAULT", "i-defaults@collation=phonebook", "de@collation=phonebook"}) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        UCollator* guest = ucol_open_nid_postfix(locale, &status);
        UCollator* host = ucol_open(locale, &hostStatus);
        Require(guest && host && status == hostStatus, "a locale ICU canonicalizes without the default opens as in ICU");
        UErrorCode first = U_ZERO_ERROR;
        UErrorCode second = U_ZERO_ERROR;
        Require(std::strcmp(ucol_getLocaleByType(guest, ULOC_ACTUAL_LOCALE, &first), ucol_getLocaleByType(host, ULOC_ACTUAL_LOCALE, &second)) == 0, "the same tailoring opens without the default");
        Require(Compare(guest, u"\u00E4", u"ad") == ucol_strcoll(host, u"\u00E4", -1, u"ad", -1), "the collator orders as ICU's");
        ucol_close_nid_postfix(guest);
        ucol_close(host);
    }
    std::int32_t setStatus = Ok;
    UErrorCode hostSetStatus = U_ZERO_ERROR;
    uloc_setDefault_nid_postfix("de", &setStatus);
    uloc_setDefault("de", &hostSetStatus);
    for (const char* locale : {"i-default@collation=phonebook", "i-default_US@x=y", "i-default.utf8", "I-DEFAULT@collation=phonebook"}) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        UCollator* guest = ucol_open_nid_postfix(locale, &status);
        UCollator* host = ucol_open(locale, &hostStatus);
        Require(guest && host && status == hostStatus, "an i-default locale opens with a guest default");
        UErrorCode first = U_ZERO_ERROR;
        UErrorCode second = U_ZERO_ERROR;
        Require(std::strcmp(ucol_getLocaleByType(guest, ULOC_ACTUAL_LOCALE, &first), ucol_getLocaleByType(host, ULOC_ACTUAL_LOCALE, &second)) == 0, "the guest default's tailoring opens");
        Require(Compare(guest, u"\u00E4", u"ad") == ucol_strcoll(host, u"\u00E4", -1, u"ad", -1), "the guest default orders as ICU's");
        ucol_close_nid_postfix(guest);
        ucol_close(host);
    }
    std::int32_t status = Ok;
    UCollator* phonebook = ucol_open_nid_postfix("i-default@collation=phonebook", &status);
    Require(phonebook && Compare(phonebook, u"\u00E4", u"ad") == Greater, "the guest default with the phonebook keyword sorts a umlaut as ae");
    ucol_close_nid_postfix(phonebook);
    uloc_setDefault_nid_postfix(nullptr, &setStatus);
    Require(Throws<std::runtime_error>([] { std::int32_t ok = Ok; ucol_open_nid_postfix("i-default@collation=phonebook", &ok); }), "the i-default locale throws again without a guest default");
}

void CheckKnownOrder() {
    UCollator* root = Open("");
    UCollator* german = Open("de");
    UCollator* swedish = Open("sv");
    UCollator* traditional = Open("es@collation=traditional");
    UCollator* phonebook = Open("de@collation=phonebook");
    Require(Compare(root, u"a", u"b") == Less && Compare(root, u"b", u"a") == Greater && Compare(root, u"a", u"a") == Equal, "basic order");
    Require(Compare(root, u"a", u"A") == Less && Compare(root, u"ab", u"Ab") == Less, "lowercase first at the tertiary level");
    Require(Compare(german, u"\u00F6", u"z") == Less && Compare(swedish, u"\u00F6", u"z") == Greater, "o umlaut sorts with o in German and after z in Swedish");
    Require(Compare(traditional, u"cz", u"ch") == Less && Compare(root, u"ch", u"cz") == Less, "traditional Spanish ch contraction");
    Require(Compare(phonebook, u"\u00E4", u"ad") == Greater && Compare(german, u"\u00E4", u"ad") == Less, "phonebook a umlaut as ae");
    Require(Compare(root, u"a\u0308", u"\u00E4") == Equal, "canonically equivalent strings are equal");
    Require(ucol_strcoll_nid_postfix(root, u"abc", 2, u"abd", 2) == Equal, "explicit lengths limit the comparison");
    Require(ucol_strcoll_nid_postfix(root, nullptr, 0, u"", -1) == Equal && ucol_strcoll_nid_postfix(root, nullptr, 3, u"a", -1) == Equal, "null strings are ICU's illegal argument, reported as equal");
    Require(Throws<std::invalid_argument>([] { ucol_strcoll_nid_postfix(nullptr, u"a", -1, u"b", -1); }), "a null collator throws in ucol_strcoll");
    ucol_close_nid_postfix(phonebook);
    ucol_close_nid_postfix(traditional);
    ucol_close_nid_postfix(swedish);
    ucol_close_nid_postfix(german);
    ucol_close_nid_postfix(root);
}

void CheckUtf8() {
    UCollator* root = Open("");
    Require(Throws<std::invalid_argument>([&] { ucol_strcollUTF8_nid_postfix(root, "a", -1, "b", -1, nullptr); }), "a null status throws in ucol_strcollUTF8");
    std::int32_t status = IllegalArgument;
    Require(ucol_strcollUTF8_nid_postfix(nullptr, "a", -1, "b", -1, &status) == Equal && status == IllegalArgument, "a failed status returns before the collator is read");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; ucol_strcollUTF8_nid_postfix(nullptr, "a", -1, "b", -1, &ok); }), "a null collator throws in ucol_strcollUTF8");
    status = Ok;
    Require(ucol_strcollUTF8_nid_postfix(root, nullptr, 1, "b", -1, &status) == Equal && status == IllegalArgument, "a null UTF-8 string with a length is an illegal argument");
    status = Ok;
    Require(ucol_strcollUTF8_nid_postfix(root, "a", -1, "b", -1, &status) == Less && status == Ok, "UTF-8 order");
    ucol_close_nid_postfix(root);
}

void CheckSweep() {
    for (const char* locale : {"", "de", "sv", "es@collation=traditional", "ja", "tr", "hi"}) {
        UCollator* guest = Open(locale);
        UCollator* host = HostOpen(locale);
        for (std::int32_t strength : {UCOL_PRIMARY, UCOL_SECONDARY, UCOL_TERTIARY, UCOL_QUATERNARY, UCOL_IDENTICAL}) {
            ucol_setStrength_nid_postfix(guest, strength);
            ucol_setStrength(host, static_cast<UCollationStrength>(strength));
            Require(ucol_getStrength_nid_postfix(guest) == strength, "the strength is set");
            for (const auto& left : Strings()) {
                for (const auto& right : Strings()) {
                    const auto leftLength = static_cast<std::int32_t>(left.size());
                    const auto rightLength = static_cast<std::int32_t>(right.size());
                    const std::int32_t expected = ucol_strcoll(host, left.data(), leftLength, right.data(), rightLength);
                    Require(ucol_strcoll_nid_postfix(guest, left.data(), leftLength, right.data(), rightLength) == expected, "ucol_strcoll matches ICU");
                    Require(ucol_strcoll_nid_postfix(guest, left.c_str(), -1, right.data(), rightLength) == ucol_strcoll(host, left.c_str(), -1, right.data(), rightLength), "ucol_strcoll with a terminated string matches ICU");
                    const std::string leftUtf8 = Utf8(left);
                    const std::string rightUtf8 = Utf8(right);
                    std::int32_t guestStatus = Ok;
                    UErrorCode hostStatus = U_ZERO_ERROR;
                    Require(ucol_strcollUTF8_nid_postfix(guest, leftUtf8.data(), static_cast<std::int32_t>(leftUtf8.size()), rightUtf8.c_str(), -1, &guestStatus) ==
                                    ucol_strcollUTF8(host, leftUtf8.data(), static_cast<std::int32_t>(leftUtf8.size()), rightUtf8.c_str(), -1, &hostStatus) &&
                                guestStatus == hostStatus,
                            "ucol_strcollUTF8 matches ICU");
                }
            }
        }
        ucol_close(host);
        ucol_close_nid_postfix(guest);
    }
}

struct IterResult {
    std::int32_t result;
    std::int32_t status;
    std::int32_t leftIndex;
    std::int32_t rightIndex;
};

IterResult GuestIter(const UCollator* collator, const std::u16string& left, const std::u16string& right, bool utf8) {
    const std::string leftUtf8 = Utf8(left);
    const std::string rightUtf8 = Utf8(right);
    void* first = utf8 ? GuestUtf8Iterator(leftUtf8.data(), static_cast<std::int32_t>(leftUtf8.size())) : GuestStringIterator(left.data(), static_cast<std::int32_t>(left.size()));
    void* second = utf8 ? GuestUtf8Iterator(rightUtf8.data(), static_cast<std::int32_t>(rightUtf8.size())) : GuestStringIterator(right.data(), static_cast<std::int32_t>(right.size()));
    std::int32_t status = Ok;
    const std::int32_t result = ucol_strcollIter_nid_postfix(collator, static_cast<UCharIterator*>(first), static_cast<UCharIterator*>(second), &status);
    const IterResult outcome{result, status, GuestIteratorIndex(first), GuestIteratorIndex(second)};
    FreeGuestIterator(first);
    FreeGuestIterator(second);
    return outcome;
}

IterResult HostIter(const UCollator* collator, const std::u16string& left, const std::u16string& right, bool utf8) {
    UCharIterator first;
    UCharIterator second;
    const std::string leftUtf8 = Utf8(left);
    const std::string rightUtf8 = Utf8(right);
    if (utf8) {
        uiter_setUTF8(&first, leftUtf8.data(), static_cast<std::int32_t>(leftUtf8.size()));
        uiter_setUTF8(&second, rightUtf8.data(), static_cast<std::int32_t>(rightUtf8.size()));
    } else {
        uiter_setString(&first, left.data(), static_cast<std::int32_t>(left.size()));
        uiter_setString(&second, right.data(), static_cast<std::int32_t>(right.size()));
    }
    UErrorCode status = U_ZERO_ERROR;
    const std::int32_t result = ucol_strcollIter(collator, &first, &second, &status);
    return {result, status, first.index, second.index};
}

void CheckIterators() {
    UCollator* root = Open("");
    auto* first = static_cast<UCharIterator*>(GuestStringIterator(u"a", -1));
    auto* second = static_cast<UCharIterator*>(GuestStringIterator(u"b", -1));
    Require(ucol_strcollIter_nid_postfix(root, first, second, nullptr) == Equal, "a null status returns equal, as in ICU");
    std::int32_t status = IllegalArgument;
    Require(ucol_strcollIter_nid_postfix(root, first, second, &status) == Equal && status == IllegalArgument, "a failed status returns equal");
    status = Ok;
    Require(ucol_strcollIter_nid_postfix(nullptr, first, second, &status) == Equal && status == IllegalArgument, "a null collator is an illegal argument");
    status = Ok;
    Require(ucol_strcollIter_nid_postfix(root, nullptr, second, &status) == Equal && status == IllegalArgument, "a null source iterator is an illegal argument");
    status = Ok;
    Require(ucol_strcollIter_nid_postfix(root, first, nullptr, &status) == Equal && status == IllegalArgument, "a null target iterator is an illegal argument");
    status = Ok;
    Require(ucol_strcollIter_nid_postfix(root, first, second, &status) == Less && status == Ok, "guest iterators compare");
    UCharIterator empty{};
    status = Ok;
    Require(ucol_strcollIter_nid_postfix(root, &empty, &empty, &status) == Equal && status == Ok, "the same iterator is equal without being read");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; ucol_strcollIter_nid_postfix(root, &empty, second, &ok); }), "a null guest callback that ICU calls throws");
    FreeGuestIterator(first);
    FreeGuestIterator(second);
    ucol_close_nid_postfix(root);
    for (const char* locale : {"", "de", "es@collation=traditional", "ja"}) {
        UCollator* guest = Open(locale);
        UCollator* host = HostOpen(locale);
        for (std::int32_t strength : {UCOL_PRIMARY, UCOL_TERTIARY, UCOL_IDENTICAL}) {
            for (std::int32_t normalization : {UCOL_OFF, UCOL_ON}) {
                std::int32_t guestStatus = Ok;
                UErrorCode hostStatus = U_ZERO_ERROR;
                ucol_setStrength_nid_postfix(guest, strength);
                ucol_setStrength(host, static_cast<UCollationStrength>(strength));
                ucol_setAttribute_nid_postfix(guest, UCOL_NORMALIZATION_MODE, normalization, &guestStatus);
                ucol_setAttribute(host, UCOL_NORMALIZATION_MODE, static_cast<UColAttributeValue>(normalization), &hostStatus);
                for (bool utf8 : {false, true}) {
                    for (const auto& left : Strings()) {
                        for (const auto& right : Strings()) {
                            const IterResult guestResult = GuestIter(guest, left, right, utf8);
                            const IterResult hostResult = HostIter(host, left, right, utf8);
                            Require(guestResult.result == hostResult.result && guestResult.status == hostResult.status, "ucol_strcollIter matches ICU");
                            Require(guestResult.leftIndex == hostResult.leftIndex && guestResult.rightIndex == hostResult.rightIndex, "the guest iterators end where ICU's do");
                        }
                    }
                }
            }
        }
        ucol_close(host);
        ucol_close_nid_postfix(guest);
    }
}

void CheckAttributes() {
    Require(ucol_getStrength_nid_postfix(nullptr) == UCOL_DEFAULT, "a null collator has the default strength");
    ucol_setStrength_nid_postfix(nullptr, UCOL_PRIMARY);
    Require(Throws<std::invalid_argument>([] { ucol_getAttribute_nid_postfix(nullptr, UCOL_STRENGTH, nullptr); }), "a null status throws in ucol_getAttribute");
    Require(Throws<std::invalid_argument>([] { ucol_setAttribute_nid_postfix(nullptr, UCOL_STRENGTH, UCOL_PRIMARY, nullptr); }), "a null status throws in ucol_setAttribute");
    std::int32_t status = Ok;
    Require(ucol_getAttribute_nid_postfix(nullptr, UCOL_STRENGTH, &status) == UCOL_DEFAULT && status == Ok, "a null collator returns the default attribute without an error");
    ucol_setAttribute_nid_postfix(nullptr, UCOL_STRENGTH, UCOL_PRIMARY, &status);
    Require(status == Ok, "setting an attribute of a null collator does nothing");
    UCollator* root = Open("");
    ucol_setStrength_nid_postfix(root, UCOL_PRIMARY);
    Require(Compare(root, u"a", u"A") == Equal && Compare(root, u"a", u"\u00E4") == Equal, "primary strength ignores case and accents");
    ucol_setStrength_nid_postfix(root, UCOL_SECONDARY);
    Require(Compare(root, u"a", u"A") == Equal && Compare(root, u"a", u"\u00E4") == Less, "secondary strength sees accents");
    status = Ok;
    ucol_setAttribute_nid_postfix(root, UCOL_STRENGTH, UCOL_DEFAULT, &status);
    Require(status == Ok && ucol_getStrength_nid_postfix(root) == UCOL_TERTIARY, "the default strength is tertiary");
    status = Ok;
    Require(Compare(root, u"10", u"9") == Less, "digits compare as text");
    ucol_setAttribute_nid_postfix(root, UCOL_NUMERIC_COLLATION, UCOL_ON, &status);
    Require(status == Ok && ucol_getAttribute_nid_postfix(root, UCOL_NUMERIC_COLLATION, &status) == UCOL_ON && Compare(root, u"10", u"9") == Greater, "numeric collation");
    ucol_setAttribute_nid_postfix(root, UCOL_CASE_FIRST, UCOL_UPPER_FIRST, &status);
    Require(status == Ok && Compare(root, u"A", u"a") == Less, "uppercase first");
    ucol_setAttribute_nid_postfix(root, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, &status);
    ucol_setStrength_nid_postfix(root, UCOL_TERTIARY);
    Require(status == Ok && Compare(root, u"a-b", u"ab") == Equal, "shifted punctuation is ignorable");
    status = IllegalArgument;
    ucol_setAttribute_nid_postfix(root, UCOL_ALTERNATE_HANDLING, UCOL_NON_IGNORABLE, &status);
    Require(status == IllegalArgument && ucol_getAttribute_nid_postfix(root, UCOL_ALTERNATE_HANDLING, &status) == UCOL_DEFAULT, "a failed status changes nothing");
    status = Ok;
    Require(ucol_getAttribute_nid_postfix(root, UCOL_ALTERNATE_HANDLING, &status) == UCOL_SHIFTED, "the attribute is kept");
    Require(ucol_getAttribute_nid_postfix(root, UCOL_ATTRIBUTE_COUNT, &status) == UCOL_DEFAULT && status == IllegalArgument, "an unknown attribute is an illegal argument");
    status = Ok;
    ucol_setAttribute_nid_postfix(root, UCOL_STRENGTH, 99, &status);
    Require(status == IllegalArgument, "an unknown value is an illegal argument");
    ucol_close_nid_postfix(root);
    for (const char* locale : {"", "da", "fr_CA", "ja", "th", "de@colNumeric=yes;colCaseFirst=upper"}) {
        UCollator* guest = Open(locale);
        UCollator* host = HostOpen(locale);
        for (std::int32_t attribute = UCOL_FRENCH_COLLATION; attribute <= UCOL_ATTRIBUTE_COUNT; ++attribute) {
            std::int32_t guestStatus = Ok;
            UErrorCode hostStatus = U_ZERO_ERROR;
            Require(ucol_getAttribute_nid_postfix(guest, attribute, &guestStatus) == ucol_getAttribute(host, static_cast<UColAttribute>(attribute), &hostStatus) && guestStatus == hostStatus, "attributes match ICU");
        }
        ucol_close(host);
        ucol_close_nid_postfix(guest);
    }
}

void CheckRules() {
    Require(Throws<std::invalid_argument>([] { std::int32_t length = 0; ucol_getRules_nid_postfix(nullptr, &length); }), "a null collator throws in ucol_getRules");
    UCollator* root = Open("");
    Require(Throws<std::invalid_argument>([&] { ucol_getRules_nid_postfix(root, nullptr); }), "a null length throws in ucol_getRules");
    std::int32_t length = -1;
    const char16_t* rules = ucol_getRules_nid_postfix(root, &length);
    Require(rules && length == 0 && rules[0] == 0, "root has no tailoring rules");
    ucol_close_nid_postfix(root);
    for (const char* locale : {"sv", "es@collation=traditional", "ja", "de"}) {
        UCollator* guest = Open(locale);
        UCollator* host = HostOpen(locale);
        std::int32_t guestLength = -1;
        std::int32_t hostLength = -1;
        const char16_t* guestRules = ucol_getRules_nid_postfix(guest, &guestLength);
        const UChar* hostRules = ucol_getRules(host, &hostLength);
        Require(guestLength == hostLength && std::memcmp(guestRules, hostRules, sizeof(char16_t) * (static_cast<std::size_t>(hostLength) + 1)) == 0, "rules match ICU");
        ucol_close(host);
        ucol_close_nid_postfix(guest);
    }
    UCollator* swedish = Open("sv");
    Require(ucol_getRules_nid_postfix(swedish, &length) && length > 0, "Swedish has tailoring rules");
    ucol_close_nid_postfix(swedish);
}

void CheckReorderCodes() {
    std::int32_t codes[16];
    Require(Throws<std::invalid_argument>([&] { ucol_getReorderCodes_nid_postfix(nullptr, codes, 16, nullptr); }), "a null status throws in ucol_getReorderCodes");
    std::int32_t status = IllegalArgument;
    Require(ucol_getReorderCodes_nid_postfix(nullptr, codes, 16, &status) == 0 && status == IllegalArgument, "a failed status returns before the collator is read");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; ucol_getReorderCodes_nid_postfix(nullptr, codes, 16, &ok); }), "a null collator throws in ucol_getReorderCodes");
    UCollator* hindi = Open("hi");
    UCollator* host = HostOpen("hi");
    std::int32_t hostCodes[16];
    UErrorCode hostStatus = U_ZERO_ERROR;
    const std::int32_t expected = ucol_getReorderCodes(host, hostCodes, 16, &hostStatus);
    status = Ok;
    Require(expected > 1 && ucol_getReorderCodes_nid_postfix(hindi, codes, 16, &status) == expected && status == Ok && std::memcmp(codes, hostCodes, sizeof(std::int32_t) * static_cast<std::size_t>(expected)) == 0, "reorder codes match ICU");
    status = Ok;
    Require(ucol_getReorderCodes_nid_postfix(hindi, nullptr, 0, &status) == expected && status == BufferOverflow, "preflighting reports the length");
    status = Ok;
    Require(ucol_getReorderCodes_nid_postfix(hindi, codes, expected - 1, &status) == expected && status == BufferOverflow, "a short buffer overflows");
    status = Ok;
    Require(ucol_getReorderCodes_nid_postfix(hindi, codes, -1, &status) == 0 && status == IllegalArgument, "a negative capacity is an illegal argument");
    status = Ok;
    Require(ucol_getReorderCodes_nid_postfix(hindi, nullptr, 1, &status) == 0 && status == IllegalArgument, "a null buffer with a capacity is an illegal argument");
    UCollator* root = Open("");
    status = Ok;
    Require(ucol_getReorderCodes_nid_postfix(root, nullptr, 0, &status) == 0 && status == Ok, "root has no reorder codes");
    ucol_close_nid_postfix(root);
    ucol_close(host);
    ucol_close_nid_postfix(hindi);
}

void CheckAvailable() {
    const std::int32_t count = ucol_countAvailable_nid_postfix();
    Require(count == ucol_countAvailable() && count > 100, "the available count matches ICU");
    for (std::int32_t index = 0; index < count; ++index) {
        const char* guest = ucol_getAvailable_nid_postfix(index);
        Require(guest && std::strcmp(guest, ucol_getAvailable(index)) == 0, "available locales match ICU");
    }
    Require(!ucol_getAvailable_nid_postfix(count) && !ucol_getAvailable_nid_postfix(count + 100), "an index past the end returns NULL");
    Require(Throws<std::invalid_argument>([] { ucol_getAvailable_nid_postfix(-1); }), "a negative index throws");
}

void CheckTailoredSet() {
    Require(Throws<std::invalid_argument>([] { ucol_getTailoredSet_nid_postfix(nullptr, nullptr); }), "a null status throws in ucol_getTailoredSet");
    std::int32_t status = IllegalArgument;
    Require(!ucol_getTailoredSet_nid_postfix(nullptr, &status) && status == IllegalArgument, "a failed status returns NULL before the collator is read");
    Require(Throws<std::invalid_argument>([] { std::int32_t ok = Ok; ucol_getTailoredSet_nid_postfix(nullptr, &ok); }), "a null collator throws in ucol_getTailoredSet");
    UCollator* root = Open("");
    status = Ok;
    USet* empty = ucol_getTailoredSet_nid_postfix(root, &status);
    Require(empty && status == Ok && uset_getItemCount_nid_postfix(empty) == 0, "root tailors nothing");
    uset_close_nid_postfix(empty);
    ucol_close_nid_postfix(root);
    for (const char* locale : {"haw", "ko", "af", "pa"}) {
        UCollator* guest = Open(locale);
        UCollator* host = HostOpen(locale);
        status = Ok;
        USet* guestSet = ucol_getTailoredSet_nid_postfix(guest, &status);
        UErrorCode hostStatus = U_ZERO_ERROR;
        USet* hostSet = ucol_getTailoredSet(host, &hostStatus);
        const icu::UnicodeSet* expected = icu::UnicodeSet::fromUSet(hostSet);
        Require(guestSet && status == hostStatus && expected->getRangeCount() > 0 && uset_getItemCount(hostSet) == expected->getRangeCount(), "a string-free tailored set converts");
        Require(uset_getItemCount_nid_postfix(guestSet) == expected->getRangeCount(), "the range count matches ICU");
        for (std::int32_t index = 0; index < expected->getRangeCount(); ++index) {
            std::int32_t start = -1;
            std::int32_t end = -1;
            std::int32_t error = Ok;
            uset_getItem_nid_postfix(guestSet, index, &start, &end, nullptr, 0, &error);
            Require(error == Ok && start == expected->getRangeStart(index) && end == expected->getRangeEnd(index), "the ranges match ICU");
        }
        uset_close(hostSet);
        uset_close_nid_postfix(guestSet);
        ucol_close(host);
        ucol_close_nid_postfix(guest);
    }
    UCollator* croatian = Open("hr");
    Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; ucol_getTailoredSet_nid_postfix(croatian, &ok); }), "a tailored set with strings throws");
    ucol_close_nid_postfix(croatian);
}

}

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    CheckOpen();
    CheckIDefault();
    CheckKnownOrder();
    CheckUtf8();
    CheckSweep();
    CheckIterators();
    CheckAttributes();
    CheckRules();
    CheckReorderCodes();
    CheckAvailable();
    CheckTailoredSet();
    return 0;
}
