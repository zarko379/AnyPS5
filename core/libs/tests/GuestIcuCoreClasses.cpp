#include "prx/libSceIcu/CoreClasses.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include <unicode/stringpiece.h>
#include <unicode/udata.h>
#include "patternprops.h"
#include "umutex.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU CORE CLASSES: %s\n", message);
    std::abort();
}

template<typename TAction>
bool Rejects(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

struct Heap {
    std::set<void*> live;
    int allocations = 0;
    int releases = 0;
};

Heap heap;

void* APS5_VABI Allocate_nid_no_patch(const void*, std::size_t size) {
    ++heap.allocations;
    void* memory = std::malloc(size ? size : 1);
    heap.live.insert(memory);
    return memory;
}

void* APS5_VABI Reallocate_nid_no_patch(const void*, void* memory, std::size_t size) {
    heap.live.erase(memory);
    void* result = std::realloc(memory, size ? size : 1);
    heap.live.insert(result);
    return result;
}

void APS5_VABI Release_nid_no_patch(const void*, void* memory) {
    Require(heap.live.erase(memory) == 1, "the library releases only memory it allocated");
    ++heap.releases;
    std::free(memory);
}

void CheckMemory() {
    const int before = heap.allocations;
    void* object = _ZN6icu_687UMemorynwEm_nid_postfix(24);
    void* array = _ZN6icu_687UMemorynaEm_nid_postfix(40);
    Require(object && array && heap.allocations == before + 2 && heap.live.count(object) && heap.live.count(array),
            "UMemory new and new[] allocate through uprv_malloc");
    const int released = heap.releases;
    _ZN6icu_687UMemorydlEPv_nid_postfix(nullptr);
    _ZN6icu_687UMemorydaEPv_nid_postfix(nullptr);
    Require(heap.releases == released, "UMemory delete of null releases nothing");
    _ZN6icu_687UMemorydlEPv_nid_postfix(object);
    _ZN6icu_687UMemorydaEPv_nid_postfix(array);
    Require(heap.releases == released + 2 && !heap.live.count(object) && !heap.live.count(array), "UMemory delete and delete[] release through uprv_free");
    void* zero = _ZN6icu_687UMemorynwEm_nid_postfix(0);
    Require(zero != nullptr && heap.allocations == before + 2, "a zero-size UMemory new returns ICU's shared zero allocation");
    _ZN6icu_687UMemorydlEPv_nid_postfix(zero);
    Require(heap.releases == released + 2, "deleting the shared zero allocation releases nothing");

    void* uobject = _ZN6icu_687UMemorynwEm_nid_postfix(8);
    std::memset(uobject, 0x3C, 8);
    _ZN6icu_687UObjectD1Ev_nid_postfix(uobject);
    _ZN6icu_687UObjectD2Ev_nid_postfix(uobject);
    Require(heap.live.count(uobject) && static_cast<unsigned char*>(uobject)[0] == 0x3C, "the complete and base destructors free nothing");
    Require(_ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix(uobject) == nullptr, "UObject has no dynamic class ID");
    _ZN6icu_687UObjectD0Ev_nid_postfix(uobject);
    Require(!heap.live.count(uobject), "the deleting destructor frees the object");
    Require(Rejects([] { _ZN6icu_687UObjectD0Ev_nid_postfix(nullptr); }), "the deleting destructor rejects a null object");
    Require(Rejects([] { _ZN6icu_687UObjectD1Ev_nid_postfix(nullptr); }), "the complete destructor rejects a null object");
    Require(Rejects([] { _ZN6icu_687UObjectD2Ev_nid_postfix(nullptr); }), "the base destructor rejects a null object");
    Require(Rejects([] { _ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix(nullptr); }), "getDynamicClassID rejects a null object");
}

Icu::StringPiece Guest(const icu::StringPiece& piece) {
    return Icu::StringPiece{piece.data(), piece.length()};
}

bool Same(const Icu::StringPiece& guest, const icu::StringPiece& host) {
    return guest.data == host.data() && guest.length == host.length();
}

void CheckStringPieceConstructors() {
    const char* texts[] = {"", "a", "hello", "hello world", "abcabcabd"};
    for (const char* text : texts) {
        Icu::StringPiece guest{reinterpret_cast<const char*>(1), 99};
        _ZN6icu_6811StringPieceC1EPKc_nid_postfix(&guest, text);
        Require(Same(guest, icu::StringPiece(text)), "the C string constructor matches ICU");
        guest = {nullptr, 7};
        _ZN6icu_6811StringPieceC2EPKc_nid_postfix(&guest, text);
        Require(Same(guest, icu::StringPiece(text)), "the base C string constructor matches ICU");
        guest = {nullptr, 7};
        _ZN6icu_6811StringPiece3setEPKc_nid_postfix(&guest, text);
        Require(Same(guest, icu::StringPiece(text)), "set matches ICU");
        const icu::StringPiece host(text);
        const Icu::StringPiece source = Guest(host);
        for (std::int32_t position = -3; position <= host.length() + 3; ++position) {
            Icu::StringPiece suffix{};
            _ZN6icu_6811StringPieceC1ERKS0_i_nid_postfix(&suffix, &source, position);
            Require(Same(suffix, icu::StringPiece(host, position)), "the suffix constructor clamps like ICU");
            _ZN6icu_6811StringPieceC2ERKS0_i_nid_postfix(&suffix, &source, position);
            Require(Same(suffix, icu::StringPiece(host, position)), "the base suffix constructor clamps like ICU");
            for (std::int32_t length = -2; length <= host.length() + 2; ++length) {
                Icu::StringPiece range{};
                _ZN6icu_6811StringPieceC1ERKS0_ii_nid_postfix(&range, &source, position, length);
                Require(Same(range, icu::StringPiece(host, position, length)), "the range constructor clamps like ICU");
                _ZN6icu_6811StringPieceC2ERKS0_ii_nid_postfix(&range, &source, position, length);
                Require(Same(range, icu::StringPiece(host, position, length)), "the base range constructor clamps like ICU");
            }
        }
    }
    Icu::StringPiece guest{"x", 1};
    _ZN6icu_6811StringPieceC1EPKc_nid_postfix(&guest, nullptr);
    Require(guest.data == nullptr && guest.length == 0, "a null C string makes an empty piece");
    guest = {"x", 1};
    _ZN6icu_6811StringPiece3setEPKc_nid_postfix(&guest, nullptr);
    Require(guest.data == nullptr && guest.length == 0, "set to null empties the piece");
    Require(Rejects([] { _ZN6icu_6811StringPieceC1EPKc_nid_postfix(nullptr, "a"); }), "the constructor rejects a null object");
    Require(Rejects([] { _ZN6icu_6811StringPiece3setEPKc_nid_postfix(nullptr, "a"); }), "set rejects a null object");
    Require(Rejects([&] { _ZN6icu_6811StringPieceC1ERKS0_i_nid_postfix(&guest, nullptr, 0); }), "the suffix constructor rejects a null source");
    Require(Rejects([&] { _ZN6icu_6811StringPieceC2ERKS0_ii_nid_postfix(&guest, nullptr, 0, 0); }), "the range constructor rejects a null source");
    Require(Rejects([&] { _ZN6icu_6811StringPieceC1ERKS0_ii_nid_postfix(nullptr, &guest, 0, 0); }), "the range constructor rejects a null object");
}

void CheckStringPieceSearch() {
    const char buffer[] = "abcabcabdxyzaab";
    std::mt19937 random(5);
    for (int round = 0; round < 20000; ++round) {
        const auto start = static_cast<std::int32_t>(random() % 15);
        const auto length = static_cast<std::int32_t>(random() % (16 - start));
        const auto needleStart = static_cast<std::int32_t>(random() % 15);
        const auto needleLength = static_cast<std::int32_t>(random() % (16 - needleStart)) % 5;
        const auto offset = static_cast<std::int32_t>(random() % 18) - 1;
        icu::StringPiece host(buffer + start, length);
        const icu::StringPiece hostNeedle(buffer + needleStart, needleLength);
        Icu::StringPiece guest = Guest(host);
        Require(_ZN6icu_6811StringPiece4findES0_i_nid_postfix(&guest, Guest(hostNeedle), offset) == host.find(hostNeedle, offset),
                "find matches ICU 68.2, including matches that run past the piece");
        Require(_ZN6icu_6811StringPiece7compareES0__nid_postfix(&guest, Guest(hostNeedle)) == host.compare(hostNeedle), "compare matches ICU");
        const Icu::StringPiece guestNeedle = Guest(hostNeedle);
        Require(_ZN6icu_68eqERKNS_11StringPieceES2__nid_postfix(&guest, &guestNeedle) == (host == hostNeedle), "operator== matches ICU");
    }
    Icu::StringPiece prefix{"abc", 2};
    Require(_ZN6icu_6811StringPiece4findES0_i_nid_postfix(&prefix, Icu::StringPiece{"bc", 2}, 0) == 1, "find matches the bytes after the piece");
    Icu::StringPiece empty{nullptr, 0};
    Require(_ZN6icu_6811StringPiece4findES0_i_nid_postfix(&empty, Icu::StringPiece{nullptr, 0}, 5) == 0, "two empty pieces find at 0");
    Require(_ZN6icu_6811StringPiece4findES0_i_nid_postfix(&empty, Icu::StringPiece{"a", 1}, 0) == -1, "nothing is found in an empty piece");
    Icu::StringPiece hello{"hello", 5};
    Require(_ZN6icu_6811StringPiece4findES0_i_nid_postfix(&hello, Icu::StringPiece{nullptr, 0}, 2) == 2, "an empty needle is found at the offset");
    Require(_ZN6icu_6811StringPiece4findES0_i_nid_postfix(&hello, Icu::StringPiece{nullptr, 0}, 9) == -1, "an offset past the piece finds nothing");
    Require(_ZN6icu_6811StringPiece7compareES0__nid_postfix(&empty, Icu::StringPiece{nullptr, 0}) == 0, "empty pieces compare equal");
    Require(_ZN6icu_6811StringPiece7compareES0__nid_postfix(&hello, Icu::StringPiece{nullptr, 0}) == 1, "a piece is greater than an empty one");
    Icu::StringPiece high{"\xE9", 1};
    Require(_ZN6icu_6811StringPiece7compareES0__nid_postfix(&high, Icu::StringPiece{"a", 1}) == -1, "compare uses signed char like ICU on x86-64");
    const Icu::StringPiece nullEmpty{nullptr, 0};
    Require(_ZN6icu_68eqERKNS_11StringPieceES2__nid_postfix(&empty, &nullEmpty) == 1, "empty pieces are equal without reading");
    Require(Rejects([] { _ZN6icu_6811StringPiece4findES0_i_nid_postfix(nullptr, Icu::StringPiece{}, 0); }), "find rejects a null object");
    Require(Rejects([] { _ZN6icu_6811StringPiece7compareES0__nid_postfix(nullptr, Icu::StringPiece{}); }), "compare rejects a null object");
    Require(Rejects([&] { _ZN6icu_68eqERKNS_11StringPieceES2__nid_postfix(nullptr, &hello); }), "operator== rejects a null left piece");
    Require(Rejects([&] { _ZN6icu_68eqERKNS_11StringPieceES2__nid_postfix(&hello, nullptr); }), "operator== rejects a null right piece");
    Icu::StringPiece broken{nullptr, 3};
    Require(Rejects([&] { _ZN6icu_6811StringPiece4findES0_i_nid_postfix(&broken, Icu::StringPiece{"a", 1}, 0); }), "find rejects a null text it reads");
    Require(_ZN6icu_6811StringPiece4findES0_i_nid_postfix(&broken, Icu::StringPiece{nullptr, 0}, 1) == 1, "find reads nothing for an empty needle");
    Require(Rejects([&] { _ZN6icu_6811StringPiece7compareES0__nid_postfix(&broken, Icu::StringPiece{"a", 1}); }), "compare rejects a null text it reads");
    Require(_ZN6icu_6811StringPiece7compareES0__nid_postfix(&broken, Icu::StringPiece{nullptr, 0}) == 1, "compare reads nothing against an empty piece");
    const Icu::StringPiece brokenOther{nullptr, 3};
    Require(Rejects([&] { _ZN6icu_68eqERKNS_11StringPieceES2__nid_postfix(&hello, &brokenOther); }) == false, "lengths differ before any read");
    const Icu::StringPiece abc{"abc", 3};
    Require(Rejects([&] { _ZN6icu_68eqERKNS_11StringPieceES2__nid_postfix(&abc, &brokenOther); }), "operator== rejects a null text it reads");
}

std::u16string RandomPatternText(std::mt19937& random) {
    static const char16_t alphabet[] = {u'a', u'Z', u'0', u'_', u' ', u'\t', u'\n', 0x200E, 0x2028, 0x85, u'#', u'-', u'(', 0x00A1, 0x3001, 0x4E00, 0xFE45};
    std::u16string text;
    const auto length = random() % 7;
    for (unsigned index = 0; index < length; ++index) text.push_back(alphabet[random() % (sizeof(alphabet) / sizeof(alphabet[0]))]);
    return text;
}

void CheckPatternProps() {
    for (std::int32_t c = -2; c <= 0x110001; ++c) {
        if (c > 0x3100 && c < 0xFD00 && c % 97 != 0) continue;
        if (c > 0xFF00 && c < 0x10FF00 && c % 211 != 0) continue;
        Require(_ZN6icu_6812PatternProps8isSyntaxEi_nid_postfix(c) == icu::PatternProps::isSyntax(c), "isSyntax matches ICU");
        Require(_ZN6icu_6812PatternProps20isSyntaxOrWhiteSpaceEi_nid_postfix(c) == icu::PatternProps::isSyntaxOrWhiteSpace(c),
                "isSyntaxOrWhiteSpace matches ICU");
        Require(_ZN6icu_6812PatternProps12isWhiteSpaceEi_nid_postfix(c) == icu::PatternProps::isWhiteSpace(c), "isWhiteSpace matches ICU");
    }
    Require(_ZN6icu_6812PatternProps8isSyntaxEi_nid_postfix(u'#') == 1 && _ZN6icu_6812PatternProps8isSyntaxEi_nid_postfix(u'a') == 0, "known syntax");
    Require(_ZN6icu_6812PatternProps12isWhiteSpaceEi_nid_postfix(0x200E) == 1 && _ZN6icu_6812PatternProps12isWhiteSpaceEi_nid_postfix(0xA0) == 0,
            "Pattern_White_Space includes LRM but not NBSP");
    std::mt19937 random(11);
    for (int round = 0; round < 5000; ++round) {
        const std::u16string text = RandomPatternText(random);
        const char16_t* data = text.c_str();
        const auto length = static_cast<std::int32_t>(text.size());
        for (std::int32_t used = -1; used <= length; ++used) {
            Require(_ZN6icu_6812PatternProps14skipWhiteSpaceEPKDsi_nid_postfix(data, used) == icu::PatternProps::skipWhiteSpace(data, used),
                    "skipWhiteSpace matches ICU");
            Require(_ZN6icu_6812PatternProps14skipIdentifierEPKDsi_nid_postfix(data, used) == icu::PatternProps::skipIdentifier(data, used),
                    "skipIdentifier matches ICU");
            Require(_ZN6icu_6812PatternProps12isIdentifierEPKDsi_nid_postfix(data, used) == icu::PatternProps::isIdentifier(data, used),
                    "isIdentifier matches ICU");
            std::int32_t guestLength = used;
            std::int32_t hostLength = used;
            Require(_ZN6icu_6812PatternProps14trimWhiteSpaceEPKDsRi_nid_postfix(data, &guestLength) ==
                        icu::PatternProps::trimWhiteSpace(data, hostLength),
                    "trimWhiteSpace returns the same start");
            Require(guestLength == hostLength, "trimWhiteSpace returns the same length");
        }
    }
    const char16_t padded[] = u"  ab c\t";
    std::int32_t length = 7;
    Require(_ZN6icu_6812PatternProps14trimWhiteSpaceEPKDsRi_nid_postfix(padded, &length) == padded + 2 && length == 4, "trimWhiteSpace trims both ends");
    Require(_ZN6icu_6812PatternProps12isIdentifierEPKDsi_nid_postfix(nullptr, 0) == 0, "an empty identifier is not an identifier");
    Require(_ZN6icu_6812PatternProps14skipWhiteSpaceEPKDsi_nid_postfix(nullptr, 0) == nullptr, "skipping nothing reads nothing");
    Require(_ZN6icu_6812PatternProps14skipIdentifierEPKDsi_nid_postfix(nullptr, -1) == nullptr, "skipping a negative length reads nothing");
    std::int32_t zero = 0;
    Require(_ZN6icu_6812PatternProps14trimWhiteSpaceEPKDsRi_nid_postfix(nullptr, &zero) == nullptr && zero == 0, "trimming nothing reads nothing");
    Require(Rejects([] { _ZN6icu_6812PatternProps14skipWhiteSpaceEPKDsi_nid_postfix(nullptr, 2); }), "skipWhiteSpace rejects a null text it reads");
    Require(Rejects([] { _ZN6icu_6812PatternProps14skipIdentifierEPKDsi_nid_postfix(nullptr, 2); }), "skipIdentifier rejects a null text it reads");
    Require(Rejects([] { _ZN6icu_6812PatternProps12isIdentifierEPKDsi_nid_postfix(nullptr, 2); }), "isIdentifier rejects a null text it reads");
    Require(Rejects([] {
        std::int32_t two = 2;
        _ZN6icu_6812PatternProps14trimWhiteSpaceEPKDsRi_nid_postfix(nullptr, &two);
    }), "trimWhiteSpace rejects a null text it reads");
    Require(Rejects([&] { _ZN6icu_6812PatternProps14trimWhiteSpaceEPKDsRi_nid_postfix(padded, nullptr); }), "trimWhiteSpace rejects a null length");
}

void CheckInitOnce() {
    Icu::InitOnce once{0, 0};
    Require(_ZN6icu_6820umtx_initImplPreInitERNS_9UInitOnceE_nid_postfix(&once) == 1 && once.state == 1, "the first caller runs the initializer");
    std::atomic<int> waiterResult{-1};
    std::atomic<bool> waiterStarted{false};
    std::thread waiter([&] {
        waiterStarted = true;
        waiterResult = _ZN6icu_6820umtx_initImplPreInitERNS_9UInitOnceE_nid_postfix(&once);
    });
    while (!waiterStarted) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(waiterResult == -1, "a second caller waits while the initializer runs");
    _ZN6icu_6821umtx_initImplPostInitERNS_9UInitOnceE_nid_postfix(&once);
    waiter.join();
    Require(waiterResult == 0 && once.state == 2, "the waiting caller returns false after the initializer finishes");
    Require(_ZN6icu_6820umtx_initImplPreInitERNS_9UInitOnceE_nid_postfix(&once) == 0, "a finished once does not run again");
    once.state = 0;
    Require(_ZN6icu_6820umtx_initImplPreInitERNS_9UInitOnceE_nid_postfix(&once) == 1, "a reset once runs again");
    _ZN6icu_6821umtx_initImplPostInitERNS_9UInitOnceE_nid_postfix(&once);
    Require(once.state == 2 && once.errorCode == 0, "post-init marks the once done and keeps the error code");
    Require(Rejects([] { _ZN6icu_6820umtx_initImplPreInitERNS_9UInitOnceE_nid_postfix(nullptr); }), "pre-init rejects a null once");
    Require(Rejects([] { _ZN6icu_6821umtx_initImplPostInitERNS_9UInitOnceE_nid_postfix(nullptr); }), "post-init rejects a null once");
}

void CheckPaths() {
    Require(uprv_pathIsAbsolute_nid_postfix("/") == 1, "a slash path is absolute");
    Require(uprv_pathIsAbsolute_nid_postfix("/app0/data") == 1, "a rooted path is absolute");
    Require(uprv_pathIsAbsolute_nid_postfix("data/icu") == 0, "a relative path is not absolute");
    Require(uprv_pathIsAbsolute_nid_postfix("") == 0, "an empty path is not absolute");
    Require(uprv_pathIsAbsolute_nid_postfix(nullptr) == 0, "a null path is not absolute");
    Require(uprv_pathIsAbsolute_nid_postfix("\\share") == 0, "a backslash is not a separator on a POSIX build");
    Require(uprv_pathIsAbsolute_nid_postfix("C:/data") == 0, "a drive letter is not absolute on a POSIX build");
}

} // namespace

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == 0, "the tracking allocator is installed");
    CheckMemory();
    CheckStringPieceConstructors();
    CheckStringPieceSearch();
    CheckPatternProps();
    CheckInitOnce();
    CheckPaths();
    Require(heap.live.empty(), "every allocation is released");
    return 0;
}
