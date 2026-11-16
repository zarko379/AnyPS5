#include "prx/libSceIcu/StringTries.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include <unicode/bytestrie.h>
#include <unicode/bytestriebuilder.h>
#include <unicode/ucharstrie.h>
#include <unicode/ucharstriebuilder.h>
#include <unicode/udata.h>
#include <unicode/unistr.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU STRING TRIES: %s\n", message);
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
};

Heap heap;

void* APS5_VABI Allocate_nid_no_patch(const void*, std::size_t size) {
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
    std::free(memory);
}

static_assert(sizeof(icu::BytesTrie) == sizeof(Icu::BytesTrie) && sizeof(icu::UCharsTrie) == sizeof(Icu::UCharsTrie));

std::int32_t RandomValue(std::mt19937& random) {
    static const std::int32_t values[] = {0, 1, 5, 0x40, 0x41, 0xff, 0x1aff, 0x1b00, 0x3fff, 0x4000, 0xfeffff, 0xff0000, 0x3ffeffff, 0x3fff0000,
                                          0x7fffffff, -1, -0x12345678, 0x0dffff, 0x2fff};
    if (random() % 3 == 0) return values[random() % (sizeof(values) / sizeof(values[0]))];
    return static_cast<std::int32_t>(random() % 100);
}

std::string RandomKey(std::mt19937& random, bool wide) {
    static const char alphabet[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'x', 'y', 'z', '\x7f', '\x80', '\xff', '0'};
    std::string key;
    const auto length = 1 + random() % (wide ? 6 : 24);
    for (unsigned index = 0; index < length; ++index) key.push_back(alphabet[random() % (wide ? 6 : sizeof(alphabet))]);
    return key;
}

void SameBytes(const Icu::BytesTrie& guest, const icu::BytesTrie& host, const char* message) {
    const auto& view = reinterpret_cast<const Icu::BytesTrie&>(host);
    Require(guest.pos == view.pos && guest.remainingMatchLength == view.remainingMatchLength, message);
}

void CheckBytesTrie(std::uint32_t seed) {
    std::mt19937 random(seed);
    UErrorCode status = U_ZERO_ERROR;
    icu::BytesTrieBuilder builder(status);
    std::vector<std::string> keys;
    const auto count = 1 + random() % 120;
    std::set<std::string> unique;
    for (unsigned index = 0; index < count; ++index) {
        std::string key = RandomKey(random, false);
        if (!unique.insert(key).second) continue;
        keys.push_back(key);
        builder.add(icu::StringPiece(key.data(), static_cast<std::int32_t>(key.size())), RandomValue(random), status);
    }
    Require(U_SUCCESS(status), "the host builder accepts the keys");
    const icu::StringPiece serialized = builder.buildStringPiece(random() % 2 ? USTRINGTRIE_BUILD_FAST : USTRINGTRIE_BUILD_SMALL, status);
    Require(U_SUCCESS(status), "the host builder serializes the trie");
    const auto* data = reinterpret_cast<const std::uint8_t*>(serialized.data());
    for (int walk = 0; walk < 300; ++walk) {
        Icu::BytesTrie guest{nullptr, data, data, -1};
        icu::BytesTrie host(data);
        std::string probe = walk % 2 ? keys[random() % keys.size()] : RandomKey(random, false);
        if (random() % 3 == 0) probe.resize(random() % (probe.size() + 1));
        if (random() % 2) {
            for (char c : probe) {
                const auto in = random() % 9 == 0 ? static_cast<std::int32_t>(static_cast<unsigned char>(c)) : static_cast<std::int32_t>(c);
                Require(_ZN6icu_689BytesTrie4nextEi_nid_postfix(&guest, in) == host.next(in), "next(int) matches ICU");
                SameBytes(guest, host, "next(int) leaves the same state");
                Require(_ZNK6icu_689BytesTrie7currentEv_nid_postfix(&guest) == host.current(), "current matches ICU");
            }
        } else {
            const bool terminated = random() % 2 == 0;
            Require(_ZN6icu_689BytesTrie4nextEPKci_nid_postfix(&guest, probe.c_str(), terminated ? -1 : static_cast<std::int32_t>(probe.size())) ==
                        host.next(probe.c_str(), terminated ? -1 : static_cast<std::int32_t>(probe.size())),
                    "next(string) matches ICU");
            SameBytes(guest, host, "next(string) leaves the same state");
        }
        std::int32_t hostUnique = 0x5a5a;
        const bool hostHas = host.hasUniqueValue(hostUnique);
        std::int32_t guestUnique = 0x5a5a;
        const bool guestHas =
            guest.pos && _ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(guest.pos + guest.remainingMatchLength + 1, 0, &guestUnique);
        Require(guestHas == hostHas && (!hostHas || guestUnique == hostUnique), "findUniqueValue matches hasUniqueValue");
        if (host.current() >= USTRINGTRIE_FINAL_VALUE) {
            const std::uint8_t* pos = guest.pos;
            const std::int32_t lead = *pos >> 1;
            Require(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(pos + 1, lead) == host.getValue(), "readValue reads the value getValue returns");
        }
    }
}

