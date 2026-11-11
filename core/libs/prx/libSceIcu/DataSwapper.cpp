#include "prx/libSceIcu/DataSwapper.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <unicode/udata.h>
#include "dictionarydata.h"
#include "ucmndata.h"
#include "udataswp.h"
#include "uprops.h"
#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>

namespace {

static_assert(sizeof(UBool) == 1 && sizeof(UChar) == 2);
static_assert(offsetof(UDataSwapper, inIsBigEndian) == 0 && offsetof(GuestDataSwapper, inIsBigEndian) == 0);
static_assert(offsetof(UDataSwapper, inCharset) == 1 && offsetof(GuestDataSwapper, inCharset) == 1);
static_assert(offsetof(UDataSwapper, outIsBigEndian) == 2 && offsetof(GuestDataSwapper, outIsBigEndian) == 2);
static_assert(offsetof(UDataSwapper, outCharset) == 3 && offsetof(GuestDataSwapper, outCharset) == 3);
static_assert(offsetof(UDataSwapper, readUInt16) == 8 && offsetof(GuestDataSwapper, readUInt16) == 8);
static_assert(offsetof(UDataSwapper, readUInt32) == 16 && offsetof(GuestDataSwapper, readUInt32) == 16);
static_assert(offsetof(UDataSwapper, compareInvChars) == 24 && offsetof(GuestDataSwapper, compareInvChars) == 24);
static_assert(offsetof(UDataSwapper, writeUInt16) == 32 && offsetof(GuestDataSwapper, writeUInt16) == 32);
static_assert(offsetof(UDataSwapper, writeUInt32) == 40 && offsetof(GuestDataSwapper, writeUInt32) == 40);
static_assert(offsetof(UDataSwapper, swapArray16) == 48 && offsetof(GuestDataSwapper, swapArray16) == 48);
static_assert(offsetof(UDataSwapper, swapArray32) == 56 && offsetof(GuestDataSwapper, swapArray32) == 56);
static_assert(offsetof(UDataSwapper, swapArray64) == 64 && offsetof(GuestDataSwapper, swapArray64) == 64);
static_assert(offsetof(UDataSwapper, swapInvChars) == 72 && offsetof(GuestDataSwapper, swapInvChars) == 72);
static_assert(offsetof(UDataSwapper, printError) == 80 && offsetof(GuestDataSwapper, printError) == 80);
static_assert(offsetof(UDataSwapper, printErrorContext) == 88 && offsetof(GuestDataSwapper, printErrorContext) == 88);
static_assert(sizeof(UDataSwapper) == 96 && sizeof(GuestDataSwapper) == 96);
static_assert(sizeof(DataHeader) == 24 && sizeof(UDataInfo) == 20);

constexpr std::size_t Variants = 4;

template<typename THost, typename TGuest>
struct Slot {
    std::array<THost, Variants> host{};
    std::array<TGuest, Variants> guest{};
    std::size_t count = 0;

    void Add(THost function) {
        for (std::size_t index = 0; index < count; ++index)
            if (host[index] == function) return;
        if (count == Variants) throw std::logic_error("libSceIcu: ICU 68.2 has more swapper functions than expected");
        host[count++] = function;
    }

    TGuest Guest(THost function) const {
        for (std::size_t index = 0; index < count; ++index)
            if (host[index] == function) return guest[index];
        throw std::logic_error("libSceIcu: a swapper function that ICU 68.2 never installs");
    }

    THost Host(TGuest function, THost fallback) const {
        for (std::size_t index = 0; index < count; ++index)
            if (guest[index] == function) return host[index];
        return fallback;
    }
};

struct SwapperTables {
    Slot<UDataReadUInt16*, GuestReadUInt16> read16;
    Slot<UDataReadUInt32*, GuestReadUInt32> read32;
    Slot<UDataCompareInvChars*, GuestCompareInvChars> compare;
    Slot<UDataWriteUInt16*, GuestWriteUInt16> write16;
    Slot<UDataWriteUInt32*, GuestWriteUInt32> write32;
    std::array<Slot<UDataSwapFn*, GuestSwapFn>, 4> swaps;
};

const SwapperTables& Tables();

struct HostView {
    UDataSwapper host{};
    const GuestDataSwapper* guest;
    GuestDataSwapper captured;

    explicit HostView(const GuestDataSwapper* source);
    HostView(const HostView&) = delete;
    HostView& operator=(const HostView&) = delete;
};

static_assert(std::is_standard_layout_v<HostView>);

thread_local const HostView* activeView = nullptr;

struct ActiveView {
    const HostView* saved;

