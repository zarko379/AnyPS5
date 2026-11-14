#include "prx/libSceIcu/Text.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/ubrk.h>
#include <unicode/utext.h>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace {

constexpr std::int32_t OpenFlag = 4;

static_assert(offsetof(GuestTextFuncs, clone) == offsetof(UTextFuncs, clone) && offsetof(GuestTextFuncs, nativeLength) == offsetof(UTextFuncs, nativeLength) && offsetof(GuestTextFuncs, access) == offsetof(UTextFuncs, access) && offsetof(GuestTextFuncs, extract) == offsetof(UTextFuncs, extract));
static_assert(offsetof(GuestTextFuncs, replace) == offsetof(UTextFuncs, replace) && offsetof(GuestTextFuncs, copy) == offsetof(UTextFuncs, copy) && offsetof(GuestTextFuncs, mapOffsetToNative) == offsetof(UTextFuncs, mapOffsetToNative) && offsetof(GuestTextFuncs, mapNativeIndexToUTF16) == offsetof(UTextFuncs, mapNativeIndexToUTF16));
static_assert(offsetof(GuestTextFuncs, close) == offsetof(UTextFuncs, close) && offsetof(GuestTextFuncs, spare3) == offsetof(UTextFuncs, spare3) && sizeof(GuestTextFuncs) == sizeof(UTextFuncs));

struct Providers {
    const UTextFuncs* chars;
    const UTextFuncs* utf8;
};

const Providers& Known() {
    static const Providers providers = [] {
        UErrorCode status = U_ZERO_ERROR;
        UText chars = UTEXT_INITIALIZER;
        UText utf8 = UTEXT_INITIALIZER;
        utext_openUChars(&chars, u"", 0, &status);
        utext_openUTF8(&utf8, "", 0, &status);
        const Providers result{chars.pFuncs, utf8.pFuncs};
        utext_close(&chars);
        utext_close(&utf8);
        if (U_FAILURE(status) || result.chars == nullptr || result.utf8 == nullptr) throw std::logic_error("ICU 68.2 UText providers are unavailable");
        return result;
    }();
    return providers;
}

struct Wrapper {
    UTextFuncs host;
    const GuestTextFuncs* guest;
};

struct Wrappers {
    std::mutex mutex;
    std::unordered_map<const GuestTextFuncs*, std::unique_ptr<Wrapper>> byGuest;
    std::unordered_set<const UTextFuncs*> hosts;
};

Wrappers& Registry() {
    static Wrappers wrappers;
    return wrappers;
}

const GuestTextFuncs* GuestOf(const UText* text) {
    return reinterpret_cast<const Wrapper*>(text->pFuncs)->guest;
}

const UTextFuncs* AsHost(const GuestTextFuncs* guest) {
    return reinterpret_cast<const UTextFuncs*>(guest);
}

const GuestTextFuncs* AsGuest(const UTextFuncs* funcs) {
    return reinterpret_cast<const GuestTextFuncs*>(funcs);
}

template<typename TSlot>
TSlot Slot(TSlot slot) {
    if (!slot) APS5_INVALID_ARG_EX;
    return slot;
}

UText* U_CALLCONV AbsentClone(UText*, const UText*, UBool, UErrorCode*) {
    APS5_INVALID_ARG_EX;
}

int64_t U_CALLCONV AbsentLength(UText*) {
    APS5_INVALID_ARG_EX;
}

UBool U_CALLCONV AbsentAccess(UText*, int64_t, UBool) {
    APS5_INVALID_ARG_EX;
}

int32_t U_CALLCONV AbsentExtract(UText*, int64_t, int64_t, UChar*, int32_t, UErrorCode*) {
    APS5_INVALID_ARG_EX;
}

int32_t U_CALLCONV AbsentReplace(UText*, int64_t, int64_t, const UChar*, int32_t, UErrorCode*) {
    APS5_INVALID_ARG_EX;
}

void U_CALLCONV AbsentCopy(UText*, int64_t, int64_t, int64_t, UBool, UErrorCode*) {
    APS5_INVALID_ARG_EX;
}

int64_t U_CALLCONV AbsentOffsetToNative(const UText*) {
    APS5_INVALID_ARG_EX;
}

int32_t U_CALLCONV AbsentNativeToOffset(const UText*, int64_t) {
    APS5_INVALID_ARG_EX;
}

void U_CALLCONV AbsentClose(UText*) {
    APS5_INVALID_ARG_EX;
}

const UTextFuncs absent{sizeof(UTextFuncs), 0, 0, 0, AbsentClone, AbsentLength, AbsentAccess, AbsentExtract, AbsentReplace, AbsentCopy, AbsentOffsetToNative, AbsentNativeToOffset, AbsentClose, nullptr, nullptr, nullptr};

