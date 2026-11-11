#include "prx/libSceIcu/include/LegacyExports.hpp"
#include "prx/libSceIcu/Normalization.hpp"
#include "prx/libSceIcu/Set.hpp"
#include <unicode/udata.h>
#include <unicode/unorm.h>
#include <unicode/unorm2.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "ICU NORMALIZATION: %s\n", message);
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

std::u16string Normalize(const UNormalizer2* normalizer, const std::u16string& text) {
    char16_t buffer[64]{};
    std::int32_t status = Ok;
    const std::int32_t length = unorm2_normalize_nid_postfix(normalizer, text.data(), static_cast<std::int32_t>(text.size()), buffer, 64, &status);
    Require(status == Ok && length >= 0 && length < 64, "normalize succeeds");
    return std::u16string(buffer, static_cast<std::size_t>(length));
}

std::u16string HostNormalize(const UNormalizer2* normalizer, const std::u16string& text) {
    char16_t buffer[64]{};
    UErrorCode status = U_ZERO_ERROR;
    const std::int32_t length = unorm2_normalize(normalizer, text.data(), static_cast<std::int32_t>(text.size()), buffer, 64, &status);
    Require(U_SUCCESS(status), "host normalize succeeds");
    return std::u16string(buffer, static_cast<std::size_t>(length));
}

struct Forms {
    const UNormalizer2* nfc;
    const UNormalizer2* nfd;
    const UNormalizer2* nfkc;
    const UNormalizer2* nfkd;
    const UNormalizer2* nfkcCasefold;
};

Forms GuestForms() {
    std::int32_t status = Ok;
    Forms forms{unorm2_getNFCInstance_nid_postfix(&status), unorm2_getNFDInstance_nid_postfix(&status), unorm2_getNFKCInstance_nid_postfix(&status),
        unorm2_getNFKDInstance_nid_postfix(&status), unorm2_getNFKCCasefoldInstance_nid_postfix(&status)};
    Require(status == Ok && forms.nfc && forms.nfd && forms.nfkc && forms.nfkd && forms.nfkcCasefold, "the five instances load from the embedded data");
    return forms;
}

Forms HostForms() {
    UErrorCode status = U_ZERO_ERROR;
    Forms forms{unorm2_getNFCInstance(&status), unorm2_getNFDInstance(&status), unorm2_getNFKCInstance(&status), unorm2_getNFKDInstance(&status),
        unorm2_getNFKCCasefoldInstance(&status)};
    Require(U_SUCCESS(status), "the host instances load");
    return forms;
}

void CheckInstances(const Forms& forms) {
    Require(Throws<std::invalid_argument>([] { unorm2_getNFCInstance_nid_postfix(nullptr); }), "a null status throws");
    std::int32_t failed = IllegalArgument;
    Require(unorm2_getNFDInstance_nid_postfix(&failed) == nullptr && failed == IllegalArgument, "a failed status returns null and stays");
    std::int32_t status = Ok;
    Require(unorm2_getInstance_nid_postfix(nullptr, "nfc", UNORM2_COMPOSE, &status) == forms.nfc && status == Ok, "getInstance nfc compose is the NFC singleton");
    Require(unorm2_getInstance_nid_postfix(nullptr, "nfkc_cf", UNORM2_COMPOSE, &status) == forms.nfkcCasefold && status == Ok, "getInstance nfkc_cf is the casefold singleton");
    const UNormalizer2* uts46 = unorm2_getInstance_nid_postfix(nullptr, "uts46", UNORM2_COMPOSE, &status);
    Require(uts46 && status == Ok && Normalize(uts46, u"ÅB") == u"åb", "the uts46 data loads from the package");
    Require(unorm2_getInstance_nid_postfix(nullptr, "nfc", UNORM2_DECOMPOSE, &status) == forms.nfd && status == Ok, "the decompose mode is the NFD singleton");
    Require(unorm2_getInstance_nid_postfix(nullptr, "nfkc", UNORM2_DECOMPOSE, &status) == forms.nfkd && status == Ok, "nfkc decompose is the NFKD singleton");
    const UNormalizer2* fcd = unorm2_getInstance_nid_postfix(nullptr, "nfc", UNORM2_FCD, &status);
    Require(fcd && status == Ok && Normalize(fcd, u"Å") == u"Å" && Normalize(fcd, u"á̧") == u"á̧", "the FCD mode keeps precomposed text and reorders marks");
    const UNormalizer2* contiguous = unorm2_getInstance_nid_postfix(nullptr, "nfc", UNORM2_COMPOSE_CONTIGUOUS, &status);
    Require(contiguous && status == Ok && Normalize(contiguous, u"Å") == u"Å", "the contiguous compose mode loads");
    unorm2_getInstance_nid_postfix(nullptr, "missing", UNORM2_COMPOSE, &status);
    Require(status > Ok, "an unknown normalizer name fails");
    status = Ok;
    unorm2_getInstance_nid_postfix(nullptr, nullptr, UNORM2_COMPOSE, &status);
    Require(status == IllegalArgument, "a null name is an illegal argument");
    status = Ok;
    Require(Throws<std::runtime_error>([&] { unorm2_getInstance_nid_postfix("package", "nfc", UNORM2_COMPOSE, &status); }), "a package name throws");
    failed = IllegalArgument;
    Require(unorm2_getInstance_nid_postfix("package", "nfc", UNORM2_COMPOSE, &failed) == nullptr && failed == IllegalArgument, "a failed status returns before the package check");
}