    explicit ActiveView(const HostView* view) : saved(activeView) {
        activeView = view;
    }

    ~ActiveView() {
        activeView = saved;
    }

    ActiveView(const ActiveView&) = delete;
    ActiveView& operator=(const ActiveView&) = delete;
};

template<typename TCall>
auto WithView(const GuestDataSwapper* ds, TCall call) {
    IcuBackend::Ensure();
    if (!ds) return call(nullptr);
    const HostView view(ds);
    const ActiveView active(&view);
    return call(&view.host);
}

template<std::size_t K>
std::uint16_t APS5_VABI SwapperReadUInt16_nid_no_patch(std::uint16_t x) {
    return Tables().read16.host[K](x);
}

template<std::size_t K>
std::uint32_t APS5_VABI SwapperReadUInt32_nid_no_patch(std::uint32_t x) {
    return Tables().read32.host[K](x);
}

template<std::size_t K>
void APS5_VABI SwapperWriteUInt16_nid_no_patch(std::uint16_t* p, std::uint16_t x) {
    Tables().write16.host[K](p, x);
}

template<std::size_t K>
void APS5_VABI SwapperWriteUInt32_nid_no_patch(std::uint32_t* p, std::uint32_t x) {
    Tables().write32.host[K](p, x);
}

template<std::size_t K>
std::int32_t APS5_VABI SwapperCompareInvChars_nid_no_patch(const GuestDataSwapper* ds, const char* outString, std::int32_t outLength, const char16_t* localString, std::int32_t localLength) {
    return WithView(ds, [&](const UDataSwapper* host) {
        return Tables().compare.host[K](host, outString, outLength, localString, localLength);
    });
}

template<std::size_t S, std::size_t K>
std::int32_t APS5_VABI SwapperSwap_nid_no_patch(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status) {
    return WithView(ds, [&](const UDataSwapper* host) {
        return Tables().swaps[S].host[K](host, inData, length, outData, reinterpret_cast<UErrorCode*>(status));
    });
}

const HostView* ViewOf(const UDataSwapper* ds) {
    return reinterpret_cast<const HostView*>(ds);
}

uint16_t U_CALLCONV HostReadUInt16(uint16_t x) {
    return activeView->captured.readUInt16(x);
}

uint32_t U_CALLCONV HostReadUInt32(uint32_t x) {
    return activeView->captured.readUInt32(x);
}

void U_CALLCONV HostWriteUInt16(uint16_t* p, uint16_t x) {
    activeView->captured.writeUInt16(p, x);
}

void U_CALLCONV HostWriteUInt32(uint32_t* p, uint32_t x) {
    activeView->captured.writeUInt32(p, x);
}

int32_t U_CALLCONV HostCompareInvChars(const UDataSwapper* ds, const char* outString, int32_t outLength, const UChar* localString, int32_t localLength) {
    const HostView* view = ViewOf(ds);
    return view->captured.compareInvChars(view->guest, outString, outLength, localString, localLength);
}

template<GuestSwapFn GuestDataSwapper::*Field>
int32_t U_CALLCONV HostSwap(const UDataSwapper* ds, const void* inData, int32_t length, void* outData, UErrorCode* status) {
    const HostView* view = ViewOf(ds);
    return (view->captured.*Field)(view->guest, inData, length, outData, reinterpret_cast<std::int32_t*>(status));
}

void APS5_VABI CallGuestPrintError_nid_no_patch(GuestPrintError function, void* context, const char* fmt, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, fmt);
    function(context, fmt, reinterpret_cast<VaList*>(args));
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, fmt);
    function(context, fmt, reinterpret_cast<VaList*>(args));
    va_end(args);
#endif
}

bool Digit(char c) {
    return c >= '0' && c <= '9';
}

bool Marshal(const char* fmt, va_list args, std::array<std::uint64_t, 8>& slots) {
    std::size_t count = 0;
    va_list copy;
    va_copy(copy, args);
    bool marshalled = true;
    for (const char* p = fmt; marshalled && *p; ++p) {
        if (*p != '%') continue;
        ++p;
        if (*p == '%') continue;
        while (Digit(*p) || *p == '.') ++p;
        if (count == slots.size()) {
            marshalled = false;
            break;
        }
        switch (*p) {
        case 'd':
        case 'i':
        case 'c':
            slots[count++] = static_cast<std::uint64_t>(static_cast<std::int64_t>(va_arg(copy, int)));
            break;
        case 'u':
        case 'o':
        case 'x':
        case 'X':
            slots[count++] = va_arg(copy, unsigned int);
            break;
        case 's':
        case 'p':
            slots[count++] = reinterpret_cast<std::uintptr_t>(va_arg(copy, const void*));
            break;
        default:
            marshalled = false;
            break;
        }
    }
    va_end(copy);
    return marshalled;
}