enum class Kind {
    Icu,
    Absent,
    Wrapped,
    Guest
};

Kind Classify(const UTextFuncs* funcs) {
    if (funcs == Known().chars || funcs == Known().utf8) return Kind::Icu;
    if (funcs == &absent) return Kind::Absent;
    Wrappers& wrappers = Registry();
    const std::lock_guard lock(wrappers.mutex);
    return wrappers.hosts.count(funcs) != 0 ? Kind::Wrapped : Kind::Guest;
}

bool IsOpen(const UText* text) {
    return text->magic == UTEXT_MAGIC && (text->flags & OpenFlag) != 0;
}

const UTextFuncs* Wrap(const GuestTextFuncs* guest);

void WrapGuest(UText* text) {
    if (text != nullptr && IsOpen(text) && text->pFuncs != nullptr && Classify(text->pFuncs) == Kind::Guest) text->pFuncs = Wrap(AsGuest(text->pFuncs));
}

class Unwrapped {
public:
    explicit Unwrapped(const UText* text) : text_(const_cast<UText*>(text)), host_(text->pFuncs), guest_(GuestOf(text)) {
        text_->pFuncs = AsHost(guest_);
    }

    ~Unwrapped() {
        if (text_->pFuncs == AsHost(guest_)) {
            text_->pFuncs = host_;
        } else {
            WrapGuest(text_);
        }
    }

    Unwrapped(const Unwrapped&) = delete;
    Unwrapped& operator=(const Unwrapped&) = delete;

    const GuestTextFuncs* Guest() const {
        return guest_;
    }

private:
    UText* text_;
    const UTextFuncs* host_;
    const GuestTextFuncs* guest_;
};

UText* U_CALLCONV CloneBridge(UText* dest, const UText* src, UBool deep, UErrorCode* status) {
    UText* result = nullptr;
    {
        const Unwrapped call(src);
        result = Slot(call.Guest()->clone)(dest, src, deep, reinterpret_cast<std::int32_t*>(status));
    }
    WrapGuest(result);
    return result;
}

int64_t U_CALLCONV NativeLengthBridge(UText* ut) {
    const Unwrapped call(ut);
    return Slot(call.Guest()->nativeLength)(ut);
}

UBool U_CALLCONV AccessBridge(UText* ut, int64_t nativeIndex, UBool forward) {
    const Unwrapped call(ut);
    return Slot(call.Guest()->access)(ut, nativeIndex, forward);
}

int32_t U_CALLCONV ExtractBridge(UText* ut, int64_t nativeStart, int64_t nativeLimit, UChar* dest, int32_t destCapacity, UErrorCode* status) {
    const Unwrapped call(ut);
    return Slot(call.Guest()->extract)(ut, nativeStart, nativeLimit, dest, destCapacity, reinterpret_cast<std::int32_t*>(status));
}

int32_t U_CALLCONV ReplaceBridge(UText* ut, int64_t nativeStart, int64_t nativeLimit, const UChar* replacementText, int32_t replacementLength, UErrorCode* status) {
    const Unwrapped call(ut);
    return Slot(call.Guest()->replace)(ut, nativeStart, nativeLimit, replacementText, replacementLength, reinterpret_cast<std::int32_t*>(status));
}

void U_CALLCONV CopyBridge(UText* ut, int64_t nativeStart, int64_t nativeLimit, int64_t nativeDest, UBool move, UErrorCode* status) {
    const Unwrapped call(ut);
    Slot(call.Guest()->copy)(ut, nativeStart, nativeLimit, nativeDest, move, reinterpret_cast<std::int32_t*>(status));
}

int64_t U_CALLCONV MapOffsetToNativeBridge(const UText* ut) {
    const Unwrapped call(ut);
    return Slot(call.Guest()->mapOffsetToNative)(ut);
}

int32_t U_CALLCONV MapNativeIndexToUTF16Bridge(const UText* ut, int64_t nativeIndex) {
    const Unwrapped call(ut);
    return Slot(call.Guest()->mapNativeIndexToUTF16)(ut, nativeIndex);
}

void U_CALLCONV CloseBridge(UText* ut) {
    const GuestTextFuncs* guest = GuestOf(ut);
    ut->pFuncs = AsHost(guest);
    if (guest->close) guest->close(ut);
}

const UTextFuncs* Wrap(const GuestTextFuncs* guest) {
    Wrappers& wrappers = Registry();
    const std::lock_guard lock(wrappers.mutex);
    std::unique_ptr<Wrapper>& wrapper = wrappers.byGuest[guest];
    if (!wrapper) {
        wrapper = std::make_unique<Wrapper>(Wrapper{{sizeof(UTextFuncs), 0, 0, 0, CloneBridge, NativeLengthBridge, AccessBridge, ExtractBridge, ReplaceBridge, CopyBridge, MapOffsetToNativeBridge, MapNativeIndexToUTF16Bridge, CloseBridge, nullptr, nullptr, nullptr}, guest});
        wrappers.hosts.insert(&wrapper->host);
    }
    return &wrapper->host;
}