void CheckNormalize(const Forms& forms) {
    Require(Normalize(forms.nfc, u"Å") == u"Å", "NFC composes A and ring above");
    Require(Normalize(forms.nfd, u"Å") == u"Å", "NFD decomposes A ring");
    Require(Normalize(forms.nfkc, u"ﬁ") == u"fi", "NFKC expands the fi ligature");
    Require(Normalize(forms.nfkd, u"ẛ̣") == u"ṩ", "NFKD of long s with dots");
    Require(Normalize(forms.nfkcCasefold, u"ABCß") == u"abcss", "NFKC_Casefold folds case and sharp s");
    std::int32_t status = Ok;
    const char16_t source[] = u"Å";
    Require(unorm2_normalize_nid_postfix(forms.nfc, source, -1, nullptr, 0, &status) == 1 && status == BufferOverflow, "preflighting reports the length and overflow");
    status = Ok;
    char16_t small[1]{};
    Require(unorm2_normalize_nid_postfix(forms.nfd, u"Å", 1, small, 1, &status) == 2 && status == BufferOverflow, "a short buffer overflows with the full length");
    status = Ok;
    char16_t exact[2]{};
    Require(unorm2_normalize_nid_postfix(forms.nfd, u"Å", 1, exact, 2, &status) == 2 && status == U_STRING_NOT_TERMINATED_WARNING && exact[0] == u'A' && exact[1] == u'̊', "an exact fit is not terminated");
    status = Ok;
    unorm2_normalize_nid_postfix(forms.nfc, nullptr, 3, small, 1, &status);
    Require(status == IllegalArgument, "a null source with a length is illegal");
    status = Ok;
    char16_t alias[4] = u"abc";
    unorm2_normalize_nid_postfix(forms.nfc, alias, 3, alias, 4, &status);
    Require(status == IllegalArgument, "an aliased destination is illegal");
    status = IllegalArgument;
    Require(unorm2_normalize_nid_postfix(forms.nfc, source, -1, small, 1, &status) == 0 && status == IllegalArgument, "a failed status passes through");
    Require(Throws<std::invalid_argument>([&] { std::int32_t ok = Ok; unorm2_normalize_nid_postfix(nullptr, source, -1, small, 1, &ok); }), "a null normalizer throws");
    Require(Throws<std::invalid_argument>([&] { unorm2_normalize_nid_postfix(forms.nfc, source, -1, small, 1, nullptr); }), "a null status throws");
}

void CheckAppend(const Forms& forms) {
    char16_t first[16] = u"A";
    std::int32_t status = Ok;
    Require(unorm2_normalizeSecondAndAppend_nid_postfix(forms.nfc, first, 1, 16, u"̊́", 2, &status) == 1 && status == Ok, "normalizeSecondAndAppend merges at the boundary");
    Require(first[0] == u'Ǻ' && first[1] == 0, "the merged NFC text is A ring acute");
    char16_t appended[16] = u"A";
    Require(unorm2_append_nid_postfix(forms.nfd, appended, 1, 16, u"̊", 1, &status) == 2 && status == Ok, "append keeps a normalized second string");
    Require(appended[0] == u'A' && appended[1] == u'̊', "append concatenates");
    char16_t unnormalized[16] = u"x";
    Require(unorm2_append_nid_postfix(forms.nfc, unnormalized, 1, 16, u"Å", 2, &status) == 3 && status == Ok && unnormalized[1] == u'A' && unnormalized[2] == u'̊', "append does not normalize a second string after a boundary");
    char16_t normalizedSecond[16] = u"x";
    Require(unorm2_normalizeSecondAndAppend_nid_postfix(forms.nfc, normalizedSecond, 1, 16, u"Å", 2, &status) == 2 && normalizedSecond[1] == u'Å', "normalizeSecondAndAppend normalizes the second string");
    char16_t tight[2] = {u'A', u'B'};
    Require(unorm2_append_nid_postfix(forms.nfc, tight, 2, 2, u"C", 1, &status) == 3 && status == BufferOverflow, "append reports the needed length");
    status = Ok;
    unorm2_normalizeSecondAndAppend_nid_postfix(forms.nfc, nullptr, 1, 0, u"x", 1, &status);
    Require(status == IllegalArgument, "a null first buffer with a length is illegal");
}

