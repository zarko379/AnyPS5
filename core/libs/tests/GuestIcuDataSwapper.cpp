#include "prx/libSceIcu/DataSwapper.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <unicode/ucptrie.h>
#include <unicode/udata.h>
#include <unicode/umutablecptrie.h>
#include "dictionarydata.h"
#include "normalizer2impl.h"
#include "rbbidata.h"
#include "ucol_data.h"
#include "ucol_swp.h"
#include "udatamem.h"
#include "udataswp.h"
#include "ucnv_bld.h"
#include "ucnv_ext.h"
#include "ucnv_io.h"
#include "ucnvmbcs.h"
#include "uresdata.h"
#include "uprops.h"
#include "utrie2.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
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
    const std::int32_t hostLength = hostSwap(host, original.data(), original.length, expected.data(), &hostStatus);
    Require(status == Ok && length == hostLength && U_SUCCESS(hostStatus) && length == preflight, name + " swap against ICU (" + std::to_string(status) + " " + std::to_string(length) + " " + std::to_string(hostStatus) + " " + std::to_string(hostLength) + " " + std::to_string(preflight) + ")");
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


std::int32_t Header(const Bytes& bytes) {
    return bytes.data()[0] | (bytes.data()[1] << 8);
}

template<typename TChange>
Bytes Mutated(const Bytes& original, TChange change) {
    Bytes bytes = original;
    change(bytes.data() + Header(original));
    return bytes;
}

template<typename TGuest, typename THost>
std::int32_t AgainstIcuAs(std::uint8_t outCharset, const Bytes& input, std::int32_t length, bool inPlace, TGuest guestSwap, THost hostSwap, const std::string& name) {
    GuestDataSwapper* ds = Swapper(false, 0, true, outCharset);
    UDataSwapper* host = HostSwapper(false, 0, true, outCharset);
    Bytes guestIn = input;
    Bytes hostIn = input;
    Bytes guestOut(input.length);
    Bytes hostOut(input.length);
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    const std::int32_t guestResult = guestSwap(ds, guestIn.data(), length, inPlace ? guestIn.data() : guestOut.data(), &status);
    const std::int32_t hostResult = hostSwap(host, hostIn.data(), length, inPlace ? hostIn.data() : hostOut.data(), &hostStatus);
    Require(guestResult == hostResult && status == hostStatus && Same(guestIn, hostIn, input.length) && Same(guestOut, hostOut, input.length), name + " against ICU");
    udata_closeSwapper(host);
    udata_closeSwapper_nid_postfix(ds);
    return status;
}

template<typename TGuest, typename THost>
std::int32_t AgainstIcu(const Bytes& input, std::int32_t length, bool inPlace, TGuest guestSwap, THost hostSwap, const std::string& name) {
    return AgainstIcuAs(0, input, length, inPlace, guestSwap, hostSwap, name);
}

template<typename TGuest>
bool ThrowsAs(std::uint8_t outCharset, const Bytes& input, std::int32_t length, bool inPlace, TGuest guestSwap) {
    GuestDataSwapper* ds = Swapper(false, 0, true, outCharset);
    Bytes in = input;
    Bytes out(input.length);
    std::int32_t status = Ok;
    const bool thrown = Invalid([&] { guestSwap(ds, in.data(), length, inPlace ? in.data() : out.data(), &status); });
    udata_closeSwapper_nid_postfix(ds);
    return thrown;
}

template<typename TGuest>
bool Throws(const Bytes& input, std::int32_t length, bool inPlace, TGuest guestSwap) {
    GuestDataSwapper* ds = Swapper(false, 0, true, 0);
    Bytes in = input;
    Bytes out(input.length);
    std::int32_t status = Ok;
    const bool thrown = Invalid([&] { guestSwap(ds, in.data(), length, inPlace ? in.data() : out.data(), &status); });
    udata_closeSwapper_nid_postfix(ds);
    return thrown;
}

template<typename TGuest, typename THost>
void FormatMessage(TGuest guestSwap, THost hostSwap, const Bytes& input, const std::string& expected, const std::string& name) {
    GuestDataSwapper* ds = Swapper(false, 0, true, 0);
    UDataSwapper* host = HostSwapper(false, 0, true, 0);
    const std::string suffix = "|";
    ds->printError = GuestPrintError;
    ds->printErrorContext = const_cast<std::string*>(&suffix);
    host->printError = HostPrintError;
    Bytes out(input.length);
    std::int32_t status = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    printed.clear();
    hostPrinted.clear();
    Require(guestSwap(ds, input.data(), input.length, out.data(), &status) == 0 && status == U_UNSUPPORTED_ERROR, name + " other data");
    hostSwap(host, input.data(), input.length, out.data(), &hostStatus);
    Require(hostStatus == U_UNSUPPORTED_ERROR && printed == hostPrinted + "|" && printed.find(expected) != std::string::npos, name + " format message");
    udata_closeSwapper(host);
    udata_closeSwapper_nid_postfix(ds);
}