void U_CALLCONV HostPrintError(void* context, const char* fmt, va_list args) {
    const auto* view = static_cast<const HostView*>(context);
    const GuestPrintError function = view->captured.printError;
    void* guestContext = view->captured.printErrorContext;
#ifdef _WIN32
    std::array<std::uint64_t, 8> slots{};
    if (Marshal(fmt, args, slots)) {
        CallGuestPrintError_nid_no_patch(function, guestContext, fmt, slots[0], slots[1], slots[2], slots[3], slots[4], slots[5], slots[6], slots[7]);
        return;
    }
    va_list copy;
    va_copy(copy, args);
    const int length = std::vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    std::string message(length > 0 ? static_cast<std::size_t>(length) : 0, '\0');
    if (length > 0) std::vsnprintf(message.data(), message.size() + 1, fmt, args);
    CallGuestPrintError_nid_no_patch(function, guestContext, "%s", message.c_str());
#else
    function(guestContext, fmt, reinterpret_cast<VaList*>(args));
#endif
}

HostView::HostView(const GuestDataSwapper* source) : guest(source), captured(*source) {
    if (!captured.readUInt16 || !captured.readUInt32 || !captured.compareInvChars || !captured.writeUInt16 || !captured.writeUInt32 ||
        !captured.swapArray16 || !captured.swapArray32 || !captured.swapArray64 || !captured.swapInvChars) APS5_INVALID_ARG_EX;
    const SwapperTables& tables = Tables();
    host.inIsBigEndian = captured.inIsBigEndian;
    host.inCharset = captured.inCharset;
    host.outIsBigEndian = captured.outIsBigEndian;
    host.outCharset = captured.outCharset;
    host.readUInt16 = tables.read16.Host(captured.readUInt16, HostReadUInt16);
    host.readUInt32 = tables.read32.Host(captured.readUInt32, HostReadUInt32);
    host.compareInvChars = tables.compare.Host(captured.compareInvChars, HostCompareInvChars);
    host.writeUInt16 = tables.write16.Host(captured.writeUInt16, HostWriteUInt16);
    host.writeUInt32 = tables.write32.Host(captured.writeUInt32, HostWriteUInt32);
    host.swapArray16 = tables.swaps[0].Host(captured.swapArray16, HostSwap<&GuestDataSwapper::swapArray16>);
    host.swapArray32 = tables.swaps[1].Host(captured.swapArray32, HostSwap<&GuestDataSwapper::swapArray32>);
    host.swapArray64 = tables.swaps[2].Host(captured.swapArray64, HostSwap<&GuestDataSwapper::swapArray64>);
    host.swapInvChars = tables.swaps[3].Host(captured.swapInvChars, HostSwap<&GuestDataSwapper::swapInvChars>);
    host.printError = captured.printError ? HostPrintError : nullptr;
    host.printErrorContext = this;
}

template<std::size_t S, std::size_t... K>
void SetSwapGuests(Slot<UDataSwapFn*, GuestSwapFn>& slot, std::index_sequence<K...>) {
    slot.guest = {SwapperSwap_nid_no_patch<S, K>...};
}

template<std::size_t... K>
void SetGuests(SwapperTables& tables, std::index_sequence<K...> sequence) {
    tables.read16.guest = {SwapperReadUInt16_nid_no_patch<K>...};
    tables.read32.guest = {SwapperReadUInt32_nid_no_patch<K>...};
    tables.compare.guest = {SwapperCompareInvChars_nid_no_patch<K>...};
    tables.write16.guest = {SwapperWriteUInt16_nid_no_patch<K>...};
    tables.write32.guest = {SwapperWriteUInt32_nid_no_patch<K>...};
    SetSwapGuests<0>(tables.swaps[0], sequence);
    SetSwapGuests<1>(tables.swaps[1], sequence);
    SetSwapGuests<2>(tables.swaps[2], sequence);
    SetSwapGuests<3>(tables.swaps[3], sequence);
}