class Bridge {
public:
    Bridge() = default;
    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;

    ~Bridge() {
        for (UText* text : texts_) {
            if (text == nullptr || text->pFuncs == nullptr) continue;
            const Kind kind = Classify(text->pFuncs);
            if (kind == Kind::Absent) {
                text->pFuncs = nullptr;
            } else if (kind == Kind::Wrapped) {
                text->pFuncs = AsHost(GuestOf(text));
            }
        }
    }

    void Enter(const UText* text) {
        if (!IsOpen(text)) return;
        UText* guest = const_cast<UText*>(text);
        if (guest->pFuncs == nullptr) {
            Keep(guest);
            guest->pFuncs = &absent;
            return;
        }
        if (Classify(guest->pFuncs) != Kind::Guest) return;
        Keep(guest);
        guest->pFuncs = Wrap(AsGuest(guest->pFuncs));
    }

    void Keep(UText* text) {
        if (text == nullptr) return;
        for (UText*& slot : texts_) {
            if (slot == text) return;
            if (slot == nullptr) {
                slot = text;
                return;
            }
        }
        throw std::logic_error("UText bridge: too many texts");
    }

    void Drop(const UText* text) {
        for (UText*& slot : texts_) {
            if (slot == text) slot = nullptr;
        }
    }

private:
    std::array<UText*, 3> texts_{};
};

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

template<typename TText>
TText* Open(TText* text, Bridge& bridge) {
    IcuBackend::Ensure();
    IcuBackend::RequireObject(text);
    if (!IsOpen(text)) APS5_INVALID_ARG_EX;
    bridge.Enter(text);
    return text;
}

template<typename TText>
TText* Readable(TText* text) {
    IcuBackend::Ensure();
    IcuBackend::RequireObject(text);
    return text;
}

bool Opens(const void* s, std::int64_t length) {
    return (s != nullptr || length == 0) && length >= -1 && length <= std::numeric_limits<std::int32_t>::max();
}

bool Writable(const UText* text) {
    return (text->providerProperties & (1 << UTEXT_PROVIDER_WRITABLE)) != 0;
}

}

