#include "prx/libSceIcu/DataSwapper.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <unicode/ucptrie.h>
#include <unicode/udata.h>
#include <unicode/umutablecptrie.h>
#include "dictionarydata.h"
#include "udatamem.h"
#include "udataswp.h"
#include "uprops.h"
#include "utrie2.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t Untouched = 0x5a5a5a5a;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "ICU DATA SWAPPER: %s\n", message.c_str());
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

struct Bytes {
    std::vector<std::uint64_t> storage;
    std::int32_t length = 0;

    explicit Bytes(std::int32_t size = 0) : storage(static_cast<std::size_t>(size + 7) / 8 + 1, 0), length(size) {}

    std::uint8_t* data() {
        return reinterpret_cast<std::uint8_t*>(storage.data());
    }

    const std::uint8_t* data() const {
        return reinterpret_cast<const std::uint8_t*>(storage.data());
    }
};

bool Same(const Bytes& left, const Bytes& right, std::int32_t length) {
    return std::memcmp(left.data(), right.data(), static_cast<std::size_t>(length)) == 0;
}

Bytes Item(const char* path, const char* type, const char* name) {
    UErrorCode status = U_ZERO_ERROR;
    UDataMemory* memory = udata_open(path, type, name, &status);
    Require(memory && U_SUCCESS(status), std::string("open ") + name);
    const auto* raw = static_cast<const std::uint8_t*>(udata_getRawMemory(memory));
    const std::int32_t header = static_cast<std::int32_t>(static_cast<const std::uint8_t*>(udata_getMemory(memory)) - raw);
    const std::int32_t length = udata_getLength(memory) + header;
    Bytes bytes(length);
    std::memcpy(bytes.data(), raw, static_cast<std::size_t>(length));
    udata_close(memory);
    return bytes;
}

UDataSwapper* HostSwapper(bool inBig, std::uint8_t inCharset, bool outBig, std::uint8_t outCharset) {
    UErrorCode status = U_ZERO_ERROR;
    UDataSwapper* ds = udata_openSwapper(inBig, inCharset, outBig, outCharset, &status);
    Require(ds && U_SUCCESS(status), "host swapper");
    return ds;
}

GuestDataSwapper* Swapper(bool inBig, std::uint8_t inCharset, bool outBig, std::uint8_t outCharset) {
    std::int32_t status = Ok;
    GuestDataSwapper* ds = udata_openSwapper_nid_postfix(inBig, inCharset, outBig, outCharset, &status);
    Require(ds && status == Ok, "guest swapper");
    return ds;
}

std::string printed;
std::string lastFormat;

void APS5_VABI GuestPrintError(void* context, const char* fmt, VaList* args) {
    char buffer[512];
    vsnprintf_nid_postfix(buffer, sizeof(buffer), fmt, args);
    lastFormat = fmt;
    printed += buffer;
    if (context) printed += *static_cast<const std::string*>(context);
}

std::string hostPrinted;

void U_CALLCONV HostPrintError(void*, const char* fmt, va_list args) {
    char buffer[512];
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    hostPrinted += buffer;
}

int read16Calls = 0;
int read32Calls = 0;
int swap16Calls = 0;
int swapInvCalls = 0;
GuestReadUInt16 savedRead16 = nullptr;
GuestReadUInt32 savedRead32 = nullptr;
GuestSwapFn savedSwap16 = nullptr;
GuestSwapFn savedSwapInv = nullptr;
const GuestDataSwapper* expectedDs = nullptr;

std::uint16_t APS5_VABI CountingRead16(std::uint16_t x) {
    ++read16Calls;
    return savedRead16(x);
}

std::uint32_t APS5_VABI CountingRead32(std::uint32_t x) {
    ++read32Calls;
    return savedRead32(x);
}

std::int32_t APS5_VABI CountingSwap16(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status) {
    ++swap16Calls;
    Require(ds == expectedDs, "replaced swapArray16 receives the guest swapper");
    return savedSwap16(ds, inData, length, outData, status);
}

std::int32_t APS5_VABI CountingSwapInv(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status) {
    ++swapInvCalls;
    Require(ds == expectedDs, "replaced swapInvChars receives the guest swapper");
    return savedSwapInv(ds, inData, length, outData, status);
}