SwapperTables BuildTables() {
    SwapperTables tables;
    SetGuests(tables, std::make_index_sequence<Variants>());
    for (int combination = 0; combination < 16; ++combination) {
        UErrorCode status = U_ZERO_ERROR;
        UDataSwapper* host = udata_openSwapper(static_cast<UBool>(combination & 1), static_cast<uint8_t>((combination >> 1) & 1), static_cast<UBool>((combination >> 2) & 1), static_cast<uint8_t>((combination >> 3) & 1), &status);
        if (!host || U_FAILURE(status)) throw std::runtime_error("libSceIcu: udata_openSwapper failed");
        tables.read16.Add(host->readUInt16);
        tables.read32.Add(host->readUInt32);
        tables.compare.Add(host->compareInvChars);
        tables.write16.Add(host->writeUInt16);
        tables.write32.Add(host->writeUInt32);
        tables.swaps[0].Add(host->swapArray16);
        tables.swaps[1].Add(host->swapArray32);
        tables.swaps[2].Add(host->swapArray64);
        tables.swaps[3].Add(host->swapInvChars);
        udata_closeSwapper(host);
    }
    return tables;
}

const SwapperTables& Tables() {
    static const SwapperTables tables = BuildTables();
    return tables;
}

std::mutex& SwapperMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_set<const GuestDataSwapper*>& Swappers() {
    static std::unordered_set<const GuestDataSwapper*> swappers;
    return swappers;
}

