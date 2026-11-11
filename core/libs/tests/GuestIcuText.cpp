#include "prx/libSceIcu/BreakIterator.hpp"
#include "prx/libSceIcu/Text.hpp"
#include <unicode/ubrk.h>
#include <unicode/udata.h>
#include <unicode/utext.h>
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

template<typename TAction>
bool NotImplemented(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::runtime_error&) {
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
    Require(NotImplemented([&] { utext_next32_nid_postfix(&provided); }), "a guest provider throws in next32");
    Require(NotImplemented([&] { utext_nativeLength_nid_postfix(&provided); }), "a guest provider throws in nativeLength");
    Require(NotImplemented([&] { utext_close_nid_postfix(&provided); }), "a guest provider throws in close");
    Require(NotImplemented([&] { std::int32_t ok = Ok; utext_openUChars_nid_postfix(&provided, text, 3, &ok); }), "reopening over a guest provider throws");
    Require(NotImplemented([&] { std::int32_t ok = Ok; utext_clone_nid_postfix(&provided, &open, 0, 0, &ok); }), "cloning over a guest provider throws");
    Require(NotImplemented([&] { std::int32_t ok = Ok; utext_clone_nid_postfix(nullptr, &provided, 0, 0, &ok); }), "cloning a guest provider throws");
    Require(utext_equals_nid_postfix(&open, &provided) == 0, "texts of different providers are not equal");
    status = Ok;
    UText* setup = utext_setup_nid_postfix(nullptr, 16, &status);
    Require(status == Ok && setup != nullptr && setup->pExtra != nullptr && setup->extraSize == 16 && setup->pFuncs == nullptr, "setup allocates a UText with extra space");
    Require(NotImplemented([&] { utext_next32_nid_postfix(setup); }), "a set-up text without a provider throws");
    Require(NotImplemented([&] { utext_close_nid_postfix(setup); }), "closing a set-up text without a provider throws");
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
    const char source[] = "Hello, w\xc3\xb6rld! \xc3\x9cn\xc3\xafcode \xf0\x9f\x98\x80 text. Two sentences here.";
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
        Require(NotImplemented([&] { ubrk_setUText_nid_postfix(guest, &provided, &status); }), "a guest provider throws in setUText");
        Require(NotImplemented([&] { ubrk_refreshUText_nid_postfix(guest, &provided, &status); }), "a guest provider throws in refreshUText");
        ubrk_close_nid_postfix(guest);
        ubrk_close(host);
        utext_close_nid_postfix(movedText);
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
    std::puts("guest_icu_text passed");
    return 0;
}