void TestBreakSwap() {
    for (const char* name : {"char", "word", "line", "line_cj", "sent", "title"}) RoundTrip(Item("ICUDATA-brkitr", "brk", name), ubrk_swap_nid_postfix, ubrk_swap, std::string("ubrk_swap ") + name);
    const Bytes rules = Item("ICUDATA-brkitr", "brk", "word");
    const Bytes flipped = Mutated(rules, [](std::uint8_t* b) {
        for (const std::uint64_t field : {offsetof(icu::RBBIDataHeader, fFTable), offsetof(icu::RBBIDataHeader, fRTable)}) {
            const std::uint64_t flags = GetU32(b, field) + offsetof(icu::RBBIStateTable, fFlags);
            if (GetU32(b, field + 4) != 0) PutU32(b, flags, GetU32(b, flags) ^ icu::RBBI_8BITS_ROWS);
        }
    });
    Require(AgainstIcu(flipped, flipped.length, false, ubrk_swap_nid_postfix, ubrk_swap, "ubrk_swap other row width") == Ok, "ubrk_swap other row width");
    Require(AgainstIcu(flipped, flipped.length, true, ubrk_swap_nid_postfix, ubrk_swap, "ubrk_swap other row width in place") == Ok, "ubrk_swap other row width in place");
    const Bytes word = Item("ICUDATA-brkitr", "brk", "word");
    const Bytes names = Item(nullptr, "icu", "unames");
    GuestDataSwapper* ds = Swapper(false, 0, true, 0);
    Bytes out(word.length);
    Require(ubrk_swap_nid_postfix(ds, word.data(), word.length, out.data(), nullptr) == 0, "ubrk_swap NULL status");
    std::int32_t status = Ok;
    Require(ubrk_swap_nid_postfix(ds, nullptr, 20, out.data(), &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "ubrk_swap NULL data");
    status = Ok;
    Require(ubrk_swap_nid_postfix(ds, word.data(), 20, nullptr, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "ubrk_swap NULL output");
    status = Ok;
    Require(ubrk_swap_nid_postfix(ds, word.data(), -2, out.data(), &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "ubrk_swap length below -1");
    udata_closeSwapper_nid_postfix(ds);
    FormatMessage(ubrk_swap_nid_postfix, ubrk_swap, names, "75.6e.61.6d (format version 01)", "ubrk_swap");

    const std::int32_t header = Header(word);
    const auto available = static_cast<std::uint32_t>(word.length - header);
    const std::uint32_t forward = GetU32(word.data() + header, offsetof(icu::RBBIDataHeader, fFTable));
    const std::uint32_t forwardFlags = GetU32(word.data() + header, forward + offsetof(icu::RBBIStateTable, fFlags));
    const std::uint32_t trie = GetU32(word.data() + header, offsetof(icu::RBBIDataHeader, fTrie));
    const std::uint32_t ruleSource = GetU32(word.data() + header, offsetof(icu::RBBIDataHeader, fRuleSource));
    const std::uint32_t statusTable = GetU32(word.data() + header, offsetof(icu::RBBIDataHeader, fStatusTable));
    const auto swap = ubrk_swap_nid_postfix;
    Require(Throws(word, 16, false, swap), "ubrk_swap data format past the length");
    Require(AgainstIcu(word, 17, false, swap, ubrk_swap, "ubrk_swap 17 bytes") == U_UNSUPPORTED_ERROR, "ubrk_swap 17 bytes reads the data header as break data");
    Require(AgainstIcu(word, header + 100, false, swap, ubrk_swap, "ubrk_swap short") == U_INDEX_OUTOFBOUNDS_ERROR, "ubrk_swap short break data");
    Require(AgainstIcu(Mutated(word, [](std::uint8_t* b) { b[offsetof(icu::RBBIDataHeader, fFormatVersion)] = 5; }), word.length, false, swap, ubrk_swap, "ubrk_swap version") == U_UNSUPPORTED_ERROR, "ubrk_swap break header version");
    Require(AgainstIcu(Mutated(word, [](std::uint8_t* b) { PutU32(b, offsetof(icu::RBBIDataHeader, fLength), sizeof(icu::RBBIDataHeader) - 1); }), word.length, false, swap, ubrk_swap, "ubrk_swap small length") == U_UNSUPPORTED_ERROR, "ubrk_swap break length below the header");
    Require(Throws(Mutated(word, [](std::uint8_t* b) { PutU32(b, offsetof(icu::RBBIDataHeader, fLength), 0x80000000u); }), word.length, false, swap), "ubrk_swap negative break length");
    Require(Throws(Mutated(word, [&](std::uint8_t* b) { PutU32(b, offsetof(icu::RBBIDataHeader, fFTable), available - 8); }), word.length, false, swap), "ubrk_swap table header past the data");
    Require(Throws(Mutated(word, [](std::uint8_t* b) { PutU32(b, offsetof(icu::RBBIDataHeader, fRTable), 0xfffffffcu); PutU32(b, offsetof(icu::RBBIDataHeader, fRTableLen), 40); }), word.length, false, swap), "ubrk_swap table before the data");
    const Bytes shortRows = Mutated(word, [&](std::uint8_t* b) {
        PutU32(b, forward + offsetof(icu::RBBIStateTable, fFlags), forwardFlags | icu::RBBI_8BITS_ROWS);
        PutU32(b, offsetof(icu::RBBIDataHeader, fFTableLen), 8);
    });
    Require(Throws(shortRows, word.length, false, swap), "ubrk_swap 8-bit table shorter than its header");
    Require(AgainstIcu(shortRows, word.length, true, swap, ubrk_swap, "ubrk_swap short 8-bit table in place") == Ok, "ubrk_swap in place copies no 8-bit rows");
    Require(Throws(Mutated(word, [&](std::uint8_t* b) {
        PutU32(b, forward + offsetof(icu::RBBIStateTable, fFlags), forwardFlags | icu::RBBI_8BITS_ROWS);
        PutU32(b, offsetof(icu::RBBIDataHeader, fFTableLen), available - forward + 1);
    }), word.length, false, swap), "ubrk_swap 8-bit rows past the data");
    const auto wideFlags = forwardFlags & ~icu::RBBI_8BITS_ROWS;
    const std::uint32_t wideLength = available - forward + 2 + ((available - forward) & 1);
    Require(Throws(Mutated(word, [&](std::uint8_t* b) {
        PutU32(b, forward + offsetof(icu::RBBIStateTable, fFlags), wideFlags);
        PutU32(b, offsetof(icu::RBBIDataHeader, fFTableLen), wideLength);
    }), word.length, true, swap), "ubrk_swap 16-bit rows past the data");
    Require(AgainstIcu(Mutated(word, [&](std::uint8_t* b) {
        PutU32(b, forward + offsetof(icu::RBBIStateTable, fFlags), wideFlags);
        PutU32(b, offsetof(icu::RBBIDataHeader, fFTableLen), wideLength + 1);
    }), word.length, false, swap, ubrk_swap, "ubrk_swap odd rows") == U_ILLEGAL_ARGUMENT_ERROR, "ubrk_swap odd 16-bit rows keep ICU's error");
    Require(Throws(Mutated(word, [&](std::uint8_t* b) {
        PutU32(b, offsetof(icu::RBBIDataHeader, fTrie), available - 8);
        PutU32(b, offsetof(icu::RBBIDataHeader, fTrieLen), 0xffffffffu);
    }), word.length, false, swap), "ubrk_swap trie header past the data");
    Require(Throws(Mutated(word, [&](std::uint8_t* b) { PutU32(b, offsetof(icu::RBBIDataHeader, fTrieLen), available - trie + 4); }), word.length, false, swap), "ubrk_swap trie past the data");
    Require(AgainstIcu(Mutated(word, [](std::uint8_t* b) {
        PutU32(b, offsetof(icu::RBBIDataHeader, fTrie), 0x7ffffff0u);
        PutU32(b, offsetof(icu::RBBIDataHeader, fTrieLen), 8);
    }), word.length, false, swap, ubrk_swap, "ubrk_swap short trie") == U_INDEX_OUTOFBOUNDS_ERROR, "ubrk_swap short trie keeps ICU's error");
    const Bytes longRules = Mutated(word, [&](std::uint8_t* b) { PutU32(b, offsetof(icu::RBBIDataHeader, fRuleSourceLen), available - ruleSource + 1); });
    Require(Throws(longRules, word.length, false, swap), "ubrk_swap rule source past the data");
    Require(AgainstIcu(longRules, word.length, true, swap, ubrk_swap, "ubrk_swap long rules in place") == Ok, "ubrk_swap in place copies no rule source");
    Require(Throws(Mutated(word, [&](std::uint8_t* b) { PutU32(b, offsetof(icu::RBBIDataHeader, fStatusTableLen), ((available - statusTable) | 3) + 1); }), word.length, false, swap), "ubrk_swap status table past the data");
    Require(AgainstIcu(Mutated(word, [&](std::uint8_t* b) { PutU32(b, offsetof(icu::RBBIDataHeader, fStatusTableLen), 6); }), word.length, false, swap, ubrk_swap, "ubrk_swap uneven status table") == U_ILLEGAL_ARGUMENT_ERROR, "ubrk_swap uneven status table keeps ICU's error");

    Bytes bare(128);
    PutU32(bare.data(), 0, 0xb1a0);
    bare.data()[4] = icu::RBBI_DATA_FORMAT_VERSION[0];
    PutU32(bare.data(), 8, sizeof(icu::RBBIDataHeader));
    std::memcpy(bare.data() + 12, "Brk ", 4);
    bare.data()[16] = icu::RBBI_DATA_FORMAT_VERSION[0];
    Require(AgainstIcu(bare, bare.length, false, swap, ubrk_swap, "ubrk_swap without a data header") == U_UNSUPPORTED_ERROR, "ubrk_swap without a data header continues like ICU");
    Bytes bareRules = bare;
    PutU32(bareRules.data(), offsetof(icu::RBBIDataHeader, fRuleSource), 64);
    PutU32(bareRules.data(), offsetof(icu::RBBIDataHeader, fRuleSourceLen), 65);
    Require(Throws(bareRules, bareRules.length, false, swap), "ubrk_swap without a data header bounds the rule copy");
}

void TestNormalizerSwap() {
    for (const char* name : {"nfkc", "nfkc_cf", "uts46"}) RoundTrip(Item(nullptr, "nrm", name), unorm2_swap_nid_postfix, unorm2_swap, std::string("unorm2_swap ") + name);
    const Bytes nfkc = Item(nullptr, "nrm", "nfkc");
    const Bytes names = Item(nullptr, "icu", "unames");
    GuestDataSwapper* ds = Swapper(false, 0, true, 0);
    Bytes out(nfkc.length);
    Require(unorm2_swap_nid_postfix(ds, nfkc.data(), nfkc.length, out.data(), nullptr) == 0, "unorm2_swap NULL status");
    udata_closeSwapper_nid_postfix(ds);
    FormatMessage(unorm2_swap_nid_postfix, unorm2_swap, names, "75.6e.61.6d (format version 01) is not recognized as Normalizer2 data", "unorm2_swap");

    using Impl = icu::Normalizer2Impl;
    const std::int32_t header = Header(nfkc);
    const auto available = static_cast<std::uint32_t>(nfkc.length - header);
    const std::uint32_t extra = GetU32(nfkc.data() + header, Impl::IX_EXTRA_DATA_OFFSET * 4);
    const auto swap = unorm2_swap_nid_postfix;
    Require(AgainstIcu(nfkc, header + 40, false, swap, unorm2_swap, "unorm2_swap short") == U_INDEX_OUTOFBOUNDS_ERROR, "unorm2_swap short indexes");
    Require(AgainstIcu(Mutated(nfkc, [&](std::uint8_t* b) { PutU32(b, Impl::IX_TOTAL_SIZE * 4, available + 4); }), nfkc.length, false, swap, unorm2_swap, "unorm2_swap size") == U_INDEX_OUTOFBOUNDS_ERROR, "unorm2_swap size past the data keeps ICU's error");
    const Bytes negative = Mutated(nfkc, [](std::uint8_t* b) { PutU32(b, Impl::IX_TOTAL_SIZE * 4, 0xfffffffcu); });
    Require(Throws(negative, nfkc.length, false, swap), "unorm2_swap negative size");
    Require(AgainstIcu(negative, nfkc.length, true, swap, unorm2_swap, "unorm2_swap negative size in place") == Ok, "unorm2_swap in place copies nothing");
    Require(Throws(Mutated(nfkc, [&](std::uint8_t* b) { PutU32(b, Impl::IX_NORM_TRIE_OFFSET * 4, (available | 3) + 1); }), nfkc.length, false, swap), "unorm2_swap indexes past the data");
    Require(Throws(Mutated(nfkc, [&](std::uint8_t* b) { PutU32(b, Impl::IX_EXTRA_DATA_OFFSET * 4, available + 64); }), nfkc.length, false, swap), "unorm2_swap trie past the data");
    Require(AgainstIcu(Mutated(nfkc, [&](std::uint8_t* b) {
        PutU32(b, Impl::IX_NORM_TRIE_OFFSET * 4, 6);
        PutU32(b, Impl::IX_EXTRA_DATA_OFFSET * 4, available + 64);
    }), nfkc.length, false, swap, unorm2_swap, "unorm2_swap misaligned trie") == U_ILLEGAL_ARGUMENT_ERROR, "unorm2_swap misaligned trie keeps ICU's error");
    Require(Throws(Mutated(nfkc, [&](std::uint8_t* b) { PutU32(b, Impl::IX_SMALL_FCD_OFFSET * 4, available + 2 + ((available - extra) & 1)); }), nfkc.length, false, swap), "unorm2_swap extra data past the data");
    Require(AgainstIcu(Mutated(nfkc, [&](std::uint8_t* b) { PutU32(b, Impl::IX_SMALL_FCD_OFFSET * 4, extra + 3); }), nfkc.length, false, swap, unorm2_swap, "unorm2_swap odd extra data") == U_ILLEGAL_ARGUMENT_ERROR, "unorm2_swap odd extra data keeps ICU's error");
}

void TestCollationBinary() {
    const Bytes uca = Item("ICUDATA-coll", "icu", "ucadata");
    const Bytes word = Item("ICUDATA-brkitr", "brk", "word");
    const Bytes names = Item(nullptr, "icu", "unames");
    GuestDataSwapper* ds = Swapper(false, 0, true, 0);
    UDataSwapper* host = HostSwapper(false, 0, true, 0);
    const auto looks = [&](const Bytes& bytes, std::int32_t length) {
        const std::int8_t result = ucol_looksLikeCollationBinary_nid_postfix(ds, bytes.data(), length);
        Require(result == ucol_looksLikeCollationBinary(host, bytes.data(), length), "ucol_looksLikeCollationBinary against ICU");
        return result;
    };
    Require(looks(uca, uca.length) == 1 && looks(uca, -1) == 1, "ucol_looksLikeCollationBinary UCA data");
    Require(looks(names, names.length) == 0 && looks(word, 13) == 0, "ucol_looksLikeCollationBinary other data");
    Require(ucol_looksLikeCollationBinary_nid_postfix(nullptr, uca.data(), uca.length) == 0 && ucol_looksLikeCollationBinary_nid_postfix(ds, nullptr, 10) == 0 && ucol_looksLikeCollationBinary_nid_postfix(ds, uca.data(), -2) == 0, "ucol_looksLikeCollationBinary arguments");
    for (const std::int32_t length : {0, 2, 3, 10, 12, 15}) Require(Invalid([&] { ucol_looksLikeCollationBinary_nid_postfix(ds, uca.data(), length); }), "ucol_looksLikeCollationBinary data header past the length " + std::to_string(length));
    Bytes noMagic = uca;
    noMagic.data()[2] = 0;
    Require(looks(noMagic, 3) == 0, "ucol_looksLikeCollationBinary stops at a bad magic byte");
    Bytes smallHeader = uca;
    PutU16(smallHeader.data(), 0, 8);
    Require(looks(smallHeader, 11) == 0, "ucol_looksLikeCollationBinary stops at a bad header size");

    Bytes v3(42 * 4);
    PutU32(v3.data(), offsetof(UCATableHeader, size), 42 * 4);
    PutU32(v3.data(), offsetof(UCATableHeader, magic), UCOL_HEADER_MAGIC);
    v3.data()[offsetof(UCATableHeader, formatVersion)] = 3;
    Require(looks(v3, v3.length) == 1 && looks(v3, -1) == 1, "ucol_looksLikeCollationBinary format version 3");
    Require(looks(v3, v3.length - 1) == 0, "ucol_looksLikeCollationBinary format version 3 too short");
    Bytes v3Big = v3;
    v3Big.data()[offsetof(UCATableHeader, isBigEndian)] = 1;
    Require(looks(v3Big, v3Big.length) == 0, "ucol_looksLikeCollationBinary format version 3 endianness");
    udata_closeSwapper(host);
    udata_closeSwapper_nid_postfix(ds);
}


Bytes WithDataHeader(const char* format, std::uint8_t major, std::uint8_t minor, const Bytes& body) {
    Bytes bytes(32 + body.length);
    std::uint8_t* b = bytes.data();
    PutU16(b, 0, 32);
    b[2] = 0xda;
    b[3] = 0x27;
    PutU16(b, 4, sizeof(UDataInfo));
    b[10] = 2;
    std::memcpy(b + 12, format, 4);
    b[16] = major;
    b[17] = minor;
    std::memcpy(b + 32, body.data(), static_cast<std::size_t>(body.length));
    return bytes;
}

constexpr std::uint32_t CollationMapping = 192;

Bytes CollationVersion3() {
    const Bytes trie = Trie1Bytes();
    const std::uint32_t endExpansion = CollationMapping + static_cast<std::uint32_t>(trie.length);
    const std::uint32_t ucaConsts = endExpansion + 8;
    const std::uint32_t combos = ucaConsts + 8;
    const std::uint32_t scripts = combos + 4;
    const std::uint32_t leads = scripts + 12;
    const std::uint32_t size = leads + 8;
    Bytes bytes(static_cast<std::int32_t>(size));
    std::uint8_t* b = bytes.data();
    for (std::uint32_t i = sizeof(UCATableHeader); i < size; ++i) b[i] = static_cast<std::uint8_t>(i * 7 + 1);
    std::memcpy(b + CollationMapping, trie.data(), static_cast<std::size_t>(trie.length));
    PutU32(b, offsetof(UCATableHeader, size), size);
    PutU32(b, offsetof(UCATableHeader, options), sizeof(UCATableHeader));
    PutU32(b, offsetof(UCATableHeader, UCAConsts), ucaConsts);
    PutU32(b, offsetof(UCATableHeader, contractionUCACombos), combos);
    PutU32(b, offsetof(UCATableHeader, magic), UCOL_HEADER_MAGIC);
    PutU32(b, offsetof(UCATableHeader, mappingPosition), CollationMapping);
    PutU32(b, offsetof(UCATableHeader, expansion), sizeof(UCATableHeader) + 4);
    PutU32(b, offsetof(UCATableHeader, contractionIndex), sizeof(UCATableHeader) + 12);
    PutU32(b, offsetof(UCATableHeader, contractionCEs), sizeof(UCATableHeader) + 16);
    PutU32(b, offsetof(UCATableHeader, contractionSize), 2);
    PutU32(b, offsetof(UCATableHeader, endExpansionCE), endExpansion);
    PutU32(b, offsetof(UCATableHeader, endExpansionCECount), 2);
    PutU32(b, offsetof(UCATableHeader, contractionUCACombosSize), 1);
    b[offsetof(UCATableHeader, contractionUCACombosWidth)] = 2;
    b[offsetof(UCATableHeader, formatVersion)] = 3;
    PutU32(b, offsetof(UCATableHeader, scriptToLeadByte), scripts);
    PutU32(b, offsetof(UCATableHeader, leadByteToScript), leads);
    PutU16(b, scripts, 1);
    PutU16(b, scripts + 2, 1);
    PutU16(b, leads, 1);
    PutU16(b, leads + 2, 1);
    return bytes;
}

void TestCollationSwap() {
    const Bytes uca = Item("ICUDATA-coll", "icu", "ucadata");
    const Bytes names = Item(nullptr, "icu", "unames");
    const Bytes v3 = CollationVersion3();
    RoundTrip(uca, ucol_swap_nid_postfix, ucol_swap, "ucol_swap UCA");
    RoundTrip(v3, ucol_swap_nid_postfix, ucol_swap, "ucol_swap format version 3");
    RoundTrip(WithDataHeader("UCol", 3, 0, v3), ucol_swap_nid_postfix, ucol_swap, "ucol_swap format version 3 with a data header");
    GuestDataSwapper* ds = Swapper(false, 0, true, 0);
    Bytes out(uca.length);
    Require(Invalid([&] { ucol_swap_nid_postfix(ds, uca.data(), uca.length, out.data(), nullptr); }), "ucol_swap NULL status");
    udata_closeSwapper_nid_postfix(ds);
    FormatMessage(ucol_swap_nid_postfix, ucol_swap, names, "75.6e.61.6d (format version 01.", "ucol_swap");
    const auto swap = ucol_swap_nid_postfix;

    const std::int32_t header = Header(uca);
    const auto available = static_cast<std::uint32_t>(uca.length - header);
    const std::uint8_t* base = uca.data() + header;
    const std::uint32_t indexesLength = GetU32(base, 0);
    Require(indexesLength > 5, "UCA data indexes");
    const std::uint32_t sizeSlot = indexesLength > 19 ? 19 : indexesLength - 1;
    const auto slot = [](std::uint32_t index) {
        return static_cast<std::uint64_t>(index) * 4;
    };
    Require(Throws(Mutated(uca, [](std::uint8_t* b) { PutU32(b, 0, 0xffffffffu); }), uca.length, false, swap), "ucol_swap negative index count");
    Require(Throws(Mutated(uca, [](std::uint8_t* b) { PutU32(b, 0, 0xffffffffu); }), uca.length, true, swap), "ucol_swap negative index count in place");
    Require(Throws(Mutated(uca, [](std::uint8_t* b) { PutU32(b, 0, 0x40000002u); }), header + 16, false, swap), "ucol_swap indexes past the data");
    Require(AgainstIcu(uca, header + 4, false, swap, ucol_swap, "ucol_swap short") == U_INDEX_OUTOFBOUNDS_ERROR, "ucol_swap short collation data");
    const Bytes negative = Mutated(uca, [&](std::uint8_t* b) { PutU32(b, slot(sizeSlot), 0xfffffff8u); });
    Require(Throws(negative, uca.length, false, swap), "ucol_swap negative size");
    Require(AgainstIcu(negative, uca.length, true, swap, ucol_swap, "ucol_swap negative size in place") == Ok, "ucol_swap in place copies nothing");
    Require(AgainstIcu(Mutated(uca, [&](std::uint8_t* b) { PutU32(b, slot(sizeSlot), available + 8); }), uca.length, false, swap, ucol_swap, "ucol_swap size") == U_INDEX_OUTOFBOUNDS_ERROR, "ucol_swap size past the data keeps ICU's error");
    const std::uint32_t ces = GetU32(base, slot(9));
    Require(Throws(Mutated(uca, [&](std::uint8_t* b) { PutU32(b, slot(10), ces + (((available - ces) | 7) + 9)); }), uca.length, false, swap), "ucol_swap CEs past the data");
    Require(Throws(Mutated(uca, [&](std::uint8_t* b) {
        PutU32(b, slot(8), available + 64);
        PutU32(b, slot(9), available + 64);
    }), uca.length, false, swap), "ucol_swap trie past the data");
    Require(AgainstIcu(Mutated(uca, [&](std::uint8_t* b) {
        PutU32(b, slot(9), GetU32(b, slot(8)) + 8);
        PutU32(b, slot(13), available + 64);
    }), uca.length, false, swap, ucol_swap, "ucol_swap unknown data") == U_UNSUPPORTED_ERROR, "ucol_swap unknown data stops before later sections");
    Require(AgainstIcu(Mutated(uca, [&](std::uint8_t* b) { PutU32(b, slot(14), GetU32(b, slot(13)) + 3); }), uca.length, false, swap, ucol_swap, "ucol_swap odd contexts") == U_ILLEGAL_ARGUMENT_ERROR, "ucol_swap odd contexts keep ICU's error");

    const auto version3 = [&](auto change) {
        Bytes bytes = v3;
        change(bytes.data());
        return bytes;
    };
    const auto all = static_cast<std::uint32_t>(v3.length);
    Require(Throws(version3([&](std::uint8_t* b) { PutU32(b, offsetof(UCATableHeader, size), 0xfffffff0u); }), v3.length, false, swap), "ucol_swap format version 3 negative size");
    Require(Throws(version3([&](std::uint8_t* b) { PutU32(b, offsetof(UCATableHeader, expansion), all + 4); }), v3.length, false, swap), "ucol_swap format version 3 options past the data");
    Require(Throws(version3([&](std::uint8_t* b) { PutU32(b, offsetof(UCATableHeader, contractionIndex), all + 8); }), v3.length, false, swap), "ucol_swap format version 3 expansions past the data");
    Require(Throws(version3([&](std::uint8_t* b) { PutU32(b, offsetof(UCATableHeader, contractionCEs), all - 4); }), v3.length, false, swap), "ucol_swap format version 3 contraction CEs past the data");
    Require(Throws(version3([&](std::uint8_t* b) { PutU32(b, offsetof(UCATableHeader, mappingPosition), all - 8); }), v3.length, false, swap), "ucol_swap format version 3 trie header past the data");
    Require(AgainstIcu(version3([&](std::uint8_t* b) { PutU32(b, offsetof(UCATableHeader, endExpansionCE), CollationMapping + 8); }), v3.length, false, swap, ucol_swap, "ucol_swap format version 3 short trie") == U_INDEX_OUTOFBOUNDS_ERROR, "ucol_swap format version 3 short trie keeps ICU's error");
    Require(Throws(version3([&](std::uint8_t* b) { PutU32(b, offsetof(UCATableHeader, endExpansionCECount), all / 4); }), v3.length, false, swap), "ucol_swap format version 3 expansion ends past the data");
    Require(Throws(version3([&](std::uint8_t* b) { PutU32(b, offsetof(UCATableHeader, contractionUCACombos), all + 8); }), v3.length, false, swap), "ucol_swap format version 3 UCA constants past the data");
    Require(Throws(version3([&](std::uint8_t* b) { PutU32(b, offsetof(UCATableHeader, contractionUCACombosSize), all); }), v3.length, false, swap), "ucol_swap format version 3 UCA contractions past the data");
    Require(Throws(version3([&](std::uint8_t* b) { PutU32(b, offsetof(UCATableHeader, scriptToLeadByte), all - 2); }), v3.length, false, swap), "ucol_swap format version 3 script counts past the data");
    Require(Throws(version3([&](std::uint8_t* b) { PutU16(b, GetU32(b, offsetof(UCATableHeader, scriptToLeadByte)), 2000); }), v3.length, false, swap), "ucol_swap format version 3 script table past the data");
    Require(Throws(version3([&](std::uint8_t* b) { PutU16(b, GetU32(b, offsetof(UCATableHeader, leadByteToScript)), 3000); }), v3.length, false, swap), "ucol_swap format version 3 lead byte table past the data");
    Require(AgainstIcu(version3([&](std::uint8_t* b) { PutU32(b, offsetof(UCATableHeader, expansion), sizeof(UCATableHeader) + 2); }), v3.length, false, swap, ucol_swap, "ucol_swap format version 3 uneven options") == U_ILLEGAL_ARGUMENT_ERROR, "ucol_swap format version 3 uneven options keep ICU's error");
    Require(AgainstIcu(version3([](std::uint8_t* b) { b[offsetof(UCATableHeader, isBigEndian)] = 1; }), v3.length, false, swap, ucol_swap, "ucol_swap format version 3 endianness") == U_INVALID_FORMAT_ERROR, "ucol_swap format version 3 endianness");
    Require(AgainstIcu(version3([](std::uint8_t* b) { PutU32(b, offsetof(UCATableHeader, magic), 1); }), v3.length, false, swap, ucol_swap, "ucol_swap format version 3 magic") == U_UNSUPPORTED_ERROR, "ucol_swap format version 3 magic");
    Require(AgainstIcu(v3, sizeof(UCATableHeader) - 1, false, swap, ucol_swap, "ucol_swap format version 3 short") == U_INDEX_OUTOFBOUNDS_ERROR, "ucol_swap format version 3 short header");
}

Bytes InverseBody() {
    Bytes body(80);
    std::uint8_t* b = body.data();
    PutU32(b, offsetof(InverseUCATableHeader, byteSize), 76);
    PutU32(b, offsetof(InverseUCATableHeader, tableSize), 3);
    PutU32(b, offsetof(InverseUCATableHeader, contsSize), 4);
    PutU32(b, offsetof(InverseUCATableHeader, table), sizeof(InverseUCATableHeader));
    PutU32(b, offsetof(InverseUCATableHeader, conts), sizeof(InverseUCATableHeader) + 36);
    b[offsetof(InverseUCATableHeader, UCAVersion)] = 13;
    for (std::uint32_t i = sizeof(InverseUCATableHeader); i < 76; ++i) b[i] = static_cast<std::uint8_t>(i * 5 + 3);
    return body;
}

void TestInverseSwap() {
    const Bytes inverse = WithDataHeader("InvC", 2, 1, InverseBody());
    const Bytes names = Item(nullptr, "icu", "unames");
    RoundTrip(inverse, ucol_swapInverseUCA_nid_postfix, ucol_swapInverseUCA, "ucol_swapInverseUCA");
    GuestDataSwapper* ds = Swapper(false, 0, true, 0);
    Bytes out(inverse.length);
    Require(ucol_swapInverseUCA_nid_postfix(ds, inverse.data(), inverse.length, out.data(), nullptr) == 0, "ucol_swapInverseUCA NULL status");
    udata_closeSwapper_nid_postfix(ds);
    FormatMessage(ucol_swapInverseUCA_nid_postfix, ucol_swapInverseUCA, names, "75.6e.61.6d (format version 01.", "ucol_swapInverseUCA");
    const auto swap = ucol_swapInverseUCA_nid_postfix;
    Require(AgainstIcu(WithDataHeader("InvC", 2, 0, InverseBody()), inverse.length, false, swap, ucol_swapInverseUCA, "ucol_swapInverseUCA minor version") == U_UNSUPPORTED_ERROR, "ucol_swapInverseUCA format version 2.0");
    Require(AgainstIcu(inverse, 32 + 16, false, swap, ucol_swapInverseUCA, "ucol_swapInverseUCA short") == U_INDEX_OUTOFBOUNDS_ERROR, "ucol_swapInverseUCA short header");
    Require(AgainstIcu(Mutated(inverse, [](std::uint8_t* b) { PutU32(b, offsetof(InverseUCATableHeader, byteSize), 81); }), inverse.length, false, swap, ucol_swapInverseUCA, "ucol_swapInverseUCA size") == U_INDEX_OUTOFBOUNDS_ERROR, "ucol_swapInverseUCA size past the data keeps ICU's error");
    Require(Throws(Mutated(inverse, [](std::uint8_t* b) { PutU32(b, offsetof(InverseUCATableHeader, tableSize), 100); }), inverse.length, false, swap), "ucol_swapInverseUCA table past the data");
    Require(Throws(Mutated(inverse, [](std::uint8_t* b) { PutU32(b, offsetof(InverseUCATableHeader, conts), 76); }), inverse.length, false, swap), "ucol_swapInverseUCA continuations past the data");
    Require(AgainstIcu(Mutated(inverse, [](std::uint8_t* b) {
        PutU32(b, offsetof(InverseUCATableHeader, tableSize), 0);
        PutU32(b, offsetof(InverseUCATableHeader, table), 0x7fffff00u);
    }), inverse.length, false, swap, ucol_swapInverseUCA, "ucol_swapInverseUCA empty table") == Ok, "ucol_swapInverseUCA empty table ignores its offset");
}

constexpr std::uint32_t Res(std::uint32_t type, std::uint32_t word) {
    return (type << 28) | word;
}

constexpr std::size_t TinyString = 14;
constexpr std::size_t TinyVector = 17;
constexpr std::size_t TinyArray = 20;
constexpr std::size_t TinyBinary = 23;

using Words = std::vector<std::uint32_t>;

void AddKeys(Words& words) {
    const char keys[] = "%%CollationBin\0k1\0k2\0\xaa\xaa\xaa";
    static_assert(sizeof(keys) == 25);
    for (std::size_t i = 0; i < 24; i += 4) words.push_back(GetU32(reinterpret_cast<const std::uint8_t*>(keys), i));
}

void SetIndexes(Words& words, std::uint32_t root, std::uint32_t maxTableLength) {
    const auto top = static_cast<std::uint32_t>(words.size());
    words[0] = Res(URES_TABLE, root);
    words[1] = 7;
    words[URES_INDEX_KEYS_TOP + 1] = 14;
    words[URES_INDEX_RESOURCES_TOP + 1] = top;
    words[URES_INDEX_BUNDLE_TOP + 1] = top;
    words[URES_INDEX_MAX_TABLE_LENGTH + 1] = maxTableLength;
    words[URES_INDEX_16BIT_TOP + 1] = 14;
}

Words TinyWords() {
    const Bytes v3 = CollationVersion3();
    Words w(8, 0);
    AddKeys(w);
    w.insert(w.end(), {2u, 0x68u | (0x69u << 16), 0u});
    w.insert(w.end(), {2u, 7u, 9u});
    w.insert(w.end(), {2u, Res(URES_STRING, TinyString), Res(URES_INT_VECTOR, TinyVector)});
    w.push_back(static_cast<std::uint32_t>(v3.length));
    for (std::int32_t i = 0; i < v3.length; i += 4) w.push_back(GetU32(v3.data(), static_cast<std::uint64_t>(i)));
    const auto root = static_cast<std::uint32_t>(w.size());
    w.insert(w.end(), {3u | (32u << 16), 47u | (50u << 16), Res(URES_BINARY, TinyBinary), Res(URES_ARRAY, TinyArray), Res(URES_STRING, TinyString)});
    SetIndexes(w, root, 3);
    return w;
}

Words WideWords(std::uint32_t count, std::uint32_t maxTableLength) {
    Words w(8, 0);
    AddKeys(w);
    w.insert(w.end(), {2u, 0x68u | (0x69u << 16), 0u});
    const auto root = static_cast<std::uint32_t>(w.size());
    std::vector<std::uint16_t> keys(count + 1 + (count + 1) % 2, 47);
    keys[0] = static_cast<std::uint16_t>(count);
    for (std::size_t i = 0; i < keys.size(); i += 2) w.push_back(keys[i] | (static_cast<std::uint32_t>(keys[i + 1]) << 16));
    w.insert(w.end(), count, Res(URES_STRING, TinyString));
    SetIndexes(w, root, maxTableLength);
    return w;
}

Bytes BundleOf(const Words& words, std::uint8_t major) {
    Bytes body(static_cast<std::int32_t>(words.size() * 4));
    std::memcpy(body.data(), words.data(), words.size() * 4);
    return WithDataHeader("ResB", major, major == 1 ? 2 : 0, body);
}

template<typename TChange>
Bytes Tiny(TChange change, std::uint8_t major = 2) {
    Words words = TinyWords();
    change(words);
    return BundleOf(words, major);
}

Bytes AsVersion1(const Bytes& bundle) {
    Bytes bytes = bundle;
    bytes.data()[16] = 1;
    bytes.data()[17] = 2;
    return bytes;
}

void TestBundleSwap() {
    const auto swap = ures_swap_nid_postfix;
    for (const auto& [path, name] : {std::pair<const char*, const char*>{nullptr, "root"}, {nullptr, "pool"}, {"ICUDATA-coll", "de"}, {"ICUDATA-coll", "root"}})
        RoundTrip(Item(path, "res", name), swap, ures_swap, std::string("ures_swap ") + name);
    const auto none = [](Words&) {};
    const Bytes tiny = Tiny(none);
    RoundTrip(tiny, swap, ures_swap, "ures_swap crafted bundle");
    const Bytes de = Item("ICUDATA-coll", "res", "de");
    const Bytes root = Item(nullptr, "res", "root");
    for (const bool inPlace : {false, true}) {
        Require(AgainstIcuAs(1, root, root.length, inPlace, swap, ures_swap, "ures_swap root to EBCDIC") == Ok, "ures_swap root to EBCDIC");
        Require(AgainstIcuAs(1, de, de.length, inPlace, swap, ures_swap, "ures_swap de to EBCDIC") == Ok, "ures_swap de to EBCDIC");
        Require(AgainstIcuAs(1, AsVersion1(de), de.length, inPlace, swap, ures_swap, "ures_swap sorted de") == Ok, "ures_swap sorts format version 1 tables by EBCDIC keys");
        Require(AgainstIcuAs(1, Tiny(none, 1), tiny.length, inPlace, swap, ures_swap, "ures_swap sorted crafted bundle") == Ok, "ures_swap sorts the crafted bundle");
        const Bytes unknownKeys = Mutated(de, [](std::uint8_t* b) { PutU32(b, 4 + 4 * URES_INDEX_KEYS_TOP, (GetU32(b, 4) & 0xff) + 1); });
        AgainstIcu(unknownKeys, unknownKeys.length, inPlace, swap, ures_swap, "ures_swap collation binary found by its data");
    }
    const auto unknown = [](Words& w) { w[w[0] & 0x0fffffff] = 3u | (0xffffu << 16); };
    Require(AgainstIcu(Tiny(unknown), tiny.length, false, swap, ures_swap, "ures_swap unknown key") == Ok, "ures_swap swaps a collation binary under an unknown key");
    Require(AgainstIcu(Tiny([&](Words& w) { unknown(w); w[TinyBinary + 1 + offsetof(UCATableHeader, magic) / 4] = 1; }), tiny.length, false, swap, ures_swap, "ures_swap other binary") == Ok, "ures_swap leaves other binaries");
    GuestDataSwapper* ds = Swapper(false, 0, true, 0);
    Bytes out(tiny.length);
    Require(ures_swap_nid_postfix(ds, tiny.data(), tiny.length, out.data(), nullptr) == 0, "ures_swap NULL status");
    std::int32_t status = Ok;
    Require(ures_swap_nid_postfix(ds, nullptr, 20, out.data(), &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "ures_swap NULL data");
    udata_closeSwapper_nid_postfix(ds);
    FormatMessage(swap, ures_swap, Item(nullptr, "icu", "unames"), "75.6e.61.6d (format version 01.", "ures_swap");

    const std::int32_t header = Header(tiny);
    const auto top = static_cast<std::uint32_t>(TinyWords().size());
    const auto with = [&](auto change) { return Tiny(change); };
    Require(AgainstIcu(tiny, header + 20, false, swap, ures_swap, "ures_swap short") == U_INDEX_OUTOFBOUNDS_ERROR, "ures_swap too few bytes");
    Require(AgainstIcu(with([](Words& w) { w[1] = 4; }), tiny.length, false, swap, ures_swap, "ures_swap indexes") == U_INDEX_OUTOFBOUNDS_ERROR, "ures_swap too few indexes");
    Require(AgainstIcu(with([&](Words& w) { w[URES_INDEX_BUNDLE_TOP + 1] = top + 1; }), tiny.length, false, swap, ures_swap, "ures_swap top") == U_INDEX_OUTOFBOUNDS_ERROR, "ures_swap top past the data keeps ICU's error");
    Require(AgainstIcu(with([](Words& w) { w[URES_INDEX_KEYS_TOP + 1] = 7; }), tiny.length, false, swap, ures_swap, "ures_swap keys") == U_ILLEGAL_ARGUMENT_ERROR, "ures_swap keys below the indexes keep ICU's error");
    Require(AgainstIcu(with([](Words& w) { w[URES_INDEX_16BIT_TOP + 1] = 14 + 0x20000000; }), tiny.length, false, swap, ures_swap, "ures_swap units") == U_ILLEGAL_ARGUMENT_ERROR, "ures_swap negative 16-bit units keep ICU's error");
    Require(AgainstIcu(with([](Words& w) { w[TinyArray + 1] = Res(10, TinyString); }), tiny.length, false, swap, ures_swap, "ures_swap type") == U_UNSUPPORTED_ERROR, "ures_swap unknown resource type");
    Require(AgainstIcu(with([](Words& w) { w[TinyString] = 0xc0000000u; }), tiny.length, false, swap, ures_swap, "ures_swap string") == U_ILLEGAL_ARGUMENT_ERROR, "ures_swap negative string length keeps ICU's error");
    Require(AgainstIcu(with([](Words& w) { w[TinyVector] = 0xffffffffu; }), tiny.length, false, swap, ures_swap, "ures_swap vector") == Ok, "ures_swap empty int vector");
    Require(AgainstIcu(with([](Words& w) { w[TinyBinary] = 0xfffffffeu; }), tiny.length, false, swap, ures_swap, "ures_swap binary") == U_ILLEGAL_ARGUMENT_ERROR, "ures_swap collation binary below -1 keeps ICU's error");
    Require(AgainstIcu(with([](Words& w) { w[1] = 6; }), header + 28, false, swap, ures_swap, "ures_swap six indexes") == U_INDEX_OUTOFBOUNDS_ERROR, "ures_swap six indexes read no 16-bit top");

    Require(Throws(tiny, header + 28, false, swap), "ures_swap 16-bit top index past the data");
    Require(Throws(tiny, 0x7fffffe1, false, swap), "ures_swap length ICU overflows");
    const Bytes negativeTop = with([](Words& w) { w[URES_INDEX_BUNDLE_TOP + 1] = 0xffffffffu; });
    Require(Throws(negativeTop, tiny.length, false, swap), "ures_swap negative top");
    Require(AgainstIcu(negativeTop, tiny.length, true, swap, ures_swap, "ures_swap negative top in place") == Ok, "ures_swap in place copies nothing");
    Require(Throws(with([&](Words& w) { w[URES_INDEX_KEYS_TOP + 1] = top + 1; }), tiny.length, false, swap), "ures_swap keys past the data");
    Require(Throws(with([&](Words& w) { w[URES_INDEX_16BIT_TOP + 1] = top + 1; }), tiny.length, false, swap), "ures_swap 16-bit units past the data");
    Require(Throws(with([](Words& w) { w[URES_INDEX_KEYS_TOP + 1] = 8 - 0x40000000; }), tiny.length, false, swap), "ures_swap 16-bit units before the data");
    Require(Throws(with([](Words& w) { w[TinyString] = 0x10000; }), tiny.length, false, swap), "ures_swap string past the data");
    Require(Throws(with([&](Words& w) { w[TinyArray + 1] = Res(URES_STRING, top + 5); }), tiny.length, false, swap), "ures_swap resource past the data");
    Require(Throws(with([&](Words& w) { w[TinyArray + 1] = Res(URES_TABLE, top); }), tiny.length, false, swap), "ures_swap table count past the data");
    Require(Throws(with([](Words& w) { w[TinyArray] = 0x100000; }), tiny.length, false, swap), "ures_swap array past the data");
    Require(Throws(with([](Words& w) { w[TinyVector] = 0x100000; }), tiny.length, false, swap), "ures_swap int vector past the data");
    Require(Throws(with([](Words& w) { w[TinyBinary] += 0x10000; }), tiny.length, false, swap), "ures_swap collation binary past the data");
    Require(Throws(with([](Words& w) { w[TinyBinary] = 0xffffffffu; }), tiny.length, false, swap), "ures_swap collation binary without a length");
    Require(Throws(with([](Words& w) { w[TinyBinary + 1 + offsetof(UCATableHeader, expansion) / 4] = 0x100000; }), tiny.length, false, swap), "ures_swap checks the collation binary");
    Require(Throws(with([](Words& w) { w[w[0] & 0x0fffffff] = 3u | (4u << 16); }), tiny.length, false, swap), "ures_swap key outside the key strings");
    Require(Throws(with([](Words& w) { for (std::size_t i = 8; i < 14; ++i) w[i] = 0x61616161u; }), tiny.length, false, swap), "ures_swap key without a NUL");
    Require(Throws(with([&](Words& w) { unknown(w); w[TinyBinary] = 0xffffffffu; }), tiny.length, false, swap), "ures_swap unknown key binary without a length");

    const auto farKey = [](Words& w) { w[(w[0] & 0x0fffffff) + 1] = 4u | (50u << 16); };
    Require(AgainstIcuAs(1, Tiny(farKey), tiny.length, false, swap, ures_swap, "ures_swap unsorted far key") == Ok, "ures_swap format version 2 does not sort");
    Require(AgainstIcu(Tiny(farKey, 1), tiny.length, false, swap, ures_swap, "ures_swap same charset far key") == Ok, "ures_swap same charset does not sort");
    Require(ThrowsAs(1, Tiny(farKey, 1), tiny.length, false, swap), "ures_swap sorted key outside the key strings");
    const Bytes wide = BundleOf(WideWords(201, 200), 1);
    Require(ThrowsAs(1, wide, wide.length, false, swap), "ures_swap table longer than the sort rows");
    Require(AgainstIcuAs(1, BundleOf(WideWords(201, 200), 2), wide.length, false, swap, ures_swap, "ures_swap long table unsorted") == Ok, "ures_swap format version 2 long table");
    Require(AgainstIcuAs(1, BundleOf(WideWords(201, 201), 1), wide.length, true, swap, ures_swap, "ures_swap long table sorted") == Ok, "ures_swap sorts a table its maximum length allows");
}

void TestAliasSwap() {
    const auto swap = ucnv_swapAliases_nid_postfix;
    const Bytes aliases = Item(nullptr, "icu", "cnvalias");
    RoundTrip(aliases, swap, ucnv_swapAliases, "ucnv_swapAliases");
    for (const bool inPlace : {false, true}) Require(AgainstIcuAs(1, aliases, aliases.length, inPlace, swap, ucnv_swapAliases, "ucnv_swapAliases to EBCDIC") == Ok, "ucnv_swapAliases sorts by EBCDIC names");
    GuestDataSwapper* ds = Swapper(false, 0, true, 0);
    Bytes out(aliases.length);
    Require(ucnv_swapAliases_nid_postfix(ds, aliases.data(), aliases.length, out.data(), nullptr) == 0, "ucnv_swapAliases NULL status");
    udata_closeSwapper_nid_postfix(ds);
    FormatMessage(swap, ucnv_swapAliases, Item(nullptr, "icu", "unames"), "75.6e.61.6d (format version 01) is not an alias table", "ucnv_swapAliases");
    const std::int32_t header = Header(aliases);
    const std::uint8_t* base = aliases.data() + header;
    Require(GetU32(base, 0) == 9, "cnvalias has nine sections");
    Require(AgainstIcu(aliases, header + 35, false, swap, ucnv_swapAliases, "ucnv_swapAliases short") == U_INDEX_OUTOFBOUNDS_ERROR, "ucnv_swapAliases too few bytes");
    Require(AgainstIcu(Mutated(aliases, [](std::uint8_t* b) { PutU32(b, 0, 7); }), aliases.length, false, swap, ucnv_swapAliases, "ucnv_swapAliases sections") == U_INVALID_FORMAT_ERROR, "ucnv_swapAliases section count");
    Require(AgainstIcu(aliases, header + 40, false, swap, ucnv_swapAliases, "ucnv_swapAliases tables") == U_INDEX_OUTOFBOUNDS_ERROR, "ucnv_swapAliases tables past the length");
    Require(Throws(aliases, header + 36, false, swap), "ucnv_swapAliases ninth section size past the length");
    Require(AgainstIcu(Mutated(aliases, [](std::uint8_t* b) { PutU32(b, 0, 8); }), header + 36, false, swap, ucnv_swapAliases, "ucnv_swapAliases eight sections") == U_INDEX_OUTOFBOUNDS_ERROR, "ucnv_swapAliases eight sections read no ninth size");
    Require(Throws(Mutated(aliases, [](std::uint8_t* b) { PutU32(b, 28, GetU32(b, 28) + 0x40000000u); }), aliases.length, false, swap), "ucnv_swapAliases overflowing total size");
    const std::uint32_t aliasList = 2 * (1 + 9) + GetU32(base, 4) + GetU32(base, 8);
    const std::uint32_t strings = aliasList + GetU32(base, 12) + GetU32(base, 16) + GetU32(base, 20) + GetU32(base, 24) + GetU32(base, 28);
    const Bytes farString = Mutated(aliases, [&](std::uint8_t* b) { PutU16(b, 2ull * aliasList, 0xffff); });
    Require(AgainstIcu(farString, aliases.length, false, swap, ucnv_swapAliases, "ucnv_swapAliases far string unsorted") == Ok, "ucnv_swapAliases same charset does not sort");
    Require(ThrowsAs(1, farString, aliases.length, false, swap), "ucnv_swapAliases sorted name past the strings");
    const auto named = [&](std::size_t letters) {
        return Mutated(aliases, [&](std::uint8_t* b) {
            const std::uint64_t start = 2ull * strings + 2ull * GetU16(b, 2ull * aliasList);
            std::memset(b + start, 'a', letters);
            b[start + letters] = 0;
        });
    };
    Require(ThrowsAs(1, named(60), aliases.length, false, swap), "ucnv_swapAliases sorted name longer than ICU's buffer");
    AgainstIcuAs(1, named(59), aliases.length, false, swap, ucnv_swapAliases, "ucnv_swapAliases longest sorted name");
}

void TestConverterSwap() {
    const auto swap = ucnv_swap_nid_postfix;
    for (const char* name : {"ibm-1006_P100-1995", "ibm-5478_P100-1995", "cns-11643-1992", "euc-jp-2007", "euc-tw-2014", "gb18030", "gsm-03.38-2009", "ibm-1363_P110-1997", "ibm-1364_P110-2007"})
        RoundTrip(Item(nullptr, "cnv", name), swap, ucnv_swap, std::string("ucnv_swap ") + name);
    const Bytes sbcs = Item(nullptr, "cnv", "ibm-1006_P100-1995");
    const Bytes ext = Item(nullptr, "cnv", "gb18030");
    const Bytes extOnly = Item(nullptr, "cnv", "ibm-1363_P110-1997");
    for (const bool inPlace : {false, true}) {
        Require(AgainstIcuAs(1, sbcs, sbcs.length, inPlace, swap, ucnv_swap, "ucnv_swap SBCS to EBCDIC") == Ok, "ucnv_swap SBCS to EBCDIC");
        Require(AgainstIcuAs(1, extOnly, extOnly.length, inPlace, swap, ucnv_swap, "ucnv_swap extension only to EBCDIC") == Ok, "ucnv_swap extension only to EBCDIC");
    }
    GuestDataSwapper* ds = Swapper(false, 0, true, 0);
    Bytes out(sbcs.length);
    Require(ucnv_swap_nid_postfix(ds, sbcs.data(), sbcs.length, out.data(), nullptr) == 0, "ucnv_swap NULL status");
    udata_closeSwapper_nid_postfix(ds);
    FormatMessage(swap, ucnv_swap, Item(nullptr, "icu", "unames"), "75.6e.61.6d (format version 01.", "ucnv_swap");

    const std::int32_t header = Header(sbcs);
    constexpr std::uint64_t mbcs = sizeof(UConverterStaticData);
    const auto field = [&](std::size_t offset) { return mbcs + offset; };
    Require(AgainstIcu(sbcs, header + 99, false, swap, ucnv_swap, "ucnv_swap short") == U_INDEX_OUTOFBOUNDS_ERROR, "ucnv_swap short static data");
    Require(AgainstIcu(Mutated(sbcs, [](std::uint8_t* b) { PutU32(b, 0, 0x10000000); }), sbcs.length, false, swap, ucnv_swap, "ucnv_swap static size") == U_INDEX_OUTOFBOUNDS_ERROR, "ucnv_swap static data past the length");
    Require(AgainstIcu(Mutated(sbcs, [](std::uint8_t* b) { b[offsetof(UConverterStaticData, conversionType)] = UCNV_SBCS; }), sbcs.length, false, swap, ucnv_swap, "ucnv_swap type") == U_UNSUPPORTED_ERROR, "ucnv_swap non-MBCS data");
    Require(AgainstIcu(sbcs, header + 100 + 39, false, swap, ucnv_swap, "ucnv_swap MBCS header") == U_INDEX_OUTOFBOUNDS_ERROR, "ucnv_swap short MBCS header");
    Require(AgainstIcu(Mutated(sbcs, [](std::uint8_t* b) { b[mbcs] = 3; }), sbcs.length, false, swap, ucnv_swap, "ucnv_swap version") == U_UNSUPPORTED_ERROR, "ucnv_swap MBCS version");
    Require(AgainstIcu(Mutated(sbcs, [&](std::uint8_t* b) { b[field(offsetof(_MBCSHeader, flags))] = 0x55; }), sbcs.length, false, swap, ucnv_swap, "ucnv_swap output") == U_UNSUPPORTED_ERROR, "ucnv_swap output type");
    Require(AgainstIcu(sbcs, header + 100 + 48, false, swap, ucnv_swap, "ucnv_swap tables") == U_INDEX_OUTOFBOUNDS_ERROR, "ucnv_swap tables past the length");
    const std::uint32_t extOffset = GetU32(ext.data() + Header(ext), mbcs + offsetof(_MBCSHeader, flags)) >> 8;
    Require(AgainstIcu(ext, Header(ext) + 100 + static_cast<std::int32_t>(extOffset) + 100, false, swap, ucnv_swap, "ucnv_swap extension") == U_INDEX_OUTOFBOUNDS_ERROR, "ucnv_swap short extension indexes");

    Require(Throws(Mutated(sbcs, [](std::uint8_t* b) { std::memset(b + 4, 'a', 96); }), header + 100, false, swap), "ucnv_swap name without a NUL");
    Require(AgainstIcu(sbcs, header + 100, false, swap, ucnv_swap, "ucnv_swap static only") == U_INDEX_OUTOFBOUNDS_ERROR, "ucnv_swap name inside the data");
    const Bytes negative = Mutated(sbcs, [&](std::uint8_t* b) { PutU32(b, field(offsetof(_MBCSHeader, offsetFromUBytes)), 0x80000000u); });
    Require(Throws(negative, sbcs.length, false, swap), "ucnv_swap negative size");
    AgainstIcu(negative, sbcs.length, true, swap, ucnv_swap, "ucnv_swap negative size in place");
    Require(Throws(Mutated(sbcs, [&](std::uint8_t* b) { PutU32(b, field(offsetof(_MBCSHeader, countStates)), 0x1000); }), sbcs.length, false, swap), "ucnv_swap states past the data");
    Require(Throws(Mutated(sbcs, [&](std::uint8_t* b) { PutU32(b, field(offsetof(_MBCSHeader, countToUFallbacks)), 0x100000); }), sbcs.length, false, swap), "ucnv_swap fallbacks past the data");
    Require(Throws(Mutated(sbcs, [&](std::uint8_t* b) { PutU32(b, field(offsetof(_MBCSHeader, offsetFromUTable)), GetU32(b, field(offsetof(_MBCSHeader, offsetToUCodeUnits))) + 0x100000); }), sbcs.length, false, swap), "ucnv_swap code units past the data");
    Require(Throws(Mutated(ext, [&](std::uint8_t* b) { PutU32(b, field(offsetof(_MBCSHeader, offsetFromUBytes)), 0x100000); }), ext.length, false, swap), "ucnv_swap stage 2 past the data");
    const auto extIndex = [&](std::int32_t slot) { return mbcs + extOffset + 4ull * static_cast<std::uint64_t>(slot); };
    Require(Throws(Mutated(ext, [&](std::uint8_t* b) { PutU32(b, extIndex(UCNV_EXT_TO_U_INDEX), 0x100000); }), ext.length, false, swap), "ucnv_swap extension table past the data");
    Require(Throws(Mutated(ext, [&](std::uint8_t* b) { PutU32(b, extIndex(UCNV_EXT_TO_U_INDEX), 0xfffffffcu); }), ext.length, false, swap), "ucnv_swap extension table at a negative offset");
    Require(Throws(Mutated(ext, [&](std::uint8_t* b) { PutU32(b, extIndex(UCNV_EXT_INDEXES_LENGTH), 0x100000); }), ext.length, false, swap), "ucnv_swap extension indexes past the data");
    const std::uint32_t onlyOffset = GetU32(extOnly.data() + Header(extOnly), mbcs + offsetof(_MBCSHeader, flags)) >> 8;
    const Bytes noName = Mutated(extOnly, [&](std::uint8_t* b) {
        std::memset(b + mbcs + 32, 'a', onlyOffset - 32 + 128);
        PutU32(b, mbcs + onlyOffset + 4ull * UCNV_EXT_SIZE, 0x81818181u);
    });
    Require(Throws(noName, Header(extOnly) + 100 + static_cast<std::int32_t>(onlyOffset) + 128, true, swap), "ucnv_swap base name without a NUL");
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
    TestBreakSwap();
    TestNormalizerSwap();
    TestCollationBinary();
    TestCollationSwap();
    TestInverseSwap();
    TestBundleSwap();
    TestAliasSwap();
    TestConverterSwap();
    std::puts("ICU DATA SWAPPER TESTS PASSED");
    return 0;
}