GuestDataSwapper* Guest(UDataSwapper* host, UErrorCode* status) {
    if (!host) return nullptr;
    const SwapperTables& tables = Tables();
    auto* guest = new (std::nothrow) GuestDataSwapper{};
    if (!guest) {
        udata_closeSwapper(host);
        *status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    guest->inIsBigEndian = host->inIsBigEndian;
    guest->inCharset = host->inCharset;
    guest->outIsBigEndian = host->outIsBigEndian;
    guest->outCharset = host->outCharset;
    guest->readUInt16 = tables.read16.Guest(host->readUInt16);
    guest->readUInt32 = tables.read32.Guest(host->readUInt32);
    guest->compareInvChars = tables.compare.Guest(host->compareInvChars);
    guest->writeUInt16 = tables.write16.Guest(host->writeUInt16);
    guest->writeUInt32 = tables.write32.Guest(host->writeUInt32);
    guest->swapArray16 = tables.swaps[0].Guest(host->swapArray16);
    guest->swapArray32 = tables.swaps[1].Guest(host->swapArray32);
    guest->swapArray64 = tables.swaps[2].Guest(host->swapArray64);
    guest->swapInvChars = tables.swaps[3].Guest(host->swapInvChars);
    udata_closeSwapper(host);
    const std::lock_guard lock(SwapperMutex());
    Swappers().insert(guest);
    return guest;
}

UErrorCode* OptionalStatus(std::int32_t* status) {
    IcuBackend::Ensure();
    return reinterpret_cast<UErrorCode*>(status);
}

UErrorCode* RequiredStatus(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

std::int32_t HeaderSize(const UDataSwapper* ds, const void* inData, std::int32_t length, const void* outData) {
    if (!inData || length < -1 || (length > 0 && !outData)) return -1;
    const auto* header = static_cast<const DataHeader*>(inData);
    if ((length >= 0 && length < static_cast<std::int32_t>(sizeof(DataHeader))) || header->dataHeader.magic1 != 0xda || header->dataHeader.magic2 != 0x27 || header->info.sizeofUChar != 2) return -1;
    const std::uint16_t headerSize = ds->readUInt16(header->dataHeader.headerSize);
    const std::uint16_t infoSize = ds->readUInt16(header->info.size);
    if (headerSize < sizeof(DataHeader) || infoSize < sizeof(UDataInfo) || headerSize < sizeof(header->dataHeader) + infoSize || (length >= 0 && length < headerSize)) return -1;
    return headerSize;
}

bool Format(const void* inData, const char* format) {
    const auto* info = reinterpret_cast<const UDataInfo*>(static_cast<const char*>(inData) + 4);
    return std::memcmp(info->dataFormat, format, 4) == 0 && info->formatVersion[0] == 1;
}

struct Reader {
    const UDataSwapper* ds;
    const std::uint8_t* bytes;

    std::uint32_t U32(std::uint64_t offset) const {
        std::uint32_t raw;
        std::memcpy(&raw, bytes + offset, sizeof(raw));
        return ds->readUInt32(raw);
    }

    std::uint16_t U16(std::uint64_t offset) const {
        std::uint16_t raw;
        std::memcpy(&raw, bytes + offset, sizeof(raw));
        return ds->readUInt16(raw);
    }
};

void CheckNames(const UDataSwapper* ds, const void* inData, std::int32_t length, const void* outData) {
    const std::int32_t headerSize = HeaderSize(ds, inData, length, outData);
    if (headerSize < 0 || !Format(inData, "unam")) return;
    const std::uint64_t available = static_cast<std::uint64_t>(length - headerSize);
    const Reader reader{ds, static_cast<const std::uint8_t*>(inData) + headerSize};
    if (available < 20) return;
    const std::uint64_t algNamesOffset = reader.U32(12);
    if (available < algNamesOffset) return;
    if (ds->inCharset != ds->outCharset) NotImplemented_nid_no_patch("libSceIcu: a names swap that changes the charset family (ICU 68.2 permutes tokens and walks the group strings without bounds checks)");
    const std::uint64_t tokenStringOffset = reader.U32(0);
    const std::uint64_t groupsOffset = reader.U32(4);
    const std::uint64_t tokenCount = reader.U16(16);
    if (18 + 2 * tokenCount > available || tokenStringOffset > groupsOffset || groupsOffset + 2 > available) APS5_INVALID_ARG_EX;
    if (groupsOffset + 2 * (1 + 3 * static_cast<std::uint64_t>(reader.U16(groupsOffset))) > available || algNamesOffset + 4 > available) APS5_INVALID_ARG_EX;
    const std::uint32_t rangeCount = reader.U32(algNamesOffset);
    std::uint64_t offset = algNamesOffset + 4;
    for (std::uint32_t range = 0; range < rangeCount; ++range) {
        if (offset > available) return;
        if (offset + 12 > available) APS5_INVALID_ARG_EX;
        const std::uint8_t type = reader.bytes[offset + 8];
        const std::uint64_t factors = reader.bytes[offset + 9];
        const std::uint64_t next = offset + reader.U16(offset + 10);
        if (type == 0) {
            if (!std::memchr(reader.bytes + offset + 12, 0, available - (offset + 12))) APS5_INVALID_ARG_EX;
        } else if (type == 1) {
            if (offset + 12 + 2 * factors > next || next > available) APS5_INVALID_ARG_EX;
        } else {
            return;
        }
        offset = next;
    }
}

void CheckDictionary(const UDataSwapper* ds, const void* inData, std::int32_t length, const void* outData) {
    const std::int32_t headerSize = HeaderSize(ds, inData, length, outData);
    if (headerSize < 0 || !Format(inData, "Dict")) return;
    const std::int32_t available = length - headerSize;
    const Reader reader{ds, static_cast<const std::uint8_t*>(inData) + headerSize};
    if (available < static_cast<std::int32_t>(icu::DictionaryData::IX_COUNT * sizeof(std::int32_t))) return;
    const auto size = static_cast<std::int32_t>(reader.U32(icu::DictionaryData::IX_TOTAL_SIZE * sizeof(std::int32_t)));
    if (available < size) return;
    if (size < 0) APS5_INVALID_ARG_EX;
    const auto trieType = static_cast<std::int32_t>(reader.U32(icu::DictionaryData::IX_TRIE_TYPE * sizeof(std::int32_t))) & icu::DictionaryData::TRIE_TYPE_MASK;
    const auto nextOffset = static_cast<std::int32_t>(reader.U32(icu::DictionaryData::IX_RESERVED1_OFFSET * sizeof(std::int32_t)));
    if (trieType == icu::DictionaryData::TRIE_TYPE_UCHARS && nextOffset > available) APS5_INVALID_ARG_EX;
}

template<typename TSwap>
std::int32_t SwapChecked(const GuestDataSwapper* ds, std::int32_t* status, TSwap swap) {
    UErrorCode* error = OptionalStatus(status);
    if (!error || U_FAILURE(*error)) return 0;
    return WithView(ds, [&](const UDataSwapper* host) {
        return swap(host, error);
    });
}

template<typename TSwap>
std::int32_t SwapRequired(const GuestDataSwapper* ds, std::int32_t* status, TSwap swap) {
    UErrorCode* error = RequiredStatus(status);
    if (U_FAILURE(*error)) return 0;
    return WithView(ds, [&](const UDataSwapper* host) {
        return swap(host, error);
    });
}

}

extern "C" {

GuestDataSwapper* APS5_VABI udata_openSwapper_nid_postfix(std::int8_t inIsBigEndian, std::uint8_t inCharset, std::int8_t outIsBigEndian, std::uint8_t outCharset, std::int32_t* status) {
    UErrorCode* error = OptionalStatus(status);
    return Guest(udata_openSwapper(inIsBigEndian, inCharset, outIsBigEndian, outCharset, error), error);
}

GuestDataSwapper* APS5_VABI udata_openSwapperForInputData_nid_postfix(const void* data, std::int32_t length, std::int8_t outIsBigEndian, std::uint8_t outCharset, std::int32_t* status) {
    UErrorCode* error = OptionalStatus(status);
    return Guest(udata_openSwapperForInputData(data, length, outIsBigEndian, outCharset, error), error);
}

void APS5_VABI udata_closeSwapper_nid_postfix(GuestDataSwapper* ds) {
    if (!ds) return;
    {
        const std::lock_guard lock(SwapperMutex());
        if (!Swappers().erase(ds)) APS5_INVALID_ARG_EX;
    }
    delete ds;
}

std::int32_t APS5_VABI udata_swapDataHeader_nid_postfix(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status) {
    return SwapChecked(ds, status, [&](const UDataSwapper* host, UErrorCode* error) {
        return udata_swapDataHeader(host, inData, length, outData, error);
    });
}

std::int16_t APS5_VABI udata_readInt16_nid_postfix(const GuestDataSwapper* ds, std::int16_t x) {
    IcuBackend::RequireObject(ds);
    if (!ds->readUInt16) APS5_INVALID_ARG_EX;
    return static_cast<std::int16_t>(ds->readUInt16(static_cast<std::uint16_t>(x)));
}

std::int32_t APS5_VABI udata_readInt32_nid_postfix(const GuestDataSwapper* ds, std::int32_t x) {
    IcuBackend::RequireObject(ds);
    if (!ds->readUInt32) APS5_INVALID_ARG_EX;
    return static_cast<std::int32_t>(ds->readUInt32(static_cast<std::uint32_t>(x)));
}

std::int32_t APS5_VABI udata_swapInvStringBlock_nid_postfix(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status) {
    return SwapChecked(ds, status, [&](const UDataSwapper* host, UErrorCode* error) {
        return udata_swapInvStringBlock(host, inData, length, outData, error);
    });
}

void APS5_VABI udata_printError_nid_postfix(const GuestDataSwapper* ds, const char* fmt, ...) {
    IcuBackend::RequireObject(ds);
    if (!ds->printError) return;
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, fmt);
    ds->printError(ds->printErrorContext, fmt, reinterpret_cast<VaList*>(args));
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, fmt);
    ds->printError(ds->printErrorContext, fmt, reinterpret_cast<VaList*>(args));
    va_end(args);
#endif
}