void SameUnits(const Icu::UCharsTrie& guest, const icu::UCharsTrie& host, const char* message) {
    const auto& view = reinterpret_cast<const Icu::UCharsTrie&>(host);
    Require(guest.pos == view.pos && guest.remainingMatchLength == view.remainingMatchLength, message);
}

std::u16string RandomUnits(std::mt19937& random) {
    static const char16_t alphabet[] = {u'a', u'b', u'c', u'd', 0x00E9, 0x4E00, 0xD83D, 0xDE00, 0xFFFF, u'0', 0x0430};
    std::u16string text;
    const auto length = 1 + random() % 20;
    for (unsigned index = 0; index < length; ++index) text.push_back(alphabet[random() % (sizeof(alphabet) / sizeof(alphabet[0]))]);
    return text;
}

void CheckUCharsTrie(std::uint32_t seed, unsigned large) {
    std::mt19937 random(seed);
    UErrorCode status = U_ZERO_ERROR;
    icu::UCharsTrieBuilder builder(status);
    std::vector<std::u16string> keys;
    std::set<std::u16string> unique;
    const auto count = large ? large : 1 + random() % 120;
    for (unsigned index = 0; index < count; ++index) {
        std::u16string key = RandomUnits(random);
        if (large) key += static_cast<char16_t>(0x100 + index % 5000);
        if (!unique.insert(key).second) continue;
        keys.push_back(key);
        builder.add(icu::UnicodeString(key.data(), static_cast<std::int32_t>(key.size())), RandomValue(random), status);
        if (key.size() > 1 && random() % 3 == 0) {
            const std::u16string prefix = key.substr(0, key.size() - 1 - random() % (key.size() - 1));
            if (unique.insert(prefix).second) {
                keys.push_back(prefix);
                builder.add(icu::UnicodeString(prefix.data(), static_cast<std::int32_t>(prefix.size())), RandomValue(random), status);
            }
        }
    }
    icu::UnicodeString serialized;
    builder.buildUnicodeString(random() % 2 ? USTRINGTRIE_BUILD_FAST : USTRINGTRIE_BUILD_SMALL, serialized, status);
    Require(U_SUCCESS(status), "the host builder serializes the trie");
    const char16_t* data = serialized.getBuffer();
    for (int walk = 0; walk < 300; ++walk) {
        Icu::UCharsTrie guest{nullptr, data, data, -1};
        icu::UCharsTrie host(data);
        std::u16string probe = walk % 2 ? keys[random() % keys.size()] : RandomUnits(random);
        if (random() % 3 == 0) probe.resize(random() % (probe.size() + 1));
        const unsigned mode = random() % 3;
        if (mode == 0) {
            for (char16_t c : probe) {
                Require(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(&guest, c) == host.next(c), "next(int) matches ICU");
                SameUnits(guest, host, "next(int) leaves the same state");
                Require(_ZNK6icu_6810UCharsTrie7currentEv_nid_postfix(&guest) == host.current(), "current matches ICU");
            }
        } else if (mode == 1) {
            const char16_t* text = probe.c_str();
            const bool terminated = random() % 2 == 0;
            const std::int32_t length = terminated ? -1 : static_cast<std::int32_t>(probe.size());
            Require(_ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(&guest, &text, length) == host.next(text, length),
                    "next(string) matches ICU");
            SameUnits(guest, host, "next(string) leaves the same state");
        } else {
            static const std::int32_t points[] = {u'a', u'b', 0x4E00, 0x1F600, 0x10FFFF, 0xFFFF, u'0'};
            const std::int32_t first = points[random() % 7];
            Require(_ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(&guest, first) == host.firstForCodePoint(first), "firstForCodePoint matches ICU");
            SameUnits(guest, host, "firstForCodePoint leaves the same state");
            for (int step = 0; step < 4; ++step) {
                const std::int32_t cp = points[random() % 7];
                Require(_ZN6icu_6810UCharsTrie16nextForCodePointEi_nid_postfix(&guest, cp) == host.nextForCodePoint(cp), "nextForCodePoint matches ICU");
                SameUnits(guest, host, "nextForCodePoint leaves the same state");
            }
        }
        std::int32_t hostUnique = 0x5a5a;
        const bool hostHas = host.hasUniqueValue(hostUnique);
        std::int32_t guestUnique = 0x5a5a;
        const bool guestHas =
            guest.pos && _ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(guest.pos + guest.remainingMatchLength + 1, 0, &guestUnique);
        Require(guestHas == hostHas && (!hostHas || guestUnique == hostUnique), "findUniqueValue matches hasUniqueValue");
    }
}

