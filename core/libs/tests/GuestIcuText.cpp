#include "prx/libSceIcu/BreakIterator.hpp"
#include "prx/libSceIcu/Text.hpp"
#include <unicode/ubrk.h>
#include <unicode/udata.h>
#include <unicode/utext.h>
#include <unicode/utf16.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t NoWritePermission = 30;
constexpr std::int32_t NotTerminated = -124;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "ICU TEXT: %s\n", message.c_str());
        std::abort();
    }
}

template<typename TAction>
bool Invalid(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

const std::vector<char16_t>& Utf16() {
    static const std::vector<char16_t> text{u'a', 0xd83d, 0xde00, u'b', 0xd800, u'c', 0xdc00, 0x00e9, u'z', 0xd83d};
    return text;
}

const std::vector<char>& Utf8() {
    static const std::vector<char> text{'a', '\xf0', '\x9f', '\x98', '\x80', 'b', '\xff', '\xc3', '\xa9', '\xe2', '\x82', 'z', '\xc3'};
    return text;
}

void CompareWalk(UText* guest, UText* host, const std::string& what) {
    const auto length = utext_nativeLength_nid_postfix(guest);
    Require(length == utext_nativeLength(host), what + ": native length");
    for (std::int64_t index = -2; index <= length + 2; ++index) {
        const std::string at = what + " at " + std::to_string(index);
        Require(utext_char32At_nid_postfix(guest, index) == utext_char32At(host, index), at + ": char32At");
        Require(utext_getNativeIndex_nid_postfix(guest) == utext_getNativeIndex(host), at + ": index after char32At");
        Require(utext_next32From_nid_postfix(guest, index) == utext_next32From(host, index), at + ": next32From");
        Require(utext_getNativeIndex_nid_postfix(guest) == utext_getNativeIndex(host), at + ": index after next32From");
        Require(utext_previous32From_nid_postfix(guest, index) == utext_previous32From(host, index), at + ": previous32From");
        Require(utext_getNativeIndex_nid_postfix(guest) == utext_getNativeIndex(host), at + ": index after previous32From");
        utext_setNativeIndex_nid_postfix(guest, index);
        utext_setNativeIndex(host, index);
        Require(utext_getNativeIndex_nid_postfix(guest) == utext_getNativeIndex(host), at + ": setNativeIndex");
        Require(utext_current32_nid_postfix(guest) == utext_current32(host), at + ": current32");
        Require(utext_getPreviousNativeIndex_nid_postfix(guest) == utext_getPreviousNativeIndex(host), at + ": getPreviousNativeIndex");
        for (const std::int32_t delta : {-3, -1, 0, 1, 2}) {
            utext_setNativeIndex_nid_postfix(guest, index);
            utext_setNativeIndex(host, index);
            const bool moved = utext_moveIndex32_nid_postfix(guest, delta) != 0;
            Require(moved == (utext_moveIndex32(host, delta) != 0) && utext_getNativeIndex_nid_postfix(guest) == utext_getNativeIndex(host), at + ": moveIndex32 " + std::to_string(delta));
        }
    }
    utext_setNativeIndex_nid_postfix(guest, 0);
    utext_setNativeIndex(host, 0);
    for (;;) {
        const auto character = utext_next32_nid_postfix(guest);
        Require(character == utext_next32(host) && utext_getNativeIndex_nid_postfix(guest) == utext_getNativeIndex(host), what + ": forward walk");
        if (character == U_SENTINEL) break;
    }
    for (;;) {
        const auto character = utext_previous32_nid_postfix(guest);
        Require(character == utext_previous32(host) && utext_getNativeIndex_nid_postfix(guest) == utext_getNativeIndex(host), what + ": backward walk");
        if (character == U_SENTINEL) break;
    }
    Require((utext_isLengthExpensive_nid_postfix(guest) != 0) == (utext_isLengthExpensive(host) != 0), what + ": isLengthExpensive");
    Require(utext_isWritable_nid_postfix(guest) == 0 && utext_hasMetaData_nid_postfix(guest) == 0, what + ": properties");
}

void CheckOpen() {
    const auto& utf16 = Utf16();
    for (const std::int64_t length : {static_cast<std::int64_t>(utf16.size()), std::int64_t{5}}) {
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        UText* guest = utext_openUChars_nid_postfix(nullptr, utf16.data(), length, &status);
        UText* host = utext_openUChars(nullptr, utf16.data(), length, &hostStatus);
        Require(status == Ok && guest != nullptr && U_SUCCESS(hostStatus), "a counted UChars text opens on the heap");
        CompareWalk(guest, host, "UChars " + std::to_string(length));
        Require(utext_close_nid_postfix(guest) == nullptr, "closing a heap UText returns NULL");
        utext_close(host);
    }
    const char16_t terminated[] = u"xé\U0001f600y";
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    UText guestStack = UTEXT_INITIALIZER;
    UText hostStack = UTEXT_INITIALIZER;
    Require(utext_openUChars_nid_postfix(&guestStack, terminated, -1, &status) == &guestStack && status == Ok, "a NUL-terminated text opens into a stack UText");
    utext_openUChars(&hostStack, terminated, -1, &hostStatus);
    CompareWalk(&guestStack, &hostStack, "NUL-terminated UChars");
    const auto& utf8 = Utf8();
    Require(utext_openUTF8_nid_postfix(&guestStack, utf8.data(), static_cast<std::int64_t>(utf8.size()), &status) == &guestStack && status == Ok, "an open stack UText reopens as UTF-8");
    utext_openUTF8(&hostStack, utf8.data(), static_cast<std::int64_t>(utf8.size()), &hostStatus);
    CompareWalk(&guestStack, &hostStack, "UTF-8");
    const char terminated8[] = "k\xc3\xa9\xf0\x9f\x98\x80\xedq";
    Require(utext_openUTF8_nid_postfix(&guestStack, terminated8, -1, &status) == &guestStack && status == Ok, "a NUL-terminated UTF-8 text opens");
    utext_openUTF8(&hostStack, terminated8, -1, &hostStatus);
    CompareWalk(&guestStack, &hostStack, "NUL-terminated UTF-8");
    Require(utext_close_nid_postfix(&guestStack) == &guestStack && utext_close(&hostStack) == &hostStack, "closing a stack UText returns it");
    Require(utext_close_nid_postfix(&guestStack) == &guestStack, "closing a closed UText returns it");
    Require(utext_close_nid_postfix(nullptr) == nullptr, "closing NULL returns NULL");

    status = Ok;
    UText* empty = utext_openUChars_nid_postfix(nullptr, nullptr, 0, &status);
    Require(status == Ok && empty != nullptr && utext_nativeLength_nid_postfix(empty) == 0 && utext_next32_nid_postfix(empty) == U_SENTINEL, "a NULL text of length 0 is empty");
    utext_close_nid_postfix(empty);
    status = Ok;
    empty = utext_openUTF8_nid_postfix(nullptr, nullptr, 0, &status);
    Require(status == Ok && empty != nullptr && utext_nativeLength_nid_postfix(empty) == 0, "a NULL UTF-8 text of length 0 is empty");
    utext_close_nid_postfix(empty);
    for (const std::int64_t length : {std::int64_t{3}, std::int64_t{-2}, std::int64_t{0x80000000}}) {
        status = Ok;
        const char16_t* text = length == 3 ? nullptr : utf16.data();
        Require(utext_openUChars_nid_postfix(nullptr, text, length, &status) == nullptr && status == IllegalArgument, "an invalid UChars text " + std::to_string(length));
        status = Ok;
        Require(utext_openUTF8_nid_postfix(nullptr, length == 3 ? nullptr : utf8.data(), length, &status) == nullptr && status == IllegalArgument, "an invalid UTF-8 text " + std::to_string(length));
    }
    status = BufferOverflow;
    Require(utext_openUChars_nid_postfix(nullptr, utf16.data(), 1, &status) == nullptr && status == BufferOverflow, "a failed status opens nothing");
    Require(utext_openUTF8_nid_postfix(nullptr, utf8.data(), 1, &status) == nullptr && status == BufferOverflow, "a failed status opens no UTF-8 text");
    Require(Invalid([&] { utext_openUChars_nid_postfix(nullptr, utf16.data(), 1, nullptr); }), "a NULL status throws in openUChars");
    Require(Invalid([&] { utext_openUTF8_nid_postfix(nullptr, utf8.data(), 1, nullptr); }), "a NULL status throws in openUTF8");
}

void CheckExtract() {
    const char16_t hello[] = u"hello";
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    UText* guest = utext_openUChars_nid_postfix(nullptr, hello, 5, &status);
    UText* host = utext_openUChars(nullptr, hello, 5, &hostStatus);
    for (const std::int32_t capacity : {0, 2, 5, 6}) {
        char16_t buffer[8];
        char16_t hostBuffer[8];
        std::fill(std::begin(buffer), std::end(buffer), u'#');
        std::fill(std::begin(hostBuffer), std::end(hostBuffer), u'#');
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        const auto length = utext_extract_nid_postfix(guest, 0, 5, capacity ? buffer : nullptr, capacity, &status);
        const auto hostLength = utext_extract(host, 0, 5, capacity ? hostBuffer : nullptr, capacity, &hostStatus);
        Require(length == 5 && length == hostLength && status == hostStatus && std::memcmp(buffer, hostBuffer, sizeof(buffer)) == 0, "extract into " + std::to_string(capacity));
    }
    Require(status == Ok, "extract with room terminates the result");
    status = Ok;
    utext_extract_nid_postfix(guest, 0, 5, nullptr, 0, &status);
    Require(status == BufferOverflow, "preflighting reports the length");
    status = Ok;
    char16_t buffer[8];
    Require(utext_extract_nid_postfix(guest, 0, 5, buffer, 5, &status) == 5 && status == NotTerminated, "an exact fit is not terminated");
    status = Ok;
    utext_extract_nid_postfix(guest, 4, 1, buffer, 8, &status);
    hostStatus = U_ZERO_ERROR;
    utext_extract(host, 4, 1, buffer, 8, &hostStatus);
    Require(status == hostStatus, "a reversed range keeps ICU's status");
    status = BufferOverflow;
    Require(utext_extract_nid_postfix(guest, 0, 5, buffer, 8, &status) == 0 && status == BufferOverflow, "a failed status extracts nothing");
    Require(Invalid([&] { utext_extract_nid_postfix(guest, 0, 5, buffer, 8, nullptr); }), "a NULL status throws in extract");
    utext_close_nid_postfix(guest);
    utext_close(host);

    const auto& utf8 = Utf8();
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    guest = utext_openUTF8_nid_postfix(nullptr, utf8.data(), static_cast<std::int64_t>(utf8.size()), &status);
    host = utext_openUTF8(nullptr, utf8.data(), static_cast<std::int64_t>(utf8.size()), &hostStatus);
    for (std::int64_t start = 0; start <= static_cast<std::int64_t>(utf8.size()); ++start) {
        for (std::int64_t limit = start; limit <= static_cast<std::int64_t>(utf8.size()) + 1; ++limit) {
            char16_t text[32] = {};
            char16_t hostText[32] = {};
            status = Ok;
            hostStatus = U_ZERO_ERROR;
            const auto length = utext_extract_nid_postfix(guest, start, limit, text, 32, &status);
            const auto hostLength = utext_extract(host, start, limit, hostText, 32, &hostStatus);
            Require(length == hostLength && status == hostStatus && std::memcmp(text, hostText, sizeof(text)) == 0, "UTF-8 extract " + std::to_string(start) + "-" + std::to_string(limit));
        }
    }
    utext_close_nid_postfix(guest);
    utext_close(host);
}

void CheckClone() {
    const auto& utf16 = Utf16();
    std::int32_t status = Ok;
    UText* source = utext_openUChars_nid_postfix(nullptr, utf16.data(), static_cast<std::int64_t>(utf16.size()), &status);
    utext_setNativeIndex_nid_postfix(source, 3);
    UText* shallow = utext_clone_nid_postfix(nullptr, source, 0, 0, &status);
    Require(status == Ok && shallow != nullptr && shallow != source && utext_equals_nid_postfix(source, shallow) != 0, "a shallow clone equals its source");
    utext_next32_nid_postfix(shallow);
    Require(utext_equals_nid_postfix(source, shallow) == 0, "a clone at another index differs");
    UText* deep = utext_clone_nid_postfix(nullptr, source, 1, 1, &status);
    Require(status == Ok && deep != nullptr && utext_equals_nid_postfix(source, deep) == 0 && utext_isWritable_nid_postfix(deep) == 0, "a deep read-only clone owns its own text");
    UErrorCode hostStatus = U_ZERO_ERROR;
    UText* host = utext_openUChars(nullptr, utf16.data(), static_cast<std::int64_t>(utf16.size()), &hostStatus);
    CompareWalk(deep, host, "deep clone");
    UText stack = UTEXT_INITIALIZER;
    const auto& utf8 = Utf8();
    utext_openUTF8_nid_postfix(&stack, utf8.data(), static_cast<std::int64_t>(utf8.size()), &status);
    Require(utext_clone_nid_postfix(&stack, source, 0, 0, &status) == &stack && status == Ok, "a clone replaces an open stack UText");
    CompareWalk(&stack, host, "clone into a stack UText");
    status = BufferOverflow;
    Require(utext_clone_nid_postfix(&stack, nullptr, 0, 0, &status) == &stack && status == BufferOverflow, "a failed status clones nothing");
    status = Ok;
    Require(Invalid([&] { utext_clone_nid_postfix(nullptr, nullptr, 0, 0, &status); }), "a NULL source throws");
    Require(Invalid([&] { utext_clone_nid_postfix(nullptr, source, 0, 0, nullptr); }), "a NULL status throws in clone");
    Require(utext_equals_nid_postfix(nullptr, source) == 0 && utext_equals_nid_postfix(source, nullptr) == 0, "NULL never equals");
    utext_close_nid_postfix(&stack);
    utext_close_nid_postfix(deep);
    utext_close_nid_postfix(shallow);
    utext_close_nid_postfix(source);
    utext_close(host);
}

void CheckWrite() {
    const char16_t text[] = u"abc";
    std::int32_t status = Ok;
    UText* guest = utext_openUChars_nid_postfix(nullptr, text, 3, &status);
    utext_freeze_nid_postfix(guest);
    Require(utext_isWritable_nid_postfix(guest) == 0, "a frozen text is read-only");
    Require(utext_replace_nid_postfix(guest, 0, 1, u"x", 1, &status) == 0 && status == NoWritePermission, "replace needs a writable text");
    status = Ok;
    utext_copy_nid_postfix(guest, 0, 1, 2, 1, &status);
    Require(status == NoWritePermission, "copy needs a writable text");
    status = BufferOverflow;
    Require(utext_replace_nid_postfix(guest, 0, 1, u"x", 1, &status) == 0 && status == BufferOverflow, "a failed status replaces nothing");
    utext_copy_nid_postfix(guest, 0, 1, 2, 0, &status);
    Require(status == BufferOverflow, "a failed status copies nothing");
    Require(Invalid([&] { utext_replace_nid_postfix(guest, 0, 1, u"x", 1, nullptr); }), "a NULL status throws in replace");
    Require(Invalid([&] { utext_copy_nid_postfix(guest, 0, 1, 2, 0, nullptr); }), "a NULL status throws in copy");
    guest->providerProperties |= 1 << UTEXT_PROVIDER_WRITABLE;
    status = Ok;
    Require(Invalid([&] { utext_replace_nid_postfix(guest, 0, 1, u"x", 1, &status); }), "a writable flag on an ICU provider without replace throws");
    Require(Invalid([&] { utext_copy_nid_postfix(guest, 0, 1, 2, 0, &status); }), "a writable flag on an ICU provider without copy throws");
    utext_close_nid_postfix(guest);
    Require(Invalid([] { utext_freeze_nid_postfix(nullptr); }), "a NULL text throws in freeze");
    Require(Invalid([] { utext_isWritable_nid_postfix(nullptr); }), "a NULL text throws in isWritable");
    Require(Invalid([] { utext_hasMetaData_nid_postfix(nullptr); }), "a NULL text throws in hasMetaData");
    Require(Invalid([] { utext_isLengthExpensive_nid_postfix(nullptr); }), "a NULL text throws in isLengthExpensive");
}

void CheckGuards() {
    const char16_t text[] = u"abc";
    std::int32_t status = Ok;
    UText open = UTEXT_INITIALIZER;
    utext_openUChars_nid_postfix(&open, text, 3, &status);
    static const UTextFuncs guestFunctions{};
    UText provided = open;
    provided.pFuncs = &guestFunctions;
    Require(utext_next32_nid_postfix(&provided) == u'a', "a guest table reads its chunk without calls");
    Require(Invalid([&] { utext_next32From_nid_postfix(&provided, 5); }), "a guest table without access throws in next32From");
    Require(Invalid([&] { utext_nativeLength_nid_postfix(&provided); }), "a guest table without nativeLength throws");
    Require(Invalid([&] { std::int32_t ok = Ok; utext_clone_nid_postfix(nullptr, &provided, 0, 0, &ok); }), "a guest table without clone throws");
    Require(provided.pFuncs == &guestFunctions, "a throwing call restores the guest table");
    Require(utext_equals_nid_postfix(&open, &provided) == 0, "texts of different providers are not equal");
    UText reopened = provided;
    Require(utext_openUChars_nid_postfix(&reopened, text, 3, &status) == &reopened && status == Ok && utext_nativeLength_nid_postfix(&reopened) == 3, "reopening over a guest table without close");
    UText cloned = provided;
    Require(utext_clone_nid_postfix(&cloned, &open, 0, 0, &status) == &cloned && status == Ok && utext_equals_nid_postfix(&open, &cloned) != 0, "cloning over a guest table without close");
    Require(utext_close_nid_postfix(&provided) == &provided && provided.pFuncs == nullptr, "closing a guest table without close");
    utext_close_nid_postfix(&reopened);
    utext_close_nid_postfix(&cloned);
    status = Ok;
    UText* setup = utext_setup_nid_postfix(nullptr, 16, &status);
    Require(status == Ok && setup != nullptr && setup->pExtra != nullptr && setup->extraSize == 16 && setup->pFuncs == nullptr, "setup allocates a UText with extra space");
    Require(Invalid([&] { utext_next32_nid_postfix(setup); }), "a set-up text without a provider throws");
    Require(Invalid([&] { utext_close_nid_postfix(setup); }), "closing a set-up text without a provider throws");
    Require(Invalid([&] { std::int32_t ok = Ok; utext_setup_nid_postfix(setup, 0, &ok); }), "setting up a set-up text without a provider throws");
    status = Ok;
    UText stack = UTEXT_INITIALIZER;
    Require(utext_setup_nid_postfix(&stack, 0, &status) == &stack && status == Ok && stack.pFuncs == nullptr, "setup opens a stack UText");
    status = BufferOverflow;
    Require(utext_setup_nid_postfix(nullptr, 0, &status) == nullptr && status == BufferOverflow, "a failed status sets nothing up");
    Require(Invalid([] { utext_setup_nid_postfix(nullptr, 0, nullptr); }), "a NULL status throws in setup");
    utext_close_nid_postfix(&open);
    Require(Invalid([&] { utext_next32_nid_postfix(&open); }), "a closed text throws");
    Require(Invalid([&] { utext_getNativeIndex_nid_postfix(&open); }), "a closed text throws in getNativeIndex");
    UText garbage{};
    Require(Invalid([&] { utext_next32_nid_postfix(&garbage); }), "a text without the magic throws");
    Require(utext_close_nid_postfix(&garbage) == &garbage, "closing a text without the magic keeps ICU's result");
    status = Ok;
    Require(utext_setup_nid_postfix(&garbage, 0, &status) == &garbage && status == IllegalArgument, "setup over a text without the magic keeps ICU's status");
    Require(Invalid([] { utext_next32_nid_postfix(nullptr); }), "a NULL text throws in next32");
    Require(Invalid([] { utext_nativeLength_nid_postfix(nullptr); }), "a NULL text throws in nativeLength");
    Require(Invalid([] { utext_setNativeIndex_nid_postfix(nullptr, 0); }), "a NULL text throws in setNativeIndex");
}

std::vector<std::int32_t> GuestBoundaries(UBreakIterator* iterator) {
    std::vector<std::int32_t> boundaries;
    for (std::int32_t position = ubrk_first_nid_postfix(iterator); position != UBRK_DONE; position = ubrk_next_nid_postfix(iterator)) boundaries.push_back(position);
    return boundaries;
}

std::vector<std::int32_t> HostBoundaries(UBreakIterator* iterator) {
    std::vector<std::int32_t> boundaries;
    for (std::int32_t position = ubrk_first(iterator); position != UBRK_DONE; position = ubrk_next(iterator)) boundaries.push_back(position);
    return boundaries;
}

void CheckBreakIterator() {
    const char source[] = "Hello, w\xc3\xb6rld! \xc3\x9cn\xc3\xaf" "code \xf0\x9f\x98\x80 text. Two sentences here.";
    const auto length = static_cast<std::int64_t>(std::strlen(source));
    for (const std::int32_t type : {UBRK_CHARACTER, UBRK_WORD, UBRK_LINE, UBRK_SENTENCE}) {
        std::vector<char> original(source, source + length + 1);
        const char* text = original.data();
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        UBreakIterator* guest = ubrk_open_nid_postfix(type, "en", nullptr, 0, &status);
        UBreakIterator* host = ubrk_open(static_cast<UBreakIteratorType>(type), "en", nullptr, 0, &hostStatus);
        std::int32_t textStatus = Ok;
        UErrorCode hostTextStatus = U_ZERO_ERROR;
        UText* guestText = utext_openUTF8_nid_postfix(nullptr, text, length, &textStatus);
        UText* hostText = utext_openUTF8(nullptr, text, length, &hostTextStatus);
        Require(textStatus == Ok && U_SUCCESS(hostTextStatus), "UTF-8 texts open " + std::to_string(type));
        Require(status <= Ok && guest != nullptr && U_SUCCESS(hostStatus) && host != nullptr, "open " + std::to_string(type));
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        ubrk_setUText_nid_postfix(guest, guestText, &status);
        ubrk_setUText(host, hostText, &hostStatus);
        Require(status == Ok && U_SUCCESS(hostStatus), "setUText " + std::to_string(type));
        const auto boundaries = GuestBoundaries(guest);
        Require(boundaries == HostBoundaries(host) && boundaries.size() > 2u, "UTF-8 boundaries " + std::to_string(type));
        std::vector<char> moved(text, text + length + 1);
        UText* movedText = utext_openUTF8_nid_postfix(nullptr, moved.data(), length, &status);
        const std::int32_t middle = boundaries[boundaries.size() / 2u];
        ubrk_following_nid_postfix(guest, middle - 1);
        const std::int32_t before = ubrk_current_nid_postfix(guest);
        std::memset(original.data(), 0, original.size());
        ubrk_refreshUText_nid_postfix(guest, movedText, &status);
        Require(status == Ok && ubrk_current_nid_postfix(guest) == before && GuestBoundaries(guest) == boundaries, "refreshUText keeps the position " + std::to_string(type));
        ubrk_refreshUText_nid_postfix(guest, nullptr, &status);
        Require(status == IllegalArgument, "refreshing with NULL keeps ICU's status");
        status = BufferOverflow;
        ubrk_setUText_nid_postfix(guest, nullptr, &status);
        ubrk_refreshUText_nid_postfix(guest, nullptr, &status);
        Require(status == BufferOverflow, "a failed status keeps the text");
        status = Ok;
        Require(Invalid([&] { ubrk_setUText_nid_postfix(guest, nullptr, &status); }), "a NULL text throws in setUText");
        Require(Invalid([&] { ubrk_setUText_nid_postfix(guest, guestText, nullptr); }), "a NULL status throws in setUText");
        Require(Invalid([&] { ubrk_refreshUText_nid_postfix(guest, guestText, nullptr); }), "a NULL status throws in refreshUText");
        Require(Invalid([&] { ubrk_setUText_nid_postfix(nullptr, guestText, &status); }), "a NULL iterator throws in setUText");
        Require(Invalid([&] { ubrk_refreshUText_nid_postfix(nullptr, guestText, &status); }), "a NULL iterator throws in refreshUText");
        UText provided = *guestText;
        static const UTextFuncs guestFunctions{};
        provided.pFuncs = &guestFunctions;
        Require(Invalid([&] { ubrk_setUText_nid_postfix(guest, &provided, &status); }), "a guest table without clone throws in setUText");
        Require(Invalid([&] { ubrk_refreshUText_nid_postfix(guest, &provided, &status); }), "a guest table without clone throws in refreshUText");
        Require(provided.pFuncs == &guestFunctions && GuestBoundaries(guest) == boundaries, "a failed setUText keeps the iterator's text");
        ubrk_close_nid_postfix(guest);
        ubrk_close(host);
        utext_close_nid_postfix(movedText);
        utext_close_nid_postfix(guestText);
        utext_close(hostText);
    }
}


constexpr std::int64_t ChunkPoints = 3;
constexpr std::int32_t ExtraBytes = 16;
constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t Unsupported = 16;
constexpr std::int32_t OwnsText = 1 << UTEXT_PROVIDER_OWNS_TEXT;

struct Store {
    std::u32string text;
    bool writable;
    bool deep;
};

template<bool Guest>
int& Closes() {
    static int closes = 0;
    return closes;
}

template<bool Guest>
UText* Setup(UText* ut, std::int32_t extraSpace, std::int32_t* status) {
    if constexpr (Guest) {
        return utext_setup_nid_postfix(ut, extraSpace, status);
    } else {
        return utext_setup(ut, extraSpace, reinterpret_cast<UErrorCode*>(status));
    }
}

template<bool Guest>
std::int64_t IndexOf(const UText* ut) {
    if constexpr (Guest) {
        return utext_getNativeIndex_nid_postfix(ut);
    } else {
        return utext_getNativeIndex(ut);
    }
}

Store& StoreOf(const UText* ut) {
    return *static_cast<Store*>(const_cast<void*>(ut->context));
}

std::int64_t Pin(std::int64_t index, std::int64_t length) {
    return std::max<std::int64_t>(0, std::min(index, length));
}

std::int32_t Units(char32_t c) {
    return c > 0xffff ? 2 : 1;
}

void Load(UText* ut, std::int64_t start) {
    const Store& store = StoreOf(ut);
    const auto limit = std::min<std::int64_t>(start + ChunkPoints, static_cast<std::int64_t>(store.text.size()));
    auto* buffer = static_cast<char16_t*>(ut->pExtra);
    std::int32_t units = 0;
    std::int32_t indexing = -1;
    for (std::int64_t i = start; i < limit; ++i) {
        const char32_t c = store.text[static_cast<std::size_t>(i)];
        if (c > 0xffff) {
            if (indexing < 0) indexing = units;
            buffer[units++] = static_cast<char16_t>(U16_LEAD(c));
            buffer[units++] = static_cast<char16_t>(U16_TRAIL(c));
        } else {
            buffer[units++] = static_cast<char16_t>(c);
        }
    }
    ut->chunkContents = buffer;
    ut->chunkNativeStart = start;
    ut->chunkNativeLimit = limit;
    ut->chunkLength = units;
    ut->nativeIndexingLimit = indexing < 0 ? units : indexing;
}

std::int32_t OffsetOf(const UText* ut, std::int64_t index) {
    const Store& store = StoreOf(ut);
    std::int32_t offset = 0;
    for (std::int64_t i = ut->chunkNativeStart; i < index; ++i) offset += Units(store.text[static_cast<std::size_t>(i)]);
    return offset;
}

bool Access(UText* ut, std::int64_t index, bool forward) {
    const auto length = static_cast<std::int64_t>(StoreOf(ut).text.size());
    index = Pin(index, length);
    if (forward && index >= length) {
        Load(ut, length == 0 ? 0 : (length - 1) / ChunkPoints * ChunkPoints);
        ut->chunkOffset = ut->chunkLength;
        return false;
    }
    if (!forward && index <= 0) {
        Load(ut, 0);
        ut->chunkOffset = 0;
        return false;
    }
    Load(ut, (forward ? index : index - 1) / ChunkPoints * ChunkPoints);
    ut->chunkOffset = OffsetOf(ut, index);
    return true;
}

std::int64_t Length(UText* ut) {
    return static_cast<std::int64_t>(StoreOf(ut).text.size());
}

std::int64_t OffsetToNative(const UText* ut) {
    const Store& store = StoreOf(ut);
    std::int64_t native = ut->chunkNativeStart;
    for (std::int32_t offset = 0; offset < ut->chunkOffset; ++native) offset += Units(store.text[static_cast<std::size_t>(native)]);
    return native;
}

std::int32_t NativeToOffset(const UText* ut, std::int64_t index) {
    return OffsetOf(ut, index);
}

std::int32_t Extract(UText* ut, std::int64_t start, std::int64_t limit, char16_t* dest, std::int32_t capacity, std::int32_t* status) {
    if (*status > Ok) return 0;
    if (capacity < 0 || (dest == nullptr && capacity > 0)) {
        *status = IllegalArgument;
        return 0;
    }
    const Store& store = StoreOf(ut);
    const auto length = static_cast<std::int64_t>(store.text.size());
    start = Pin(start, length);
    limit = Pin(limit, length);
    if (start > limit) {
        *status = IndexOutOfBounds;
        return 0;
    }
    std::int32_t units = 0;
    for (std::int64_t i = start; i < limit; ++i) {
        const char32_t c = store.text[static_cast<std::size_t>(i)];
        if (c > 0xffff) {
            if (units + 1 < capacity) {
                dest[units] = static_cast<char16_t>(U16_LEAD(c));
                dest[units + 1] = static_cast<char16_t>(U16_TRAIL(c));
            }
            units += 2;
        } else {
            if (units < capacity) dest[units] = static_cast<char16_t>(c);
            ++units;
        }
    }
    Access(ut, limit, true);
    if (units < capacity) {
        dest[units] = 0;
    } else if (units == capacity) {
        *status = NotTerminated;
    } else {
        *status = BufferOverflow;
    }
    return units;
}

std::u32string Decode(const char16_t* text, std::int32_t length) {
    std::u32string result;
    for (std::int32_t i = 0; length < 0 ? text[i] != 0 : i < length;) {
        UChar32 c = 0;
        U16_NEXT(text, i, length < 0 ? INT32_MAX : length, c);
        result.push_back(static_cast<char32_t>(c));
    }
    return result;
}

std::int32_t Replace(UText* ut, std::int64_t start, std::int64_t limit, const char16_t* text, std::int32_t length, std::int32_t* status) {
    if (*status > Ok) return 0;
    if ((text == nullptr && length != 0) || length < -1) {
        *status = IllegalArgument;
        return 0;
    }
    Store& store = StoreOf(ut);
    const auto size = static_cast<std::int64_t>(store.text.size());
    start = Pin(start, size);
    limit = Pin(limit, size);
    if (start > limit) {
        *status = IndexOutOfBounds;
        return 0;
    }
    const std::u32string replacement = text == nullptr ? std::u32string() : Decode(text, length);
    store.text.replace(static_cast<std::size_t>(start), static_cast<std::size_t>(limit - start), replacement);
    Access(ut, start + static_cast<std::int64_t>(replacement.size()), true);
    return static_cast<std::int32_t>(static_cast<std::int64_t>(replacement.size()) - (limit - start));
}

void Copy(UText* ut, std::int64_t start, std::int64_t limit, std::int64_t destIndex, bool move, std::int32_t* status) {
    if (*status > Ok) return;
    Store& store = StoreOf(ut);
    const auto size = static_cast<std::int64_t>(store.text.size());
    start = Pin(start, size);
    limit = Pin(limit, size);
    destIndex = Pin(destIndex, size);
    if (start > limit || (destIndex > start && destIndex < limit)) {
        *status = IndexOutOfBounds;
        return;
    }
    const std::u32string piece = store.text.substr(static_cast<std::size_t>(start), static_cast<std::size_t>(limit - start));
    const auto count = static_cast<std::int64_t>(piece.size());
    store.text.insert(static_cast<std::size_t>(destIndex), piece);
    std::int64_t position = destIndex + count;
    if (move) {
        if (destIndex <= start) {
            store.text.erase(static_cast<std::size_t>(start + count), static_cast<std::size_t>(count));
        } else {
            store.text.erase(static_cast<std::size_t>(start), static_cast<std::size_t>(count));
            position = destIndex;
        }
    }
    Access(ut, position, true);
}

template<bool Guest>
UText* Clone(UText* dest, const UText* src, bool deep, std::int32_t* status) {
    if (*status > Ok) return nullptr;
    const Store& store = StoreOf(src);
    if (deep && !store.deep) {
        *status = Unsupported;
        return nullptr;
    }
    const std::int64_t index = IndexOf<Guest>(src);
    UText* result = Setup<Guest>(dest, ExtraBytes, status);
    if (*status > Ok) return result;
    result->context = deep ? new Store(store) : &store;
    result->providerProperties = deep ? src->providerProperties | OwnsText : src->providerProperties & ~OwnsText;
    result->pFuncs = src->pFuncs;
    Access(result, index, true);
    return result;
}

template<bool Guest>
void Close(UText* ut) {
    ++Closes<Guest>();
    if (ut->providerProperties & OwnsText) delete &StoreOf(ut);
}

std::int8_t APS5_VABI ProvidedAccess_nid_no_patch(UText* ut, std::int64_t index, std::int8_t forward);

void Watch(const UText* ut) {
    Require(reinterpret_cast<const GuestTextFuncs*>(ut->pFuncs)->access == ProvidedAccess_nid_no_patch, "a provider callback sees the guest table");
}

UText* APS5_VABI ProvidedClone_nid_no_patch(UText* dest, const UText* src, std::int8_t deep, std::int32_t* status) {
    Watch(src);
    return Clone<true>(dest, src, deep != 0, status);
}

std::int64_t APS5_VABI ProvidedLength_nid_no_patch(UText* ut) {
    Watch(ut);
    return Length(ut);
}

std::int8_t APS5_VABI ProvidedAccess_nid_no_patch(UText* ut, std::int64_t index, std::int8_t forward) {
    Watch(ut);
    return Access(ut, index, forward != 0);
}

std::int32_t APS5_VABI ProvidedExtract_nid_no_patch(UText* ut, std::int64_t start, std::int64_t limit, char16_t* dest, std::int32_t capacity, std::int32_t* status) {
    Watch(ut);
    return Extract(ut, start, limit, dest, capacity, status);
}

std::int32_t APS5_VABI ProvidedReplace_nid_no_patch(UText* ut, std::int64_t start, std::int64_t limit, const char16_t* text, std::int32_t length, std::int32_t* status) {
    Watch(ut);
    return Replace(ut, start, limit, text, length, status);
}

void APS5_VABI ProvidedCopy_nid_no_patch(UText* ut, std::int64_t start, std::int64_t limit, std::int64_t destIndex, std::int8_t move, std::int32_t* status) {
    Watch(ut);
    Copy(ut, start, limit, destIndex, move != 0, status);
}

std::int64_t APS5_VABI ProvidedOffsetToNative_nid_no_patch(const UText* ut) {
    Watch(ut);
    return OffsetToNative(ut);
}

std::int32_t APS5_VABI ProvidedNativeToOffset_nid_no_patch(const UText* ut, std::int64_t index) {
    Watch(ut);
    return NativeToOffset(ut, index);
}

void APS5_VABI ProvidedClose_nid_no_patch(UText* ut) {
    Watch(ut);
    Close<true>(ut);
}

UText* U_CALLCONV HostClone(UText* dest, const UText* src, UBool deep, UErrorCode* status) {
    return Clone<false>(dest, src, deep != 0, reinterpret_cast<std::int32_t*>(status));
}

int64_t U_CALLCONV HostLength(UText* ut) {
    return Length(ut);
}

UBool U_CALLCONV HostAccess(UText* ut, int64_t index, UBool forward) {
    return Access(ut, index, forward != 0);
}

int32_t U_CALLCONV HostExtract(UText* ut, int64_t start, int64_t limit, UChar* dest, int32_t capacity, UErrorCode* status) {
    return Extract(ut, start, limit, dest, capacity, reinterpret_cast<std::int32_t*>(status));
}

int32_t U_CALLCONV HostReplace(UText* ut, int64_t start, int64_t limit, const UChar* text, int32_t length, UErrorCode* status) {
    return Replace(ut, start, limit, text, length, reinterpret_cast<std::int32_t*>(status));
}

void U_CALLCONV HostCopy(UText* ut, int64_t start, int64_t limit, int64_t destIndex, UBool move, UErrorCode* status) {
    Copy(ut, start, limit, destIndex, move != 0, reinterpret_cast<std::int32_t*>(status));
}

int64_t U_CALLCONV HostOffsetToNative(const UText* ut) {
    return OffsetToNative(ut);
}

int32_t U_CALLCONV HostNativeToOffset(const UText* ut, int64_t index) {
    return NativeToOffset(ut, index);
}

void U_CALLCONV HostClose(UText* ut) {
    Close<false>(ut);
}

const GuestTextFuncs guestTable{sizeof(GuestTextFuncs), 0, 0, 0, ProvidedClone_nid_no_patch, ProvidedLength_nid_no_patch, ProvidedAccess_nid_no_patch, ProvidedExtract_nid_no_patch, ProvidedReplace_nid_no_patch, ProvidedCopy_nid_no_patch, ProvidedOffsetToNative_nid_no_patch, ProvidedNativeToOffset_nid_no_patch, ProvidedClose_nid_no_patch, nullptr, nullptr, nullptr};
const GuestTextFuncs otherGuestTable = guestTable;
const UTextFuncs hostTable{sizeof(UTextFuncs), 0, 0, 0, HostClone, HostLength, HostAccess, HostExtract, HostReplace, HostCopy, HostOffsetToNative, HostNativeToOffset, HostClose, nullptr, nullptr, nullptr};

const UTextFuncs* Table(const GuestTextFuncs& table) {
    return reinterpret_cast<const UTextFuncs*>(&table);
}

template<bool Guest>
UText* OpenStore(UText* ut, Store* store, std::int32_t* status) {
    ut = Setup<Guest>(ut, ExtraBytes, status);
    if (*status > Ok) return ut;
    ut->context = store;
    ut->providerProperties = (1 << UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE) | (store->writable ? 1 << UTEXT_PROVIDER_WRITABLE : 0);
    ut->pFuncs = Guest ? Table(guestTable) : &hostTable;
    return ut;
}

const std::u32string& Points() {
    static const std::u32string text{U'a', 0x1f600, U'b', U'c', 0xdc00, 0xd800, 0x10400, 0x10401, 0xe9, U'z', 0xd83d};
    return text;
}

void Same(const UText* guest, const UText* host, const std::string& what) {
    Require(guest->pFuncs != nullptr && guest->pFuncs != &hostTable && (guest->pFuncs == Table(guestTable) || guest->pFuncs == Table(otherGuestTable)), what + ": the guest text keeps its own table");
    Require(guest->privP == nullptr && guest->magic == UTEXT_MAGIC, what + ": the framework fields stay as ICU leaves them");
    Require(guest->chunkNativeStart == host->chunkNativeStart && guest->chunkNativeLimit == host->chunkNativeLimit && guest->chunkOffset == host->chunkOffset, what + ": chunk position");
    Require(guest->chunkLength == host->chunkLength && guest->nativeIndexingLimit == host->nativeIndexingLimit && guest->providerProperties == host->providerProperties && guest->flags == host->flags, what + ": chunk shape");
    Require(guest->chunkLength == 0 || std::memcmp(guest->chunkContents, host->chunkContents, static_cast<std::size_t>(guest->chunkLength) * sizeof(char16_t)) == 0, what + ": chunk contents");
    Require(StoreOf(guest).text == StoreOf(host).text, what + ": provider text");
}

void CompareProvided(UText* guest, UText* host, const std::string& what) {
    const auto length = utext_nativeLength_nid_postfix(guest);
    Require(length == utext_nativeLength(host), what + ": native length");
    Same(guest, host, what + " nativeLength");
    for (std::int64_t index = -2; index <= length + 2; ++index) {
        const std::string at = what + " at " + std::to_string(index);
        Require(utext_char32At_nid_postfix(guest, index) == utext_char32At(host, index), at + ": char32At");
        Same(guest, host, at + " char32At");
        Require(utext_getNativeIndex_nid_postfix(guest) == utext_getNativeIndex(host), at + ": index after char32At");
        Require(utext_next32From_nid_postfix(guest, index) == utext_next32From(host, index), at + ": next32From");
        Same(guest, host, at + " next32From");
        Require(utext_getNativeIndex_nid_postfix(guest) == utext_getNativeIndex(host), at + ": index after next32From");
        Require(utext_previous32From_nid_postfix(guest, index) == utext_previous32From(host, index), at + ": previous32From");
        Same(guest, host, at + " previous32From");
        Require(utext_getNativeIndex_nid_postfix(guest) == utext_getNativeIndex(host), at + ": index after previous32From");
        utext_setNativeIndex_nid_postfix(guest, index);
        utext_setNativeIndex(host, index);
        Same(guest, host, at + " setNativeIndex");
        Require(utext_getNativeIndex_nid_postfix(guest) == utext_getNativeIndex(host), at + ": setNativeIndex");
        Require(utext_current32_nid_postfix(guest) == utext_current32(host), at + ": current32");
        Same(guest, host, at + " current32");
        Require(utext_getPreviousNativeIndex_nid_postfix(guest) == utext_getPreviousNativeIndex(host), at + ": getPreviousNativeIndex");
        Same(guest, host, at + " getPreviousNativeIndex");
        for (const std::int32_t delta : {-4, -1, 0, 1, 3}) {
            utext_setNativeIndex_nid_postfix(guest, index);
            utext_setNativeIndex(host, index);
            const bool moved = utext_moveIndex32_nid_postfix(guest, delta) != 0;
            Require(moved == (utext_moveIndex32(host, delta) != 0) && utext_getNativeIndex_nid_postfix(guest) == utext_getNativeIndex(host), at + ": moveIndex32 " + std::to_string(delta));
            Same(guest, host, at + " moveIndex32 " + std::to_string(delta));
        }
    }
    utext_setNativeIndex_nid_postfix(guest, 0);
    utext_setNativeIndex(host, 0);
    for (;;) {
        const auto character = utext_next32_nid_postfix(guest);
        Require(character == utext_next32(host) && utext_getNativeIndex_nid_postfix(guest) == utext_getNativeIndex(host), what + ": forward walk");
        Same(guest, host, what + " forward walk");
        if (character == U_SENTINEL) break;
    }
    for (;;) {
        const auto character = utext_previous32_nid_postfix(guest);
        Require(character == utext_previous32(host) && utext_getNativeIndex_nid_postfix(guest) == utext_getNativeIndex(host), what + ": backward walk");
        Same(guest, host, what + " backward walk");
        if (character == U_SENTINEL) break;
    }
    Require(utext_isLengthExpensive_nid_postfix(guest) == utext_isLengthExpensive(host) && utext_isLengthExpensive(host) != 0, what + ": isLengthExpensive");
    Require(utext_isWritable_nid_postfix(guest) == utext_isWritable(host) && utext_hasMetaData_nid_postfix(guest) == utext_hasMetaData(host), what + ": properties");
}

void CompareExtract(UText* guest, UText* host, const std::string& what) {
    const auto length = utext_nativeLength(host);
    for (std::int64_t start = -1; start <= length + 1; ++start) {
        for (std::int64_t limit = -1; limit <= length + 1; ++limit) {
            for (const std::int32_t capacity : {0, 3, 32}) {
                char16_t text[32];
                char16_t hostText[32];
                std::fill(std::begin(text), std::end(text), u'#');
                std::fill(std::begin(hostText), std::end(hostText), u'#');
                std::int32_t status = Ok;
                UErrorCode hostStatus = U_ZERO_ERROR;
                const auto units = utext_extract_nid_postfix(guest, start, limit, capacity ? text : nullptr, capacity, &status);
                const auto hostUnits = utext_extract(host, start, limit, capacity ? hostText : nullptr, capacity, &hostStatus);
                const std::string at = what + " extract " + std::to_string(start) + "-" + std::to_string(limit) + " into " + std::to_string(capacity);
                Require(units == hostUnits && status == hostStatus && std::memcmp(text, hostText, sizeof(text)) == 0, at);
                Same(guest, host, at);
            }
        }
    }
    std::int32_t status = BufferOverflow;
    char16_t text[4];
    Require(utext_extract_nid_postfix(guest, 0, 2, text, 4, &status) == 0 && status == BufferOverflow, what + ": a failed status extracts nothing");
    status = Ok;
    Require(utext_extract_nid_postfix(guest, 0, 2, nullptr, 4, &status) == 0 && status == IllegalArgument, what + ": the provider sees the NULL buffer");
    Same(guest, host, what + " after the failed extracts");
}

void CheckProvider() {
    Store guestStore{Points(), false, true};
    Store hostStore = guestStore;
    std::int32_t status = Ok;
    std::int32_t hostStatus = Ok;
    UText* guest = OpenStore<true>(nullptr, &guestStore, &status);
    UText* host = OpenStore<false>(nullptr, &hostStore, &hostStatus);
    Require(status == Ok && hostStatus == Ok && guest != nullptr && host != nullptr, "provided texts open on the heap");
    CompareProvided(guest, host, "provided heap text");
    CompareExtract(guest, host, "provided heap text");

    UText guestStack = UTEXT_INITIALIZER;
    UText hostStack = UTEXT_INITIALIZER;
    OpenStore<true>(&guestStack, &guestStore, &status);
    OpenStore<false>(&hostStack, &hostStore, &hostStatus);
    CompareProvided(&guestStack, &hostStack, "provided stack text");

    utext_setNativeIndex_nid_postfix(guest, 7);
    utext_setNativeIndex(host, 7);
    UErrorCode error = U_ZERO_ERROR;
    UText* shallow = utext_clone_nid_postfix(nullptr, guest, 0, 0, &status);
    UText* hostShallow = utext_clone(nullptr, host, 0, 0, &error);
    Require(status == Ok && U_SUCCESS(error) && shallow != nullptr && shallow != guest && shallow->context == &guestStore, "a shallow clone of a provided text shares its store");
    Same(shallow, hostShallow, "shallow clone");
    Require(utext_equals_nid_postfix(guest, shallow) != 0 && utext_equals(host, hostShallow) != 0, "a shallow clone equals its source");
    Same(guest, host, "equals source");
    Same(shallow, hostShallow, "equals clone");
    utext_next32_nid_postfix(shallow);
    utext_next32(hostShallow);
    Require(utext_equals_nid_postfix(guest, shallow) == 0 && utext_equals(host, hostShallow) == 0, "a clone at another index differs");
    Require(utext_equals_nid_postfix(guest, guest) != 0, "a provided text equals itself");
    Same(guest, host, "equals itself");
    UText other = *shallow;
    other.pFuncs = Table(otherGuestTable);
    utext_setNativeIndex_nid_postfix(&other, 7);
    Require(utext_equals_nid_postfix(guest, &other) == 0, "texts of two guest tables are not equal");
    Require(other.pFuncs == Table(otherGuestTable) && guest->pFuncs == Table(guestTable), "each text keeps its own table");
    UText chars = UTEXT_INITIALIZER;
    utext_openUChars_nid_postfix(&chars, u"abc", 3, &status);
    Require(utext_equals_nid_postfix(guest, &chars) == 0, "a provided text never equals an ICU text");
    CompareProvided(shallow, hostShallow, "shallow clone");

    UText* deep = utext_clone_nid_postfix(nullptr, guest, 1, 1, &status);
    UText* hostDeep = utext_clone(nullptr, host, 1, 1, &error);
    Require(status == Ok && U_SUCCESS(error) && deep != nullptr && deep->context != &guestStore && (deep->providerProperties & OwnsText) != 0, "a deep clone owns a copy of the store");
    Same(deep, hostDeep, "deep read-only clone");
    Require(utext_equals_nid_postfix(guest, deep) == 0, "a deep clone does not share the source");
    CompareProvided(deep, hostDeep, "deep clone");

    guestStore.deep = false;
    hostStore.deep = false;
    error = U_ZERO_ERROR;
    Require(utext_clone_nid_postfix(nullptr, guest, 1, 0, &status) == nullptr && status == Unsupported, "an unsupported deep clone keeps the provider's status");
    Require(utext_clone(nullptr, host, 1, 0, &error) == nullptr && error == U_UNSUPPORTED_ERROR, "host ICU reports the same unsupported clone");
    Same(guest, host, "after an unsupported clone");
    guestStore.deep = true;
    hostStore.deep = true;
    status = Ok;
    error = U_ZERO_ERROR;

    const int closes = Closes<true>();
    const int hostCloses = Closes<false>();
    const auto counted = [&](int expected) { return Closes<true>() == closes + expected && Closes<false>() == hostCloses + expected; };
    Require(utext_clone_nid_postfix(&guestStack, deep, 0, 0, &status) == &guestStack && utext_clone(&hostStack, hostDeep, 0, 0, &error) == &hostStack, "a clone into an open provided text reuses it");
    Require(counted(1), "a clone closes the provided text it replaces");
    Same(&guestStack, &hostStack, "clone into an open provided text");
    UText hostChars = UTEXT_INITIALIZER;
    utext_openUChars(&hostChars, u"abc", 3, &error);
    Require(utext_clone_nid_postfix(&guestStack, &chars, 0, 0, &status) == &guestStack && utext_clone(&hostStack, &hostChars, 0, 0, &error) == &hostStack && status == Ok, "an ICU clone replaces a provided text");
    Require(counted(2) && utext_nativeLength_nid_postfix(&guestStack) == 3, "an ICU clone closes the provided text it replaces");
    OpenStore<true>(&guestStack, &guestStore, &status);
    OpenStore<false>(&hostStack, &hostStore, &hostStatus);
    Require(counted(2), "opening over an ICU text closes no provider");
    Require(utext_openUChars_nid_postfix(&guestStack, u"xy", 2, &status) == &guestStack && utext_openUChars(&hostStack, u"xy", 2, &error) == &hostStack && status == Ok && utext_nativeLength_nid_postfix(&guestStack) == 2, "reopening a provided text as UChars");
    Require(counted(3), "reopening closes the provided text");
    OpenStore<true>(&guestStack, &guestStore, &status);
    OpenStore<false>(&hostStack, &hostStore, &hostStatus);
    Require(utext_setup_nid_postfix(&guestStack, 4, &status) == &guestStack && utext_setup(&hostStack, 4, &error) == &hostStack && status == Ok && U_SUCCESS(error), "setup over a provided text");
    Require(counted(4), "setup closes the provided text");
    Require(guestStack.pFuncs == Table(guestTable) && hostStack.pFuncs == &hostTable && guestStack.context == nullptr && guestStack.privP == nullptr, "setup keeps the old table as ICU does");
    guestStack.context = &guestStore;
    hostStack.context = &hostStore;
    Require(utext_close_nid_postfix(&guestStack) == &guestStack && utext_close(&hostStack) == &hostStack, "closing a stack provided text returns it");
    Require(counted(5) && guestStack.pFuncs == nullptr && hostStack.pFuncs == nullptr, "closing calls the provider once and clears the table");
    Require(utext_close_nid_postfix(&guestStack) == &guestStack && counted(5), "closing a closed provided text calls nothing");

    Require(utext_close_nid_postfix(deep) == nullptr && utext_close(hostDeep) == nullptr, "closing a heap deep clone frees it");
    Require(utext_close_nid_postfix(shallow) == nullptr && utext_close(hostShallow) == nullptr, "closing a heap shallow clone frees it");
    Require(utext_close_nid_postfix(guest) == nullptr && utext_close(host) == nullptr, "closing a heap provided text frees it");
    Require(counted(8), "each heap text closes once");
    utext_close_nid_postfix(&chars);
    utext_close(&hostChars);
}

void CheckProvidedWrite() {
    Store guestStore{Points(), true, true};
    Store hostStore = guestStore;
    std::int32_t status = Ok;
    std::int32_t hostStatus = Ok;
    UText guest = UTEXT_INITIALIZER;
    UText host = UTEXT_INITIALIZER;
    OpenStore<true>(&guest, &guestStore, &status);
    OpenStore<false>(&host, &hostStore, &hostStatus);
    Require(utext_isWritable_nid_postfix(&guest) != 0 && utext_isWritable(&host) != 0, "a writable provider");
    struct Edit {
        std::int64_t start;
        std::int64_t limit;
        const char16_t* text;
        std::int32_t length;
    };
    const char16_t supplementary[] = {0xd83d, 0xde01, u'q', 0};
    for (const Edit& edit : {Edit{1, 3, u"XY", 2}, Edit{0, 0, supplementary, -1}, Edit{4, 9, nullptr, 0}, Edit{-3, 1, u"long", 4}, Edit{2, 99, supplementary, 2}, Edit{5, 2, u"x", 1}, Edit{0, 1, nullptr, 3}}) {
        status = Ok;
        UErrorCode error = U_ZERO_ERROR;
        const auto delta = utext_replace_nid_postfix(&guest, edit.start, edit.limit, edit.text, edit.length, &status);
        const auto hostDelta = utext_replace(&host, edit.start, edit.limit, edit.text, edit.length, &error);
        const std::string at = "replace " + std::to_string(edit.start) + "-" + std::to_string(edit.limit);
        Require(delta == hostDelta && status == error, at);
        Same(&guest, &host, at);
        CompareProvided(&guest, &host, at);
    }
    struct Move {
        std::int64_t start;
        std::int64_t limit;
        std::int64_t dest;
        std::int8_t move;
    };
    for (const Move& move : {Move{0, 2, 5, 0}, Move{0, 2, 5, 1}, Move{4, 6, 1, 1}, Move{3, 5, 4, 0}, Move{1, 4, 99, 1}, Move{6, 2, 0, 0}}) {
        status = Ok;
        UErrorCode error = U_ZERO_ERROR;
        utext_copy_nid_postfix(&guest, move.start, move.limit, move.dest, move.move, &status);
        utext_copy(&host, move.start, move.limit, move.dest, move.move, &error);
        const std::string at = "copy " + std::to_string(move.start) + "-" + std::to_string(move.limit) + " to " + std::to_string(move.dest) + (move.move ? " moving" : "");
        Require(status == error, at);
        Same(&guest, &host, at);
    }
    CompareExtract(&guest, &host, "edited text");
    status = BufferOverflow;
    Require(utext_replace_nid_postfix(&guest, 0, 1, u"x", 1, &status) == 0 && status == BufferOverflow, "a failed status replaces nothing in a provided text");
    utext_copy_nid_postfix(&guest, 0, 1, 2, 0, &status);
    Require(status == BufferOverflow && StoreOf(&guest).text == StoreOf(&host).text, "a failed status copies nothing in a provided text");
    utext_freeze_nid_postfix(&guest);
    utext_freeze(&host);
    status = Ok;
    UErrorCode error = U_ZERO_ERROR;
    Require(utext_replace_nid_postfix(&guest, 0, 1, u"x", 1, &status) == utext_replace(&host, 0, 1, u"x", 1, &error) && status == NoWritePermission && error == U_NO_WRITE_PERMISSION, "a frozen provided text refuses replace");
    status = Ok;
    utext_copy_nid_postfix(&guest, 0, 1, 2, 0, &status);
    Require(status == NoWritePermission, "a frozen provided text refuses copy");
    Same(&guest, &host, "frozen");

    GuestTextFuncs readOnly = guestTable;
    readOnly.replace = nullptr;
    readOnly.copy = nullptr;
    UText partial = UTEXT_INITIALIZER;
    status = Ok;
    Require(OpenStore<true>(&partial, &guestStore, &status) == &partial && status == Ok, "a second provided text opens");
    partial.pFuncs = Table(readOnly);
    Require(Invalid([&] { utext_replace_nid_postfix(&partial, 0, 1, u"x", 1, &status); }), "a writable provider without replace throws");
    Require(Invalid([&] { utext_copy_nid_postfix(&partial, 0, 1, 2, 0, &status); }), "a writable provider without copy throws");
    Require(partial.pFuncs == Table(readOnly), "a throwing call restores the guest table");
    partial.providerProperties &= ~(1 << UTEXT_PROVIDER_WRITABLE);
    Require(utext_replace_nid_postfix(&partial, 0, 1, u"x", 1, &status) == 0 && status == NoWritePermission, "a read-only provider without replace keeps ICU's status");
    utext_close_nid_postfix(&partial);
    utext_close_nid_postfix(&guest);
    utext_close(&host);
}

void CheckProvidedGuards() {
    Store store{Points(), false, true};
    std::int32_t status = Ok;
    UText text = UTEXT_INITIALIZER;
    OpenStore<true>(&text, &store, &status);
    GuestTextFuncs sparse{};
    sparse.access = guestTable.access;
    sparse.nativeLength = guestTable.nativeLength;
    text.pFuncs = Table(sparse);
    Require(utext_next32_nid_postfix(&text) == U'a' && utext_nativeLength_nid_postfix(&text) == static_cast<std::int64_t>(store.text.size()), "a provider without map functions iterates its 1:1 prefix");
    utext_setNativeIndex_nid_postfix(&text, 1);
    utext_next32_nid_postfix(&text);
    Require(Invalid([&] { utext_getNativeIndex_nid_postfix(&text); }), "a missing mapOffsetToNative throws when ICU calls it");
    Require(Invalid([&] { utext_setNativeIndex_nid_postfix(&text, 2); }), "a missing mapNativeIndexToUTF16 throws when ICU calls it");
    Require(Invalid([&] { char16_t buffer[4]; std::int32_t ok = Ok; utext_extract_nid_postfix(&text, 0, 1, buffer, 4, &ok); }), "a missing extract throws");
    Require(Invalid([&] { std::int32_t ok = Ok; utext_clone_nid_postfix(nullptr, &text, 0, 0, &ok); }), "a missing clone throws");
    Require(text.pFuncs == Table(sparse), "the guest table survives each throw");
    const int closes = Closes<true>();
    Require(utext_close_nid_postfix(&text) == &text && text.pFuncs == nullptr && Closes<true>() == closes, "a provider without close closes");
}

std::vector<std::int32_t> Boundaries(UBreakIterator* iterator, bool guest) {
    std::vector<std::int32_t> boundaries;
    for (std::int32_t position = guest ? ubrk_first_nid_postfix(iterator) : ubrk_first(iterator); position != UBRK_DONE; position = guest ? ubrk_next_nid_postfix(iterator) : ubrk_next(iterator)) boundaries.push_back(position);
    return boundaries;
}

void CheckProvidedBreakIterator() {
    const std::u32string source = U"Hello, wörld! \U0001f600 Ünïcode \U00010400\U00010401 text. Two sentences here.";
    for (const std::int32_t type : {UBRK_CHARACTER, UBRK_WORD, UBRK_LINE, UBRK_SENTENCE}) {
        const std::string what = "provided break iterator " + std::to_string(type);
        Store guestStore{source, false, true};
        Store hostStore = guestStore;
        std::int32_t status = Ok;
        std::int32_t hostStatus = Ok;
        UText* guestText = OpenStore<true>(nullptr, &guestStore, &status);
        UText* hostText = OpenStore<false>(nullptr, &hostStore, &hostStatus);
        UErrorCode error = U_ZERO_ERROR;
        UBreakIterator* guest = ubrk_open_nid_postfix(type, "en", nullptr, 0, &status);
        UBreakIterator* host = ubrk_open(static_cast<UBreakIteratorType>(type), "en", nullptr, 0, &error);
        Require(status <= Ok && U_SUCCESS(error) && guest != nullptr && host != nullptr, what + ": open");
        const int closes = Closes<true>();
        const int hostCloses = Closes<false>();
        status = Ok;
        error = U_ZERO_ERROR;
        ubrk_setUText_nid_postfix(guest, guestText, &status);
        ubrk_setUText(host, hostText, &error);
        Require(status == Ok && U_SUCCESS(error), what + ": setUText");
        Require(guestText->pFuncs == Table(guestTable), what + ": setUText leaves the guest table on the text");
        const auto boundaries = Boundaries(guest, true);
        Require(boundaries == Boundaries(host, false) && boundaries.size() > 2u, what + ": boundaries");
        Require(Closes<true>() == closes && Closes<false>() == hostCloses, what + ": setUText over an ICU text closes no provider");

        UBreakIterator* clone = ubrk_safeClone_nid_postfix(guest, nullptr, nullptr, &status);
        UBreakIterator* hostClone = ubrk_safeClone(host, nullptr, nullptr, &error);
        Require(clone != nullptr && hostClone != nullptr && status == static_cast<std::int32_t>(error) && status <= Ok, what + ": safeClone");
        status = Ok;
        error = U_ZERO_ERROR;
        Require(Boundaries(clone, true) == boundaries && Boundaries(hostClone, false) == boundaries, what + ": a clone keeps the provided text");

        Store movedStore = guestStore;
        Store hostMovedStore = hostStore;
        UText* moved = OpenStore<true>(nullptr, &movedStore, &status);
        UText* hostMoved = OpenStore<false>(nullptr, &hostMovedStore, &hostStatus);
        const std::int32_t middle = boundaries[boundaries.size() / 2u];
        ubrk_following_nid_postfix(guest, middle - 1);
        ubrk_following(host, middle - 1);
        const std::int32_t before = ubrk_current_nid_postfix(guest);
        ubrk_refreshUText_nid_postfix(guest, moved, &status);
        ubrk_refreshUText(host, hostMoved, &error);
        Require(status == Ok && U_SUCCESS(error) && ubrk_current_nid_postfix(guest) == before && ubrk_current(host) == before, what + ": refreshUText keeps the position");
        guestStore.text.assign(guestStore.text.size(), U'#');
        hostStore.text.assign(hostStore.text.size(), U'#');
        Require(Boundaries(guest, true) == boundaries && moved->pFuncs == Table(guestTable), what + ": refreshUText reads the new text");
        Require(Closes<true>() == closes + 1 && Closes<false>() == hostCloses + 1, what + ": refreshUText closes the old clone");

        ubrk_setText_nid_postfix(clone, u"two words", 9, &status);
        ubrk_setText(hostClone, u"two words", 9, &error);
        Require(Closes<true>() == closes + 2 && Closes<false>() == hostCloses + 2, what + ": setText closes the provided clone");
        ubrk_close_nid_postfix(clone);
        ubrk_close(hostClone);
        ubrk_close_nid_postfix(guest);
        ubrk_close(host);
        Require(Closes<true>() == closes + 3 && Closes<false>() == hostCloses + 3, what + ": closing the iterator closes its provided text");
        utext_close_nid_postfix(moved);
        utext_close(hostMoved);
        utext_close_nid_postfix(guestText);
        utext_close(hostText);
    }
}
}

int main() {
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    CheckOpen();
    CheckExtract();
    CheckClone();
    CheckWrite();
    CheckGuards();
    CheckBreakIterator();
    CheckProvider();
    CheckProvidedWrite();
    CheckProvidedGuards();
    CheckProvidedBreakIterator();
    std::puts("guest_icu_text passed");
    return 0;
}