void CheckDecompositions(const Forms& forms) {
    char16_t buffer[8]{};
    std::int32_t status = Ok;
    Require(unorm2_getDecomposition_nid_postfix(forms.nfc, 0x00C5, buffer, 8, &status) == 2 && buffer[0] == u'A' && buffer[1] == u'̊', "the decomposition of A ring");
    Require(unorm2_getDecomposition_nid_postfix(forms.nfc, u'A', buffer, 8, &status) == -1 && status == Ok, "a character without a mapping returns -1");
    Require(unorm2_getDecomposition_nid_postfix(forms.nfc, 0x1E08, buffer, 8, &status) == 3 && buffer[0] == u'C' && buffer[1] == u'̧' && buffer[2] == u'́', "the full decomposition of C cedilla acute");
    Require(unorm2_getRawDecomposition_nid_postfix(forms.nfc, 0x1E08, buffer, 8, &status) == 2 && buffer[0] == u'Ç' && buffer[1] == u'́', "the raw decomposition is one level");
    Require(unorm2_getDecomposition_nid_postfix(forms.nfc, 0x1E08, buffer, 1, &status) == 3 && status == BufferOverflow, "a short decomposition buffer overflows");
    status = Ok;
    unorm2_getRawDecomposition_nid_postfix(forms.nfc, 0x1E08, nullptr, 4, &status);
    Require(status == IllegalArgument, "a null buffer with capacity is illegal");
    Require(unorm2_composePair_nid_postfix(forms.nfc, u'A', 0x030A) == 0x00C5, "composePair composes A and ring");
    Require(unorm2_composePair_nid_postfix(forms.nfc, u'A', u'B') == -1, "composePair without a composite is -1");
    Require(unorm2_getCombiningClass_nid_postfix(forms.nfc, 0x0301) == 230 && unorm2_getCombiningClass_nid_postfix(forms.nfc, u'A') == 0, "combining classes");
}

void CheckQueries(const Forms& forms) {
    std::int32_t status = Ok;
    Require(unorm2_isNormalized_nid_postfix(forms.nfc, u"Å", 2, &status) == 0 && status == Ok, "A plus ring is not NFC");
    Require(unorm2_isNormalized_nid_postfix(forms.nfc, u"Å", 1, &status) == 1, "A ring is NFC");
    Require(unorm2_quickCheck_nid_postfix(forms.nfc, u"Å", 2, &status) == UNORM_MAYBE, "a combining mark is a maybe for NFC");
    Require(unorm2_quickCheck_nid_postfix(forms.nfd, u"Å", 1, &status) == UNORM_NO, "a precomposed letter is a no for NFD");
    Require(unorm2_quickCheck_nid_postfix(forms.nfc, u"abc", 3, &status) == UNORM_YES, "ASCII is a yes");
    Require(unorm2_spanQuickCheckYes_nid_postfix(forms.nfd, u"abcÅd", 5, &status) == 3 && status == Ok, "the quick-check yes span stops before A ring");
    Require(unorm2_hasBoundaryBefore_nid_postfix(forms.nfc, u'A') == 1 && unorm2_hasBoundaryBefore_nid_postfix(forms.nfc, 0x0301) == 0, "boundaries before");
    Require(unorm2_hasBoundaryAfter_nid_postfix(forms.nfd, u'.') == 1 && unorm2_hasBoundaryAfter_nid_postfix(forms.nfc, u'A') == 0, "boundaries after");
    Require(unorm2_isInert_nid_postfix(forms.nfc, u'.') == 1 && unorm2_isInert_nid_postfix(forms.nfc, u'A') == 0, "inert characters");
    unorm2_isNormalized_nid_postfix(forms.nfc, nullptr, 2, &status);
    Require(status == IllegalArgument, "isNormalized of a null string with a length is illegal");
    status = IllegalArgument;
    Require(unorm2_spanQuickCheckYes_nid_postfix(forms.nfc, u"abc", 3, &status) == 0 && status == IllegalArgument, "a failed status passes through spanQuickCheckYes");
    Require(Throws<std::invalid_argument>([] { unorm2_composePair_nid_postfix(nullptr, 1, 2); }), "a null normalizer throws in composePair");
}