void CheckDirect() {
    const std::uint8_t delta1[] = {0x05};
    Require(_ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(delta1) == delta1 + 1 + 5, "a one-byte delta");
    const std::uint8_t delta2[] = {0xc1, 0x02};
    Require(_ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(delta2) == delta2 + 2 + 0x102, "a two-byte delta");
    const std::uint8_t delta3[] = {0xf1, 0x00, 0x03};
    Require(_ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(delta3) == delta3 + 3 + 0x10003, "a three-byte delta");
    const std::uint8_t delta4[] = {0xfe, 0x01, 0x00, 0x00};
    Require(_ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(delta4) == delta4 + 4 + 0x10000, "a four-byte delta");
    const std::uint8_t delta5[] = {0xff, 0x00, 0x00, 0x01, 0x00};
    Require(_ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(delta5) == delta5 + 5 + 0x100, "a five-byte delta");
    const std::uint8_t value[] = {0x12, 0x34, 0x56, 0x78};
    Require(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(nullptr, 0x20) == 0x10, "a one-byte value reads nothing");
    Require(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(value, 0x52) == 0x112, "a two-byte value");
    Require(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(value, 0x6d) == 0x11234, "a three-byte value");
    Require(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(value, 0x7e) == 0x123456, "a four-byte value");
    Require(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(value, 0x7f) == 0x12345678, "a five-byte value");
    Icu::BytesTrie stopped{nullptr, value, nullptr, -1};
    Require(_ZN6icu_689BytesTrie4nextEi_nid_postfix(&stopped, 'a') == 0 && _ZNK6icu_689BytesTrie7currentEv_nid_postfix(&stopped) == 0,
            "a stopped trie matches nothing");
    Require(_ZN6icu_689BytesTrie4nextEPKci_nid_postfix(&stopped, "", -1) == 0 && _ZN6icu_689BytesTrie4nextEPKci_nid_postfix(&stopped, nullptr, 0) == 0,
            "an empty string reports the current state");
    Icu::BytesTrie owned{static_cast<std::uint8_t*>(uprv_malloc_nid_postfix(8)), nullptr, nullptr, -1};
    _ZN6icu_689BytesTrieD1Ev_nid_postfix(&owned);
    Icu::BytesTrie borrowed{nullptr, value, value, -1};
    _ZN6icu_689BytesTrieD2Ev_nid_postfix(&borrowed);
    Icu::UCharsTrie ownedUnits{static_cast<char16_t*>(uprv_malloc_nid_postfix(8)), nullptr, nullptr, -1};
    _ZN6icu_6810UCharsTrieD2Ev_nid_postfix(&ownedUnits);
    Icu::UCharsTrie borrowedUnits{nullptr, nullptr, nullptr, -1};
    _ZN6icu_6810UCharsTrieD1Ev_nid_postfix(&borrowedUnits);
    Require(heap.live.empty(), "the destructors free only an owned array");
    Require(Rejects([] { _ZN6icu_689BytesTrieD1Ev_nid_postfix(nullptr); }), "the destructor rejects a null trie");
    Require(Rejects([] { _ZN6icu_689BytesTrie4nextEi_nid_postfix(nullptr, 1); }), "next rejects a null trie");
    Require(Rejects([&] { _ZN6icu_689BytesTrie4nextEPKci_nid_postfix(&borrowed, nullptr, -1); }), "next rejects a null string");
    Require(Rejects([] { _ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(nullptr); }), "jumpByDelta rejects a null position");
    Require(Rejects([] { _ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(nullptr, 0x52); }), "readValue rejects a null position it reads");
    Require(Rejects([] {
        std::int32_t unique = 0;
        _ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(nullptr, 0, &unique);
    }), "findUniqueValue rejects a null position");
    Require(Rejects([&] { _ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(value, 0, nullptr); }), "findUniqueValue rejects a null result");
    Require(Rejects([&] { _ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(&borrowedUnits, 'a'); }), "first rejects a trie without data");
    Require(Rejects([&] { _ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(&borrowedUnits, nullptr, 2); }), "next rejects a null pointer object");
    Require(Rejects([] { _ZNK6icu_6810UCharsTrie7currentEv_nid_postfix(nullptr); }), "current rejects a null trie");
}

} // namespace

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == 0, "the tracking allocator is installed");
    CheckDirect();
    for (std::uint32_t seed = 1; seed <= 40; ++seed) {
        CheckBytesTrie(seed);
        CheckUCharsTrie(seed * 7, 0);
    }
    CheckUCharsTrie(99, 40000);
    return 0;
}