void TestPrimitives() {
    const std::uint16_t words16[] = {0x1234, 0xabcd, 0x0001};
    const std::uint32_t words32[] = {0x11223344, 0xa0b0c0d0};
    const std::uint64_t words64[] = {0x0102030405060708ull, 0x1122334455667788ull};
    const char ascii[] = "Hello World 123";
    const char16_t local[] = u"Hello World 123";
    for (int combination = 0; combination < 16; ++combination) {
        const bool inBig = combination & 1;
        const std::uint8_t inCharset = (combination >> 1) & 1;
        const bool outBig = (combination >> 2) & 1;
        const std::uint8_t outCharset = (combination >> 3) & 1;
        GuestDataSwapper* guest = Swapper(inBig, inCharset, outBig, outCharset);
        UDataSwapper* host = HostSwapper(inBig, inCharset, outBig, outCharset);
        Require(guest->inIsBigEndian == inBig && guest->inCharset == inCharset && guest->outIsBigEndian == outBig && guest->outCharset == outCharset, "swapper flags");
        Require(guest->printError == nullptr && guest->printErrorContext == nullptr, "swapper print function");
        Require(guest->readUInt16(0x1234) == host->readUInt16(0x1234) && guest->readUInt32(0x11223344) == host->readUInt32(0x11223344), "read functions against ICU");
        Require(guest->readUInt16(0x1234) == (inBig ? 0x3412 : 0x1234), "readUInt16 value");
        Require(udata_readInt16_nid_postfix(guest, 0x1234) == udata_readInt16(host, 0x1234) && udata_readInt32_nid_postfix(guest, -2) == udata_readInt32(host, -2), "readInt against ICU");
        std::uint16_t guest16 = 0;
        std::uint16_t host16 = 0;
        std::uint32_t guest32 = 0;
        std::uint32_t host32 = 0;
        guest->writeUInt16(&guest16, 0x1234);
        host->writeUInt16(&host16, 0x1234);
        guest->writeUInt32(&guest32, 0x11223344);
        host->writeUInt32(&host32, 0x11223344);
        Require(guest16 == host16 && guest32 == host32 && guest16 == (outBig ? 0x3412 : 0x1234), "write functions against ICU");
        std::uint8_t guestOut[64]{};
        std::uint8_t hostOut[64]{};
        std::int32_t status = Ok;
        UErrorCode hostStatus = U_ZERO_ERROR;
        Require(guest->swapArray16(guest, words16, sizeof(words16), guestOut, &status) == host->swapArray16(host, words16, sizeof(words16), hostOut, &hostStatus) && std::memcmp(guestOut, hostOut, sizeof(words16)) == 0, "swapArray16 against ICU");
        Require(guest->swapArray32(guest, words32, sizeof(words32), guestOut, &status) == host->swapArray32(host, words32, sizeof(words32), hostOut, &hostStatus) && std::memcmp(guestOut, hostOut, sizeof(words32)) == 0, "swapArray32 against ICU");
        Require(guest->swapArray64(guest, words64, sizeof(words64), guestOut, &status) == host->swapArray64(host, words64, sizeof(words64), hostOut, &hostStatus) && std::memcmp(guestOut, hostOut, sizeof(words64)) == 0, "swapArray64 against ICU");
        Require(status == Ok && U_SUCCESS(hostStatus), "array swaps succeed");
        Require(guest->swapInvChars(guest, ascii, 15, guestOut, &status) == host->swapInvChars(host, ascii, 15, hostOut, &hostStatus) && std::memcmp(guestOut, hostOut, 15) == 0 && status == hostStatus, "swapInvChars against ICU");
        Require(guest->compareInvChars(guest, ascii, 15, local, 15) == host->compareInvChars(host, ascii, 15, local, 15), "compareInvChars against ICU");
        Require(guest->compareInvChars(guest, "Hellp", 5, local, 5) == host->compareInvChars(host, "Hellp", 5, local, 5), "compareInvChars order against ICU");
        status = Ok;
        Require(guest->swapArray16(guest, words16, 3, guestOut, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "swapArray16 odd length");
        status = Ok;
        Require(guest->swapArray32(nullptr, words32, 4, guestOut, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "swapArray32 NULL swapper");
        status = U_INVALID_FORMAT_ERROR;
        Require(guest->swapArray64(guest, nullptr, 8, nullptr, &status) == 0 && status == U_INVALID_FORMAT_ERROR, "swapArray64 failed status");
        udata_closeSwapper(host);
        udata_closeSwapper_nid_postfix(guest);
    }
    GuestDataSwapper* ebcdic = Swapper(false, U_ASCII_FAMILY, false, U_EBCDIC_FAMILY);
    std::uint8_t out[4]{};
    std::int32_t status = Ok;
    Require(ebcdic->swapInvChars(ebcdic, "A", 1, out, &status) == 1 && out[0] == 0xc1, "ASCII to EBCDIC");
    status = Ok;
    Require(ebcdic->swapInvChars(ebcdic, "a[b", 3, out, &status) == 0 && status == U_INVALID_CHAR_FOUND, "variant character without print function");
    udata_closeSwapper_nid_postfix(ebcdic);
}

void TestOpenAndErrors() {
    std::int32_t status = Ok;
    Require(udata_openSwapper_nid_postfix(0, 2, 0, 0, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "openSwapper charset");
    status = Ok;
    Require(udata_openSwapper_nid_postfix(0, 0, 0, 7, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "openSwapper out charset");
    status = Untouched;
    Require(udata_openSwapper_nid_postfix(0, 0, 0, 0, &status) == nullptr && status == Untouched, "openSwapper failed status");
    Require(udata_openSwapper_nid_postfix(0, 0, 0, 0, nullptr) == nullptr, "openSwapper NULL status");
    const Bytes names = Item(nullptr, "icu", "unames");
    status = Ok;
    GuestDataSwapper* forInput = udata_openSwapperForInputData_nid_postfix(names.data(), names.length, 1, U_EBCDIC_FAMILY, &status);
    Require(forInput && status == Ok && forInput->inIsBigEndian == 0 && forInput->inCharset == U_ASCII_FAMILY && forInput->outIsBigEndian == 1 && forInput->outCharset == U_EBCDIC_FAMILY, "openSwapperForInputData");
    Require(forInput->readUInt16(0x1234) == 0x1234 && forInput->writeUInt16 != nullptr, "openSwapperForInputData functions");
    udata_closeSwapper_nid_postfix(forInput);
    status = Ok;
    forInput = udata_openSwapperForInputData_nid_postfix(names.data(), -1, 0, 0, &status);
    Require(forInput && status == Ok, "openSwapperForInputData preflight length");
    udata_closeSwapper_nid_postfix(forInput);
    status = Ok;
    Require(udata_openSwapperForInputData_nid_postfix(nullptr, 100, 0, 0, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "openSwapperForInputData NULL data");
    status = Ok;
    Require(udata_openSwapperForInputData_nid_postfix(names.data(), 10, 0, 0, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "openSwapperForInputData short");
    status = Ok;
    Require(udata_openSwapperForInputData_nid_postfix(names.data(), names.length, 0, 2, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "openSwapperForInputData charset");
    Bytes bad = names;
    bad.data()[2] = 0;
    status = Ok;
    Require(udata_openSwapperForInputData_nid_postfix(bad.data(), bad.length, 0, 0, &status) == nullptr && status == U_UNSUPPORTED_ERROR, "openSwapperForInputData magic");
    status = Untouched;
    Require(udata_openSwapperForInputData_nid_postfix(nullptr, 0, 0, 0, &status) == nullptr && status == Untouched, "openSwapperForInputData failed status");

    udata_closeSwapper_nid_postfix(nullptr);
    GuestDataSwapper* ds = Swapper(false, 0, true, 0);
    GuestDataSwapper copy = *ds;
    Require(Invalid([&] { udata_closeSwapper_nid_postfix(&copy); }), "closeSwapper foreign swapper");
    udata_closeSwapper_nid_postfix(ds);
    Require(Invalid([&] { udata_closeSwapper_nid_postfix(ds); }), "closeSwapper twice");
    Require(Invalid([] { udata_readInt16_nid_postfix(nullptr, 1); }) && Invalid([] { udata_readInt32_nid_postfix(nullptr, 1); }), "readInt NULL swapper");
    copy.readUInt16 = nullptr;
    copy.readUInt32 = nullptr;
    Require(Invalid([&] { udata_readInt16_nid_postfix(&copy, 1); }) && Invalid([&] { udata_readInt32_nid_postfix(&copy, 1); }), "readInt NULL function");
    Require(Invalid([] { udata_printError_nid_postfix(nullptr, "x"); }), "printError NULL swapper");
}

void TestHeaderAndStrings() {
    const Bytes names = Item(nullptr, "icu", "unames");
    GuestDataSwapper* guest = Swapper(false, 0, true, 0);
    UDataSwapper* host = HostSwapper(false, 0, true, 0);
    Bytes guestOut(names.length);
    Bytes hostOut(names.length);
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    const std::int32_t headerSize = udata_swapDataHeader_nid_postfix(guest, names.data(), names.length, guestOut.data(), &status);
    Require(headerSize > 24 && status == Ok && headerSize == udata_swapDataHeader(host, names.data(), names.length, hostOut.data(), &hostStatus) && Same(guestOut, hostOut, headerSize), "swapDataHeader against ICU");
    Require(guestOut.data()[8] == 1, "swapped header is big-endian");
    status = Ok;
    Require(udata_swapDataHeader_nid_postfix(guest, names.data(), -1, nullptr, &status) == headerSize && status == Ok, "swapDataHeader preflight");
    status = Ok;
    Require(udata_swapDataHeader_nid_postfix(nullptr, names.data(), names.length, guestOut.data(), &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "swapDataHeader NULL swapper");
    Require(udata_swapDataHeader_nid_postfix(guest, names.data(), names.length, guestOut.data(), nullptr) == 0, "swapDataHeader NULL status");
    status = U_INVALID_FORMAT_ERROR;
    GuestDataSwapper empty{};
    Require(udata_swapDataHeader_nid_postfix(&empty, names.data(), names.length, guestOut.data(), &status) == 0 && status == U_INVALID_FORMAT_ERROR, "swapDataHeader failed status reads no swapper");
    status = Ok;
    Require(Invalid([&] { udata_swapDataHeader_nid_postfix(&empty, names.data(), names.length, guestOut.data(), &status); }), "swapDataHeader NULL functions");

    const char strings[] = "abc\0def\0gh";
    char guestStrings[16]{};
    char hostStrings[16]{};
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    Require(udata_swapInvStringBlock_nid_postfix(guest, strings, 11, guestStrings, &status) == udata_swapInvStringBlock(host, strings, 11, hostStrings, &hostStatus) && std::memcmp(guestStrings, hostStrings, 11) == 0 && status == Ok, "swapInvStringBlock against ICU");
    status = Ok;
    Require(udata_swapInvStringBlock_nid_postfix(guest, strings, -1, guestStrings, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "swapInvStringBlock negative length");
    Require(udata_swapInvStringBlock_nid_postfix(guest, strings, 11, guestStrings, nullptr) == 0, "swapInvStringBlock NULL status");
    GuestDataSwapper* ebcdic = Swapper(false, U_ASCII_FAMILY, false, U_EBCDIC_FAMILY);
    UDataSwapper* hostEbcdic = HostSwapper(false, U_ASCII_FAMILY, false, U_EBCDIC_FAMILY);
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    Require(udata_swapInvStringBlock_nid_postfix(ebcdic, strings, 11, guestStrings, &status) == udata_swapInvStringBlock(hostEbcdic, strings, 11, hostStrings, &hostStatus) && std::memcmp(guestStrings, hostStrings, 11) == 0, "swapInvStringBlock EBCDIC against ICU");
    udata_closeSwapper(hostEbcdic);
    udata_closeSwapper_nid_postfix(ebcdic);
    udata_closeSwapper(host);
    udata_closeSwapper_nid_postfix(guest);
}

template<typename TGuest, typename THost>
void RoundTrip(const Bytes& original, TGuest guestSwap, THost hostSwap, const std::string& name) {
    GuestDataSwapper* toBig = Swapper(false, 0, true, 0);
    GuestDataSwapper* toLittle = Swapper(true, 0, false, 0);
    UDataSwapper* host = HostSwapper(false, 0, true, 0);
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    const std::int32_t preflight = guestSwap(toBig, original.data(), -1, nullptr, &status);
    Require(status == Ok && preflight == hostSwap(host, original.data(), -1, nullptr, &hostStatus) && preflight > 0 && preflight <= original.length, name + " preflight against ICU");
    Bytes big(original.length);
    Bytes expected(original.length);
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    const std::int32_t length = guestSwap(toBig, original.data(), original.length, big.data(), &status);
    Require(status == Ok && length == hostSwap(host, original.data(), original.length, expected.data(), &hostStatus) && U_SUCCESS(hostStatus) && length == preflight, name + " swap against ICU");
    Require(Same(big, expected, length) && !Same(big, original, length), name + " swapped bytes");
    Bytes back(original.length);
    status = Ok;
    Require(guestSwap(toLittle, big.data(), length, back.data(), &status) == length && status == Ok && Same(back, original, length), name + " round trip");
    Bytes inPlace = big;
    status = Ok;
    Require(guestSwap(toLittle, inPlace.data(), length, inPlace.data(), &status) == length && status == Ok && Same(inPlace, original, length), name + " in place");
    status = Ok;
    Require(guestSwap(nullptr, original.data(), original.length, big.data(), &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, name + " NULL swapper");
    status = U_INVALID_FORMAT_ERROR;
    Require(guestSwap(toBig, original.data(), original.length, big.data(), &status) == 0 && status == U_INVALID_FORMAT_ERROR, name + " failed status");
    udata_closeSwapper(host);
    udata_closeSwapper_nid_postfix(toBig);
    udata_closeSwapper_nid_postfix(toLittle);
}

Bytes Trie2Bytes() {
    UErrorCode status = U_ZERO_ERROR;
    UTrie2* trie = utrie2_open(0, 0xbad, &status);
    utrie2_setRange32(trie, 0x41, 0x5a, 0x12345, true, &status);
    utrie2_setRange32(trie, 0x2f800, 0x2fa00, 7, true, &status);
    utrie2_freeze(trie, UTRIE2_32_VALUE_BITS, &status);
    const std::int32_t length = utrie2_serialize(trie, nullptr, 0, &status);
    status = U_ZERO_ERROR;
    Bytes bytes(length);
    utrie2_serialize(trie, bytes.data(), length, &status);
    utrie2_close(trie);
    Require(U_SUCCESS(status), "UTrie2 bytes");
    return bytes;
}

Bytes CodePointTrieBytes(UCPTrieType type, UCPTrieValueWidth width) {
    UErrorCode status = U_ZERO_ERROR;
    UMutableCPTrie* mutableTrie = umutablecptrie_open(0, 0xad, &status);
    umutablecptrie_setRange(mutableTrie, 0x41, 0x5a, 0x1234, &status);
    umutablecptrie_setRange(mutableTrie, 0x30000, 0x30100, 0x55, &status);
    UCPTrie* trie = umutablecptrie_buildImmutable(mutableTrie, type, width, &status);
    const std::int32_t length = ucptrie_toBinary(trie, nullptr, 0, &status);
    status = U_ZERO_ERROR;
    Bytes bytes(length);
    ucptrie_toBinary(trie, bytes.data(), length, &status);
    ucptrie_close(trie);
    umutablecptrie_close(mutableTrie);
    Require(U_SUCCESS(status), "UCPTrie bytes");
    return bytes;
}

Bytes Trie1Bytes() {
    const std::int32_t indexLength = 2048;
    const std::int32_t dataLength = 64;
    Bytes bytes(16 + indexLength * 2 + dataLength * 4);
    const std::uint32_t header[4] = {0x54726965, 0x100u | (2u << 4) | 5u, static_cast<std::uint32_t>(indexLength), static_cast<std::uint32_t>(dataLength)};
    std::memcpy(bytes.data(), header, sizeof(header));
    auto* index = reinterpret_cast<std::uint16_t*>(bytes.data() + 16);
    for (std::int32_t i = 0; i < indexLength; ++i) index[i] = static_cast<std::uint16_t>(i == 2 ? 8 : 0);
    auto* data = reinterpret_cast<std::uint32_t*>(index + indexLength);
    for (std::int32_t i = 0; i < dataLength; ++i) data[i] = 0x01020304u * static_cast<std::uint32_t>(i);
    return bytes;
}

void TestFormatSwaps() {
    RoundTrip(Trie2Bytes(), utrie2_swap_nid_postfix, utrie2_swap, "utrie2_swap");
    RoundTrip(CodePointTrieBytes(UCPTRIE_TYPE_FAST, UCPTRIE_VALUE_BITS_16), ucptrie_swap_nid_postfix, ucptrie_swap, "ucptrie_swap 16");
    RoundTrip(CodePointTrieBytes(UCPTRIE_TYPE_SMALL, UCPTRIE_VALUE_BITS_32), ucptrie_swap_nid_postfix, ucptrie_swap, "ucptrie_swap 32");
    Bytes small8 = CodePointTrieBytes(UCPTRIE_TYPE_FAST, UCPTRIE_VALUE_BITS_8);
    GuestDataSwapper* toBig = Swapper(false, 0, true, 0);
    UDataSwapper* host = HostSwapper(false, 0, true, 0);
    Bytes guestOut(small8.length);
    Bytes hostOut(small8.length);
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    Require(ucptrie_swap_nid_postfix(toBig, small8.data(), small8.length, guestOut.data(), &status) == ucptrie_swap(host, small8.data(), small8.length, hostOut.data(), &hostStatus) && Same(guestOut, hostOut, small8.length), "ucptrie_swap 8 against ICU");
    for (const Bytes& trie : {Trie1Bytes(), Trie2Bytes(), CodePointTrieBytes(UCPTRIE_TYPE_FAST, UCPTRIE_VALUE_BITS_32)}) {
        Bytes guestAny(trie.length);
        Bytes hostAny(trie.length);
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        const std::int32_t length = utrie_swapAnyVersion_nid_postfix(toBig, trie.data(), trie.length, guestAny.data(), &status);
        Require(length == trie.length && status == Ok && length == utrie_swapAnyVersion(host, trie.data(), trie.length, hostAny.data(), &hostStatus) && Same(guestAny, hostAny, length), "utrie_swapAnyVersion against ICU");
        status = Ok;
        const std::int32_t anyPreflight = utrie_swapAnyVersion_nid_postfix(toBig, trie.data(), -1, nullptr, &status);
        hostStatus = U_ZERO_ERROR;
        Require(anyPreflight == 0 && status == U_INVALID_FORMAT_ERROR && utrie_swapAnyVersion(host, trie.data(), -1, nullptr, &hostStatus) == 0 && hostStatus == status, "utrie_swapAnyVersion preflight is an error in ICU 68.2");
    }
    Bytes trie1 = Trie1Bytes();
    Bytes back(trie1.length);
    Bytes big(trie1.length);
    GuestDataSwapper* toLittle = Swapper(true, 0, false, 0);
    status = Ok;
    utrie_swapAnyVersion_nid_postfix(toBig, trie1.data(), trie1.length, big.data(), &status);
    Require(utrie_swapAnyVersion_nid_postfix(toLittle, big.data(), trie1.length, back.data(), &status) == trie1.length && status == Ok && Same(back, trie1, trie1.length), "UTrie v1 round trip");
    status = Ok;
    Require(utrie_swapAnyVersion_nid_postfix(toBig, trie1.data(), 15, big.data(), &status) == 0 && status == U_INVALID_FORMAT_ERROR, "utrie_swapAnyVersion short");
    status = Ok;
    Require(utrie2_swap_nid_postfix(toBig, trie1.data(), trie1.length, big.data(), &status) == 0 && status == U_INVALID_FORMAT_ERROR, "utrie2_swap wrong signature");
    Require(Invalid([&] { utrie2_swap_nid_postfix(toBig, trie1.data(), trie1.length, big.data(), nullptr); }) && Invalid([&] { ucptrie_swap_nid_postfix(toBig, trie1.data(), trie1.length, big.data(), nullptr); }) && Invalid([&] { utrie_swapAnyVersion_nid_postfix(toBig, trie1.data(), trie1.length, big.data(), nullptr); }), "trie swaps NULL status");

    RoundTrip(Item(nullptr, "icu", "unames"), uchar_swapNames_nid_postfix, uchar_swapNames, "uchar_swapNames");
    RoundTrip(Item("ICUDATA-brkitr", "dict", "thaidict"), udict_swap_nid_postfix, udict_swap, "udict_swap bytes");
    RoundTrip(Item("ICUDATA-brkitr", "dict", "cjdict"), udict_swap_nid_postfix, udict_swap, "udict_swap uchars");
    Require(uchar_swapNames_nid_postfix(toBig, nullptr, 0, nullptr, nullptr) == 0 && udict_swap_nid_postfix(toBig, nullptr, 0, nullptr, nullptr) == 0, "names and dictionary NULL status");
    udata_closeSwapper(host);
    udata_closeSwapper_nid_postfix(toBig);
    udata_closeSwapper_nid_postfix(toLittle);
}

void TestReplacedFunctions() {
    const Bytes trie = Trie2Bytes();
    const Bytes names = Item(nullptr, "icu", "unames");
    GuestDataSwapper* opened = Swapper(false, 0, true, 0);
    UDataSwapper* host = HostSwapper(false, 0, true, 0);
    GuestDataSwapper custom = *opened;
    savedRead16 = custom.readUInt16;
    savedRead32 = custom.readUInt32;
    savedSwap16 = custom.swapArray16;
    savedSwapInv = custom.swapInvChars;
    custom.readUInt16 = CountingRead16;
    custom.readUInt32 = CountingRead32;
    custom.swapArray16 = CountingSwap16;
    custom.swapInvChars = CountingSwapInv;
    expectedDs = &custom;
    Bytes guestOut(trie.length);
    Bytes hostOut(trie.length);
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    Require(utrie2_swap_nid_postfix(&custom, trie.data(), trie.length, guestOut.data(), &status) == utrie2_swap(host, trie.data(), trie.length, hostOut.data(), &hostStatus) && Same(guestOut, hostOut, trie.length) && status == Ok, "replaced functions swap like ICU");
    Require(read16Calls == 3 && read32Calls == 1 && swap16Calls == 2 && swapInvCalls == 0, "replaced functions called by ICU");
    Bytes header(names.length);
    status = Ok;
    Require(udata_swapDataHeader_nid_postfix(&custom, names.data(), names.length, header.data(), &status) > 0 && status == Ok && swapInvCalls == 1, "replaced swapInvChars called by ICU");
    Require(udata_readInt16_nid_postfix(&custom, 0x1234) == 0x1234 && read16Calls == 6, "readInt16 calls the guest function");
    Require(udata_readInt32_nid_postfix(&custom, 1) == 1 && read32Calls == 2, "readInt32 calls the guest function");
    GuestDataSwapper swapped = *opened;
    GuestDataSwapper* copyVariant = Swapper(false, 0, false, 0);
    swapped.swapArray32 = copyVariant->swapArray32;
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    UDataSwapper* copyHost = HostSwapper(false, 0, false, 0);
    host->swapArray32 = copyHost->swapArray32;
    Require(utrie2_swap_nid_postfix(&swapped, trie.data(), trie.length, guestOut.data(), &status) == utrie2_swap(host, trie.data(), trie.length, hostOut.data(), &hostStatus) && Same(guestOut, hostOut, trie.length), "library functions moved between swappers");
    GuestDataSwapper broken = *opened;
    broken.swapArray64 = nullptr;
    status = Ok;
    Require(Invalid([&] { utrie2_swap_nid_postfix(&broken, trie.data(), trie.length, guestOut.data(), &status); }), "NULL swapper function");
    Require(Invalid([&] { broken.swapArray16(&broken, trie.data(), 2, guestOut.data(), &status); }), "NULL swapper function in a library function");
    udata_closeSwapper(copyHost);
    udata_closeSwapper(host);
    udata_closeSwapper_nid_postfix(copyVariant);
    udata_closeSwapper_nid_postfix(opened);
}

void TestPrintError() {
    const Bytes names = Item(nullptr, "icu", "unames");
    GuestDataSwapper* ds = Swapper(false, 0, true, 0);
    UDataSwapper* host = HostSwapper(false, 0, true, 0);
    host->printError = HostPrintError;
    const std::string suffix = "|";
    ds->printError = GuestPrintError;
    ds->printErrorContext = const_cast<std::string*>(&suffix);
    printed.clear();
    udata_printError_nid_postfix(ds, "x=%d s=%s %02x %u %lld\n", -5, "abc", 10, 4000000000u, -7LL);
    Require(printed == "x=-5 s=abc 0a 4000000000 -7\n|", "printError export");
    Bytes bad = names;
    bad.data()[2] = 0;
    Bytes out(names.length);
    std::int32_t status = Ok;
    printed.clear();
    hostPrinted.clear();
    UErrorCode hostStatus = U_ZERO_ERROR;
    Require(udata_swapDataHeader_nid_postfix(ds, bad.data(), bad.length, out.data(), &status) == 0 && status == U_UNSUPPORTED_ERROR, "swapDataHeader bad magic");
    udata_swapDataHeader(host, bad.data(), bad.length, out.data(), &hostStatus);
    Require(printed == hostPrinted + "|" && printed.find("initial bytes") != std::string::npos, "printError without arguments");
    bad = names;
    status = Ok;
    printed.clear();
    hostPrinted.clear();
    hostStatus = U_ZERO_ERROR;
    Require(udata_swapDataHeader_nid_postfix(ds, bad.data(), 30, out.data(), &status) == 0 && status == U_INDEX_OUTOFBOUNDS_ERROR, "swapDataHeader short");
    udata_swapDataHeader(host, bad.data(), 30, out.data(), &hostStatus);
    Require(printed == hostPrinted + "|" && printed.find("length 30") != std::string::npos, "printError with three integers");
    Require(lastFormat.find("length %d") != std::string::npos, "printError keeps ICU's integer format");
    const Bytes dictionary = Item("ICUDATA-brkitr", "dict", "thaidict");
    status = Ok;
    printed.clear();
    hostPrinted.clear();
    hostStatus = U_ZERO_ERROR;
    Require(uchar_swapNames_nid_postfix(ds, dictionary.data(), dictionary.length, out.data(), &status) == 0 && status == U_UNSUPPORTED_ERROR, "uchar_swapNames on a dictionary");
    uchar_swapNames(host, dictionary.data(), dictionary.length, out.data(), &hostStatus);
    Require(printed == hostPrinted + "|" && printed.find("44.69.63.74") != std::string::npos, "printError with hex bytes");
    Require(lastFormat.find("%02x.%02x") != std::string::npos, "printError keeps ICU's format");
    GuestDataSwapper* ebcdic = Swapper(false, U_ASCII_FAMILY, false, U_EBCDIC_FAMILY);
    ebcdic->printError = GuestPrintError;
    std::uint8_t chars[4]{};
    status = Ok;
    printed.clear();
    Require(ebcdic->swapInvChars(ebcdic, "a[b", 3, chars, &status) == 0 && status == U_INVALID_CHAR_FOUND, "variant character");
    Require(printed == "uprv_ebcdicFromAscii() string[3] contains a variant character in position 1\n", "printError from a library function");
    ds->printError = nullptr;
    printed.clear();
    udata_printError_nid_postfix(ds, "%d", 1);
    status = Ok;
    udata_swapDataHeader_nid_postfix(ds, bad.data(), 30, out.data(), &status);
    Require(printed.empty() && status == U_INDEX_OUTOFBOUNDS_ERROR, "NULL print function");
    udata_closeSwapper_nid_postfix(ebcdic);
    udata_closeSwapper(host);
    udata_closeSwapper_nid_postfix(ds);
}

void PutU32(std::uint8_t* bytes, std::uint64_t offset, std::uint32_t value) {
    std::memcpy(bytes + offset, &value, 4);
}

std::uint32_t GetU32(const std::uint8_t* bytes, std::uint64_t offset) {
    std::uint32_t value;
    std::memcpy(&value, bytes + offset, 4);
    return value;
}

std::uint16_t GetU16(const std::uint8_t* bytes, std::uint64_t offset) {
    std::uint16_t value;
    std::memcpy(&value, bytes + offset, 2);
    return value;
}

void PutU16(std::uint8_t* bytes, std::uint64_t offset, std::uint16_t value) {
    std::memcpy(bytes + offset, &value, 2);
}

void TestGuards() {
    GuestDataSwapper* ds = Swapper(false, 0, true, 0);
    const Bytes names = Item(nullptr, "icu", "unames");
    const std::int32_t headerSize = names.data()[0] | (names.data()[1] << 8);
    const std::uint32_t available = static_cast<std::uint32_t>(names.length - headerSize);
    Bytes out(names.length + 64);
    const auto names2 = [&](Bytes bytes, std::int32_t length) {
        std::int32_t status = Ok;
        return uchar_swapNames_nid_postfix(ds, bytes.data(), length, out.data(), &status);
    };
    const auto mutate = [&](auto change) {
        Bytes bytes = names;
        change(bytes.data() + headerSize);
        return bytes;
    };
    Require(Invalid([&] { names2(mutate([&](std::uint8_t* b) { PutU32(b, 0, 900); PutU32(b, 4, 900); PutU16(b, 900, 0); PutU32(b, 12, 904); PutU32(b, 904, 0); PutU16(b, 16, 600); }), headerSize + 1000); }), "names token count");
    Require(Invalid([&] { names2(mutate([&](std::uint8_t* b) { PutU32(b, 0, GetU32(b, 4) + 4); }), names.length); }), "names token strings after groups");
    Require(Invalid([&] { names2(mutate([&](std::uint8_t* b) { PutU32(b, 0, 0xfffffff0u); PutU32(b, 4, 0); }), names.length); }), "names wrapped token strings");
    Require(Invalid([&] { names2(mutate([&](std::uint8_t* b) { PutU32(b, 4, available - 1); }), names.length); }), "names group table offset");
    Require(Invalid([&] { names2(mutate([&](std::uint8_t* b) { PutU32(b, 4, available - 4); PutU16(b, available - 4, 1); }), names.length); }), "names group count");
    Require(Invalid([&] { names2(mutate([&](std::uint8_t* b) { PutU32(b, 12, available - 2); }), names.length); }), "names range count offset");
    Require(Invalid([&] { names2(mutate([&](std::uint8_t* b) { PutU32(b, 12, available - 8); PutU32(b, available - 8, 1); }), names.length); }), "names range header");
    const std::uint8_t* base = names.data() + headerSize;
    std::uint64_t offset = GetU32(base, 12) + 4;
    std::uint64_t type0 = 0;
    std::uint64_t type1 = 0;
    for (std::uint32_t range = 0; range < GetU32(base, GetU32(base, 12)); ++range) {
        if (base[offset + 8] == 0 && !type0) type0 = offset;
        if (base[offset + 8] == 1 && !type1) type1 = offset;
        offset += GetU16(base, offset + 10);
    }
    Require(type0 && type1, "names algorithmic ranges");
    Require(Invalid([&] { names2(mutate([&](std::uint8_t* b) { PutU16(b, type1 + 10, 12); }), names.length); }), "names factor range size");
    Require(Invalid([&] { names2(mutate([&](std::uint8_t* b) { PutU16(b, type1 + 10, static_cast<std::uint16_t>(available - type1 + 2)); }), names.length); }), "names factor range end");
    Require(Invalid([&] {
        Bytes bytes = names;
        const std::uint64_t start = headerSize + type0 + 12;
        std::memset(bytes.data() + start, 'A', static_cast<std::size_t>(names.length - start));
        names2(bytes, names.length);
    }), "names prefix without NUL");
    std::int32_t status = Ok;
    Require(names2(mutate([&](std::uint8_t* b) { PutU32(b, 12, available + 1); }), names.length) == 0, "names offset past the data keeps ICU's error");
    Require(names2(mutate([&](std::uint8_t* b) {
        PutU32(b, 12, available - 17);
        PutU32(b, available - 17, 2);
        b[available - 13 + 8] = 0;
        PutU16(b, available - 13 + 10, 20);
        b[available - 1] = 0;
    }), names.length) == 0, "names range past the data keeps ICU's error");
    GuestDataSwapper* ebcdic = Swapper(false, U_ASCII_FAMILY, true, U_EBCDIC_FAMILY);
    status = Ok;
    Require(NotImplemented([&] { uchar_swapNames_nid_postfix(ebcdic, names.data(), names.length, out.data(), &status); }), "names charset change");
    status = Ok;
    UDataSwapper* hostEbcdic = HostSwapper(false, U_ASCII_FAMILY, true, U_EBCDIC_FAMILY);
    UErrorCode hostStatus = U_ZERO_ERROR;
    Require(uchar_swapNames_nid_postfix(ebcdic, names.data(), -1, nullptr, &status) == uchar_swapNames(hostEbcdic, names.data(), -1, nullptr, &hostStatus) && status == Ok, "names charset change preflight");
    udata_closeSwapper(hostEbcdic);

    const Bytes dictionary = Item("ICUDATA-brkitr", "dict", "cjdict");
    const std::int32_t dictionaryHeader = dictionary.data()[0] | (dictionary.data()[1] << 8);
    const auto dict = [&](auto change) {
        Bytes bytes = dictionary;
        change(bytes.data() + dictionaryHeader);
        Bytes target(dictionary.length);
        std::int32_t error = Ok;
        return udict_swap_nid_postfix(ds, bytes.data(), bytes.length, target.data(), &error);
    };
    Require(Invalid([&] { dict([](std::uint8_t* b) { PutU32(b, 12, 0x80000000u); }); }), "dictionary negative size");
    Require(Invalid([&] { dict([&](std::uint8_t* b) { PutU32(b, 16, 1); PutU32(b, 4, static_cast<std::uint32_t>(dictionary.length - dictionaryHeader + 2)); }); }), "dictionary trie end");
    Require(dict([&](std::uint8_t* b) { PutU32(b, 12, static_cast<std::uint32_t>(dictionary.length)); }) == 0, "dictionary size past the data keeps ICU's error");
    Require(dict([&](std::uint8_t* b) { PutU32(b, 16, 0); PutU32(b, 4, 0x7fffffff); }) > 0, "dictionary bytes trie ignores the offset");
    udata_closeSwapper_nid_postfix(ebcdic);
    udata_closeSwapper_nid_postfix(ds);
}

}

int main() {
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    TestPrimitives();
    TestOpenAndErrors();
    TestHeaderAndStrings();
    TestFormatSwaps();
    TestReplacedFunctions();
    TestPrintError();
    TestGuards();
    std::puts("ICU DATA SWAPPER TESTS PASSED");
    return 0;
}