void CheckFiltered(const Forms& forms) {
    USet* set = uset_openEmpty_nid_postfix();
    uset_add_nid_postfix(set, 0x00C5);
    uset_addRange_nid_postfix(set, 0x1E00, 0x1E09);
    std::int32_t status = Ok;
    UNormalizer2* filtered = unorm2_openFiltered_nid_postfix(forms.nfd, set, &status);
    Require(filtered && status == Ok, "openFiltered builds a normalizer from a guest set");
    uset_close_nid_postfix(set);
    Require(Normalize(filtered, u"ÅÇḈ") == u"ÅÇḈ", "only characters in the filter set decompose");
    unorm2_close_nid_postfix(filtered);
    unorm2_close_nid_postfix(nullptr);
    status = Ok;
    Require(unorm2_openFiltered_nid_postfix(forms.nfc, nullptr, &status) == nullptr && status == IllegalArgument, "a null filter set is illegal");
    status = IllegalArgument;
    Require(unorm2_openFiltered_nid_postfix(forms.nfc, nullptr, &status) == nullptr && status == IllegalArgument, "a failed status returns before the set check");
    USet* empty = uset_openEmpty_nid_postfix();
    status = Ok;
    UNormalizer2* none = unorm2_openFiltered_nid_postfix(forms.nfkc, empty, &status);
    Require(none && Normalize(none, u"ﬁ") == u"ﬁ", "an empty filter leaves text unchanged");
    unorm2_close_nid_postfix(none);
    uset_close_nid_postfix(empty);
}

void CheckCompare() {
    std::int32_t status = Ok;
    Require(unorm_compare_nid_postfix(u"Å", 1, u"Å", 2, 0, &status) == 0 && status == Ok, "canonically equivalent strings compare equal");
    Require(unorm_compare_nid_postfix(u"a", -1, u"A", -1, U_COMPARE_IGNORE_CASE, &status) == 0, "case-insensitive comparison");
    Require(unorm_compare_nid_postfix(u"a", -1, u"b", -1, 0, &status) < 0 && unorm_compare_nid_postfix(u"b", -1, u"a", -1, 0, &status) > 0, "ordering");
    unorm_compare_nid_postfix(nullptr, 1, u"a", 1, 0, &status);
    Require(status == IllegalArgument, "a null string with a length is illegal");
}

void CheckAgainstHost(const Forms& guest, const Forms& host) {
    const std::pair<const UNormalizer2*, const UNormalizer2*> pairs[] = {{guest.nfc, host.nfc}, {guest.nfd, host.nfd}, {guest.nfkc, host.nfkc},
        {guest.nfkd, host.nfkd}, {guest.nfkcCasefold, host.nfkcCasefold}};
    for (std::int32_t codePoint = 0; codePoint <= 0x10FFFF; codePoint += 13) {
        if (codePoint >= 0xD800 && codePoint <= 0xDFFF) continue;
        char16_t units[2]{};
        std::int32_t length = 0;
        if (codePoint < 0x10000) {
            units[length++] = static_cast<char16_t>(codePoint);
        } else {
            units[length++] = static_cast<char16_t>(0xD7C0 + (codePoint >> 10));
            units[length++] = static_cast<char16_t>(0xDC00 | (codePoint & 0x3FF));
        }
        const std::u16string text(units, static_cast<std::size_t>(length));
        for (const auto& [guestForm, hostForm] : pairs) {
            Require(Normalize(guestForm, text) == HostNormalize(hostForm, text), "every form matches the host ICU");
            Require(unorm2_getCombiningClass_nid_postfix(guestForm, codePoint) == unorm2_getCombiningClass(hostForm, codePoint), "combining classes match the host ICU");
            Require(unorm2_isInert_nid_postfix(guestForm, codePoint) == unorm2_isInert(hostForm, codePoint), "inertness matches the host ICU");
        }
    }
}

}

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    const Forms forms = GuestForms();
    CheckInstances(forms);
    CheckNormalize(forms);
    CheckAppend(forms);
    CheckDecompositions(forms);
    CheckQueries(forms);
    CheckFiltered(forms);
    CheckCompare();
    CheckAgainstHost(forms, HostForms());
    Require(Throws<std::runtime_error>([] { unorm2_swap_nid_postfix(); }), "unorm2_swap still throws");
    return 0;
}