std::int32_t APS5_VABI utrie2_swap_nid_postfix(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status) {
    return SwapRequired(ds, status, [&](const UDataSwapper* host, UErrorCode* error) {
        return utrie2_swap(host, inData, length, outData, error);
    });
}

std::int32_t APS5_VABI ucptrie_swap_nid_postfix(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status) {
    return SwapRequired(ds, status, [&](const UDataSwapper* host, UErrorCode* error) {
        return ucptrie_swap(host, inData, length, outData, error);
    });
}

std::int32_t APS5_VABI utrie_swapAnyVersion_nid_postfix(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status) {
    return SwapRequired(ds, status, [&](const UDataSwapper* host, UErrorCode* error) {
        return utrie_swapAnyVersion(host, inData, length, outData, error);
    });
}

std::int32_t APS5_VABI uchar_swapNames_nid_postfix(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status) {
    const auto swap = [&](const UDataSwapper* host, UErrorCode* error) {
        if (host && length >= 0) CheckNames(host, inData, length, outData);
        return uchar_swapNames(host, inData, length, outData, error);
    };
    return SwapChecked(ds, status, swap);
}

std::int32_t APS5_VABI udict_swap_nid_postfix(const GuestDataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* status) {
    return SwapChecked(ds, status, [&](const UDataSwapper* host, UErrorCode* error) {
        if (host && length >= 0) CheckDictionary(host, inData, length, outData);
        return udict_swap(host, inData, length, outData, error);
    });
}

}