extern "C" {

UText* APS5_VABI utext_openUChars_nid_postfix(UText* ut, const char16_t* s, std::int64_t length, std::int32_t* status) {
    UErrorCode* error = Status(status);
    Bridge bridge;
    if (U_SUCCESS(*error) && ut != nullptr && Opens(s, length)) bridge.Enter(ut);
    return utext_openUChars(ut, s, length, error);
}

UText* APS5_VABI utext_openUTF8_nid_postfix(UText* ut, const char* s, std::int64_t length, std::int32_t* status) {
    UErrorCode* error = Status(status);
    Bridge bridge;
    if (U_SUCCESS(*error) && ut != nullptr && Opens(s, length)) bridge.Enter(ut);
    return utext_openUTF8(ut, s, length, error);
}

UText* APS5_VABI utext_setup_nid_postfix(UText* ut, std::int32_t extraSpace, std::int32_t* status) {
    UErrorCode* error = Status(status);
    Bridge bridge;
    if (U_SUCCESS(*error) && ut != nullptr) bridge.Enter(ut);
    return utext_setup(ut, extraSpace, error);
}

UText* APS5_VABI utext_close_nid_postfix(UText* ut) {
    IcuBackend::Ensure();
    Bridge bridge;
    if (ut != nullptr) bridge.Enter(ut);
    UText* result = utext_close(ut);
    if (result == nullptr) bridge.Drop(ut);
    return result;
}

UText* APS5_VABI utext_clone_nid_postfix(UText* dest, const UText* src, std::int8_t deep, std::int8_t readOnly, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return dest;
    Bridge bridge;
    Open(src, bridge);
    if (dest != nullptr) bridge.Enter(dest);
    UText* result = utext_clone(dest, src, deep, readOnly, error);
    bridge.Keep(result);
    return result;
}

std::int8_t APS5_VABI utext_equals_nid_postfix(const UText* a, const UText* b) {
    IcuBackend::Ensure();
    Bridge bridge;
    if (a != nullptr && b != nullptr && a->magic == UTEXT_MAGIC && b->magic == UTEXT_MAGIC && a->pFuncs == b->pFuncs && a->context == b->context) {
        Open(a, bridge);
        Open(b, bridge);
    }
    return utext_equals(a, b);
}

std::int64_t APS5_VABI utext_nativeLength_nid_postfix(UText* ut) {
    Bridge bridge;
    return utext_nativeLength(Open(ut, bridge));
}

std::int8_t APS5_VABI utext_isLengthExpensive_nid_postfix(const UText* ut) {
    return utext_isLengthExpensive(Readable(ut));
}

std::int32_t APS5_VABI utext_char32At_nid_postfix(UText* ut, std::int64_t nativeIndex) {
    Bridge bridge;
    return utext_char32At(Open(ut, bridge), nativeIndex);
}

std::int32_t APS5_VABI utext_current32_nid_postfix(UText* ut) {
    Bridge bridge;
    return utext_current32(Open(ut, bridge));
}

std::int32_t APS5_VABI utext_next32_nid_postfix(UText* ut) {
    Bridge bridge;
    return utext_next32(Open(ut, bridge));
}

std::int32_t APS5_VABI utext_previous32_nid_postfix(UText* ut) {
    Bridge bridge;
    return utext_previous32(Open(ut, bridge));
}

std::int32_t APS5_VABI utext_next32From_nid_postfix(UText* ut, std::int64_t nativeIndex) {
    Bridge bridge;
    return utext_next32From(Open(ut, bridge), nativeIndex);
}

std::int32_t APS5_VABI utext_previous32From_nid_postfix(UText* ut, std::int64_t nativeIndex) {
    Bridge bridge;
    return utext_previous32From(Open(ut, bridge), nativeIndex);
}

std::int64_t APS5_VABI utext_getNativeIndex_nid_postfix(const UText* ut) {
    Bridge bridge;
    return utext_getNativeIndex(Open(ut, bridge));
}

void APS5_VABI utext_setNativeIndex_nid_postfix(UText* ut, std::int64_t nativeIndex) {
    Bridge bridge;
    utext_setNativeIndex(Open(ut, bridge), nativeIndex);
}

std::int8_t APS5_VABI utext_moveIndex32_nid_postfix(UText* ut, std::int32_t delta) {
    Bridge bridge;
    return utext_moveIndex32(Open(ut, bridge), delta);
}

std::int64_t APS5_VABI utext_getPreviousNativeIndex_nid_postfix(UText* ut) {
    Bridge bridge;
    return utext_getPreviousNativeIndex(Open(ut, bridge));
}

std::int32_t APS5_VABI utext_extract_nid_postfix(UText* ut, std::int64_t nativeStart, std::int64_t nativeLimit, char16_t* dest, std::int32_t destCapacity, std::int32_t* status) {
    UErrorCode* error = Status(status);
    Bridge bridge;
    return utext_extract(Open(ut, bridge), nativeStart, nativeLimit, dest, destCapacity, error);
}

std::int8_t APS5_VABI utext_isWritable_nid_postfix(const UText* ut) {
    return utext_isWritable(Readable(ut));
}

std::int8_t APS5_VABI utext_hasMetaData_nid_postfix(const UText* ut) {
    return utext_hasMetaData(Readable(ut));
}

std::int32_t APS5_VABI utext_replace_nid_postfix(UText* ut, std::int64_t nativeStart, std::int64_t nativeLimit, const char16_t* replacementText, std::int32_t replacementLength, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return 0;
    Bridge bridge;
    if (Writable(Open(ut, bridge)) && ut->pFuncs->replace == nullptr) APS5_INVALID_ARG_EX;
    return utext_replace(ut, nativeStart, nativeLimit, replacementText, replacementLength, error);
}

void APS5_VABI utext_copy_nid_postfix(UText* ut, std::int64_t nativeStart, std::int64_t nativeLimit, std::int64_t destIndex, std::int8_t move, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return;
    Bridge bridge;
    if (Writable(Open(ut, bridge)) && ut->pFuncs->copy == nullptr) APS5_INVALID_ARG_EX;
    utext_copy(ut, nativeStart, nativeLimit, destIndex, move, error);
}

void APS5_VABI utext_freeze_nid_postfix(UText* ut) {
    utext_freeze(Readable(ut));
}

void APS5_VABI ubrk_setUText_nid_postfix(UBreakIterator* bi, UText* text, std::int32_t* status) {
    UErrorCode* error = Status(status);
    IcuBackend::RequireObject(bi);
    Bridge bridge;
    if (U_SUCCESS(*error)) Open(text, bridge);
    ubrk_setUText(bi, text, error);
}

void APS5_VABI ubrk_refreshUText_nid_postfix(UBreakIterator* bi, UText* text, std::int32_t* status) {
    UErrorCode* error = Status(status);
    IcuBackend::RequireObject(bi);
    Bridge bridge;
    if (U_SUCCESS(*error) && text != nullptr) Open(text, bridge);
    ubrk_refreshUText(bi, text, error);
}

}
