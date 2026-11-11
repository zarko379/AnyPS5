#include "prx/libSceIcu/Converter.hpp"
#include "prx/libSceIcu/ConverterCallbacks.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/ucnv.h>
#include <unicode/ucnv_cb.h>
#include <unicode/ucnv_err.h>
#include <unicode/ustring.h>
#include "ucnv_bld.h"
#include "ucnv_imp.h"
#include "ucnv_io.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {

UErrorCode* Status(std::int32_t* status) {
    return reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
}

UErrorCode* Optional(std::int32_t* status) {
    IcuBackend::Ensure();
    return reinterpret_cast<UErrorCode*>(status);
}

bool Proceeds(const std::int32_t* status) {
    return status && *status <= U_ZERO_ERROR;
}

template<typename TConverter>
TConverter* Object(TConverter* converter) {
    IcuBackend::RequireObject(converter);
    return converter;
}

UBool* Flag(std::int8_t* flag) {
    return reinterpret_cast<UBool*>(flag);
}

struct GuestFromUnicodeArgs {
    std::uint16_t size;
    std::int8_t flush;
    UConverter* converter;
    const char16_t* source;
    const char16_t* sourceLimit;
    char* target;
    const char* targetLimit;
    std::int32_t* offsets;
};

struct GuestToUnicodeArgs {
    std::uint16_t size;
    std::int8_t flush;
    UConverter* converter;
    const char* source;
    const char* sourceLimit;
    char16_t* target;
    const char16_t* targetLimit;
    std::int32_t* offsets;
};

static_assert(sizeof(UBool) == 1 && sizeof(UChar) == 2 && sizeof(UChar32) == 4 && sizeof(UConverterCallbackReason) == 4);
static_assert(offsetof(GuestFromUnicodeArgs, size) == 0 && offsetof(UConverterFromUnicodeArgs, size) == 0);
static_assert(offsetof(GuestFromUnicodeArgs, flush) == 2 && offsetof(UConverterFromUnicodeArgs, flush) == 2);
static_assert(offsetof(GuestFromUnicodeArgs, converter) == 8 && offsetof(UConverterFromUnicodeArgs, converter) == 8);
static_assert(offsetof(GuestFromUnicodeArgs, source) == 16 && offsetof(UConverterFromUnicodeArgs, source) == 16);
static_assert(offsetof(GuestFromUnicodeArgs, sourceLimit) == 24 && offsetof(UConverterFromUnicodeArgs, sourceLimit) == 24);
static_assert(offsetof(GuestFromUnicodeArgs, target) == 32 && offsetof(UConverterFromUnicodeArgs, target) == 32);
static_assert(offsetof(GuestFromUnicodeArgs, targetLimit) == 40 && offsetof(UConverterFromUnicodeArgs, targetLimit) == 40);
static_assert(offsetof(GuestFromUnicodeArgs, offsets) == 48 && offsetof(UConverterFromUnicodeArgs, offsets) == 48);
static_assert(sizeof(GuestFromUnicodeArgs) == 56 && sizeof(UConverterFromUnicodeArgs) == 56);
static_assert(offsetof(GuestToUnicodeArgs, size) == 0 && offsetof(UConverterToUnicodeArgs, size) == 0);
static_assert(offsetof(GuestToUnicodeArgs, flush) == 2 && offsetof(UConverterToUnicodeArgs, flush) == 2);
static_assert(offsetof(GuestToUnicodeArgs, converter) == 8 && offsetof(UConverterToUnicodeArgs, converter) == 8);
static_assert(offsetof(GuestToUnicodeArgs, source) == 16 && offsetof(UConverterToUnicodeArgs, source) == 16);
static_assert(offsetof(GuestToUnicodeArgs, sourceLimit) == 24 && offsetof(UConverterToUnicodeArgs, sourceLimit) == 24);
static_assert(offsetof(GuestToUnicodeArgs, target) == 32 && offsetof(UConverterToUnicodeArgs, target) == 32);
static_assert(offsetof(GuestToUnicodeArgs, targetLimit) == 40 && offsetof(UConverterToUnicodeArgs, targetLimit) == 40);
static_assert(offsetof(GuestToUnicodeArgs, offsets) == 48 && offsetof(UConverterToUnicodeArgs, offsets) == 48);
static_assert(sizeof(GuestToUnicodeArgs) == 56 && sizeof(UConverterToUnicodeArgs) == 56);

template<typename TGuest>
struct Record {
    TGuest function;
    const void* context;
};

using FromRecord = Record<Icu::ConverterFromUCallback>;
using ToRecord = Record<Icu::ConverterToUCallback>;

struct Slots {
    std::unique_ptr<FromRecord> from;
    std::unique_ptr<ToRecord> to;
};

std::mutex& RecordMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<const UConverter*, Slots>& RecordSlots() {
    static std::unordered_map<const UConverter*, Slots> slots;
    return slots;
}

std::unordered_set<const void*>& LiveRecords() {
    static std::unordered_set<const void*> records;
    return records;
}

void U_EXPORT2 FromUnicodeTrampoline_nid_no_patch(const void* context, UConverterFromUnicodeArgs* arguments, const UChar* codeUnits, int32_t length, UChar32 codePoint, UConverterCallbackReason reason, UErrorCode* status) {
    const auto* record = static_cast<const FromRecord*>(context);
    const Icu::ConverterFromUCallback function = record->function;
    const void* guestContext = record->context;
    function(guestContext, reinterpret_cast<Icu::ConverterFromUnicodeArgs*>(arguments), codeUnits, length, codePoint, reason, reinterpret_cast<std::int32_t*>(status));
}

void U_EXPORT2 ToUnicodeTrampoline_nid_no_patch(const void* context, UConverterToUnicodeArgs* arguments, const char* codeUnits, int32_t length, UConverterCallbackReason reason, UErrorCode* status) {
    const auto* record = static_cast<const ToRecord*>(context);
    const Icu::ConverterToUCallback function = record->function;
    const void* guestContext = record->context;
    function(guestContext, reinterpret_cast<Icu::ConverterToUnicodeArgs*>(arguments), codeUnits, length, reason, reinterpret_cast<std::int32_t*>(status));
}

template<typename THost, typename TGuest>
struct Builtin {
    THost host;
    TGuest guest;
    bool direct;
};

struct FromSide {
    using Host = UConverterFromUCallback;
    using Guest = Icu::ConverterFromUCallback;
    using Entry = FromRecord;

    static constexpr Host Trampoline = FromUnicodeTrampoline_nid_no_patch;

    static const std::array<Builtin<Host, Guest>, 4>& Builtins() {
        static const std::array<Builtin<Host, Guest>, 4> builtins{{
            {UCNV_FROM_U_CALLBACK_SUBSTITUTE, UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix, true},
            {UCNV_FROM_U_CALLBACK_ESCAPE, UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix, true},
            {UCNV_FROM_U_CALLBACK_STOP, UCNV_FROM_U_CALLBACK_STOP_nid_postfix, false},
            {UCNV_FROM_U_CALLBACK_SKIP, UCNV_FROM_U_CALLBACK_SKIP_nid_postfix, false},
        }};
        return builtins;
    }

    static void Get(const UConverter* converter, Host* action, const void** context) {
        ucnv_getFromUCallBack(converter, action, context);
    }

    static void Set(UConverter* converter, Host action, const void* context, UErrorCode* status) {
        ucnv_setFromUCallBack(converter, action, context, nullptr, nullptr, status);
    }

    static std::unique_ptr<Entry>& Slot(Slots& slots) {
        return slots.from;
    }
};

struct ToSide {
    using Host = UConverterToUCallback;
    using Guest = Icu::ConverterToUCallback;
    using Entry = ToRecord;

    static constexpr Host Trampoline = ToUnicodeTrampoline_nid_no_patch;

    static const std::array<Builtin<Host, Guest>, 4>& Builtins() {
        static const std::array<Builtin<Host, Guest>, 4> builtins{{
            {UCNV_TO_U_CALLBACK_SUBSTITUTE, UCNV_TO_U_CALLBACK_SUBSTITUTE_nid_postfix, true},
            {UCNV_TO_U_CALLBACK_ESCAPE, UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix, true},
            {UCNV_TO_U_CALLBACK_STOP, UCNV_TO_U_CALLBACK_STOP_nid_postfix, false},
            {UCNV_TO_U_CALLBACK_SKIP, UCNV_TO_U_CALLBACK_SKIP_nid_postfix, false},
        }};
        return builtins;
    }

    static void Get(const UConverter* converter, Host* action, const void** context) {
        ucnv_getToUCallBack(converter, action, context);
    }

    static void Set(UConverter* converter, Host action, const void* context, UErrorCode* status) {
        ucnv_setToUCallBack(converter, action, context, nullptr, nullptr, status);
    }

    static std::unique_ptr<Entry>& Slot(Slots& slots) {
        return slots.to;
    }
};

void EraseEmpty(const UConverter* converter) {
    auto found = RecordSlots().find(converter);
    if (found != RecordSlots().end() && !found->second.from && !found->second.to) RecordSlots().erase(found);
}

template<typename TSide>
std::pair<typename TSide::Guest, const void*> GuestView(typename TSide::Host action, const void* context) {
    if (action == TSide::Trampoline) {
        const std::lock_guard lock(RecordMutex());
        if (!LiveRecords().count(context)) APS5_INVALID_ARG_EX;
        const auto* record = static_cast<const typename TSide::Entry*>(context);
        return {record->function, record->context};
    }
    for (const auto& builtin : TSide::Builtins())
        if (builtin.host == action) return {builtin.guest, context};
    throw std::logic_error("libSceIcu: a converter callback that ICU 68.2 never installs");
}

template<typename TSide>
void GetCallback(const UConverter* converter, typename TSide::Guest* action, const void** context) {
    IcuBackend::Ensure();
    Object(converter);
    Object(action);
    Object(context);
    typename TSide::Host hostAction = nullptr;
    const void* hostContext = nullptr;
    TSide::Get(converter, &hostAction, &hostContext);
    const auto view = GuestView<TSide>(hostAction, hostContext);
    *action = view.first;
    *context = view.second;
}

template<typename TSide>
void SetCallback(UConverter* converter, typename TSide::Guest action, const void* context, typename TSide::Guest* oldAction, const void** oldContext, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return;
    Object(converter);
    if (!action) APS5_INVALID_ARG_EX;
    typename TSide::Host currentAction = nullptr;
    const void* currentContext = nullptr;
    TSide::Get(converter, &currentAction, &currentContext);
    std::pair<typename TSide::Guest, const void*> previous{nullptr, nullptr};
    if (oldAction || oldContext) previous = GuestView<TSide>(currentAction, currentContext);
    typename TSide::Host hostAction = TSide::Trampoline;
    const void* hostContext = context;
    std::unique_ptr<typename TSide::Entry> record;
    for (const auto& builtin : TSide::Builtins())
        if (builtin.direct && builtin.guest == action) hostAction = builtin.host;
    if (hostAction == TSide::Trampoline) {
        record = std::make_unique<typename TSide::Entry>(typename TSide::Entry{action, context});
        hostContext = record.get();
        const std::lock_guard lock(RecordMutex());
        LiveRecords().insert(record.get());
    }
    TSide::Set(converter, hostAction, hostContext, error);
    std::unique_ptr<typename TSide::Entry> released;
    {
        const std::lock_guard lock(RecordMutex());
        auto& slot = TSide::Slot(RecordSlots()[converter]);
        released = std::move(slot);
        slot = std::move(record);
        if (released) LiveRecords().erase(released.get());
        EraseEmpty(converter);
    }
    if (oldAction) *oldAction = previous.first;
    if (oldContext) *oldContext = previous.second;
}

Slots Release(const UConverter* converter) {
    const std::lock_guard lock(RecordMutex());
    auto found = RecordSlots().find(converter);
    if (found == RecordSlots().end()) return {};
    Slots slots = std::move(found->second);
    RecordSlots().erase(found);
    return slots;
}

void Forget(Slots slots) {
    const std::lock_guard lock(RecordMutex());
    if (slots.from) LiveRecords().erase(slots.from.get());
    if (slots.to) LiveRecords().erase(slots.to.get());
}

UConverter* Fresh(UConverter* converter) {
    if (converter) Forget(Release(converter));
    return converter;
}

template<typename TSide>
void OwnClone(UConverter* clone) {
    typename TSide::Host action = nullptr;
    const void* context = nullptr;
    TSide::Get(clone, &action, &context);
    std::unique_ptr<typename TSide::Entry> released;
    const void* own = nullptr;
    {
        const std::lock_guard lock(RecordMutex());
        std::unique_ptr<typename TSide::Entry> record;
        if (action == TSide::Trampoline) {
            if (!LiveRecords().count(context)) APS5_INVALID_ARG_EX;
            record = std::make_unique<typename TSide::Entry>(*static_cast<const typename TSide::Entry*>(context));
            LiveRecords().insert(record.get());
        }
        own = record.get();
        auto& slot = TSide::Slot(RecordSlots()[clone]);
        released = std::move(slot);
        if (released) LiveRecords().erase(released.get());
        slot = std::move(record);
        EraseEmpty(clone);
    }
    if (!own) return;
    UErrorCode status = U_ZERO_ERROR;
    TSide::Set(clone, TSide::Trampoline, own, &status);
}

UConverterFromUnicodeArgs* FromArgs(Icu::ConverterFromUnicodeArgs* arguments) {
    return reinterpret_cast<UConverterFromUnicodeArgs*>(arguments);
}

UConverterToUnicodeArgs* ToArgs(Icu::ConverterToUnicodeArgs* arguments) {
    return reinterpret_cast<UConverterToUnicodeArgs*>(arguments);
}

template<typename TArgs>
TArgs* WithConverter(TArgs* arguments) {
    IcuBackend::RequireObject(Object(arguments)->converter);
    return arguments;
}

bool Ignorable(std::int32_t c) {
    return c == 0x00ad || c == 0x034f || c == 0x061c || c == 0x115f || c == 0x1160 || (0x17b4 <= c && c <= 0x17b5) ||
        (0x180b <= c && c <= 0x180e) || (0x200b <= c && c <= 0x200f) || (0x202a <= c && c <= 0x202e) ||
        (0x2060 <= c && c <= 0x206f) || c == 0x3164 || (0xfe00 <= c && c <= 0xfe0f) || c == 0xfeff || c == 0xffa0 ||
        (0xfff0 <= c && c <= 0xfff8) || (0x1bca0 <= c && c <= 0x1bca3) || (0x1d173 <= c && c <= 0x1d17a) || (0xe0000 <= c && c <= 0xe0fff);
}

bool Substitutes(const void* context, std::int32_t reason) {
    return !context || (*static_cast<const char*>(context) == 'i' && reason == UCNV_UNASSIGNED);
}

template<typename TArgs>
void Overflow(const TArgs* arguments, std::int32_t length, std::size_t unit) {
    const auto target = reinterpret_cast<std::uintptr_t>(arguments->target);
    const auto limit = reinterpret_cast<std::uintptr_t>(arguments->targetLimit);
    const std::int64_t room = limit > target ? static_cast<std::int64_t>((limit - target + unit - 1) / unit) : 0;
    if (arguments->converter && length - room > UCNV_ERROR_BUFFER_LENGTH) APS5_INVALID_ARG_EX;
}

std::int32_t Digits(std::uint32_t value, std::uint32_t radix) {
    std::int32_t digits = 1;
    while (value >= radix) {
        value /= radix;
        ++digits;
    }
    return digits;
}

std::int32_t ToEscapeLength(const void* context, const char* codeUnits, std::int32_t length) {
    const char kind = context ? *static_cast<const char*>(context) : 0;
    std::int32_t total = 0;
    for (std::int32_t index = 0; index < length; ++index) {
        const auto byte = static_cast<std::uint8_t>(codeUnits[index]);
        if (kind == 'D') total += 3 + Digits(byte, 10);
        else if (kind == 'X') total += 4 + Digits(byte, 16);
        else total += 4;
    }
    return total;
}

void OpenUName(const char16_t* name) {
    IcuBackend::Converter(nullptr);
    if (!name) return;
    UErrorCode probe = U_ZERO_ERROR;
    UConverter* converter = ucnv_open(nullptr, &probe);
    if (U_FAILURE(probe)) {
        ucnv_close(converter);
        return;
    }
    const std::int32_t length = ucnv_fromUChars(converter, nullptr, 0, name, -1, &probe);
    ucnv_close(converter);
    if (length >= UCNV_MAX_CONVERTER_NAME_LENGTH) APS5_INVALID_ARG_EX;
}

}

extern "C" {

UConverter* APS5_VABI ucnv_open_nid_postfix(const char* name, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    if (!Proceeds(status)) return nullptr;
    IcuBackend::Converter(name);
    return Fresh(ucnv_open(name, error));
}

UConverter* APS5_VABI ucnv_openU_nid_postfix(const char16_t* name, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    if (!Proceeds(status)) return nullptr;
    if (!name || u_strlen(name) < UCNV_MAX_CONVERTER_NAME_LENGTH) OpenUName(name);
    return Fresh(ucnv_openU(name, error));
}

UConverter* APS5_VABI ucnv_openCCSID_nid_postfix(std::int32_t codepage, std::int32_t platform, std::int32_t* status) {
    return Fresh(ucnv_openCCSID(codepage, static_cast<UConverterPlatform>(platform), Optional(status)));
}

UConverter* APS5_VABI ucnv_openPackage_nid_postfix(const char* packageName, const char* converterName, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    IcuBackend::RequireObject(converterName);
    return Fresh(ucnv_openPackage(IcuBackend::Package(packageName), converterName, error));
}

UConverter* APS5_VABI ucnv_createConverter_nid_postfix(UConverter* fillIn, const char* name, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    if (fillIn) NotImplemented_nid_no_patch("libSceIcu: ucnv_createConverter into caller memory (ICU 68.2's internal UConverter layout)");
    IcuBackend::Converter(name);
    return Fresh(ucnv_createConverter(nullptr, name, error));
}

void APS5_VABI ucnv_close_nid_postfix(UConverter* converter) {
    IcuBackend::Ensure();
    Slots slots = Release(converter);
    ucnv_close(converter);
    Forget(std::move(slots));
}

void APS5_VABI ucnv_getSubstChars_nid_postfix(const UConverter* converter, char* subChars, std::int8_t* length, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return;
    Object(converter);
    Object(length);
    if (converter->subCharLen > 0 && *length >= converter->subCharLen) IcuBackend::RequireObject(subChars);
    ucnv_getSubstChars(converter, subChars, length, error);
}

void APS5_VABI ucnv_setSubstChars_nid_postfix(UConverter* converter, const char* subChars, std::int8_t length, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return;
    const UConverterStaticData* data = Object(converter)->sharedData->staticData;
    if (length <= data->maxBytesPerChar && length >= data->minBytesPerChar && length > 0) IcuBackend::RequireObject(subChars);
    ucnv_setSubstChars(converter, subChars, length, error);
}

void APS5_VABI ucnv_setSubstString_nid_postfix(UConverter* converter, const char16_t* text, std::int32_t length, std::int32_t* status) {
    ucnv_setSubstString(converter, text, length, Status(status));
}

void APS5_VABI ucnv_getInvalidChars_nid_postfix(const UConverter* converter, char* bytes, std::int8_t* length, std::int32_t* status) {
    ucnv_getInvalidChars(converter, bytes, length, Optional(status));
}

void APS5_VABI ucnv_getInvalidUChars_nid_postfix(const UConverter* converter, char16_t* units, std::int8_t* length, std::int32_t* status) {
    ucnv_getInvalidUChars(converter, units, length, Optional(status));
}

void APS5_VABI ucnv_reset_nid_postfix(UConverter* converter) {
    IcuBackend::Ensure();
    ucnv_reset(converter);
}

void APS5_VABI ucnv_resetToUnicode_nid_postfix(UConverter* converter) {
    IcuBackend::Ensure();
    ucnv_resetToUnicode(converter);
}

void APS5_VABI ucnv_resetFromUnicode_nid_postfix(UConverter* converter) {
    IcuBackend::Ensure();
    ucnv_resetFromUnicode(converter);
}

std::int8_t APS5_VABI ucnv_getMaxCharSize_nid_postfix(const UConverter* converter) {
    return ucnv_getMaxCharSize(Object(converter));
}

std::int8_t APS5_VABI ucnv_getMinCharSize_nid_postfix(const UConverter* converter) {
    return ucnv_getMinCharSize(Object(converter));
}

const char* APS5_VABI ucnv_getName_nid_postfix(const UConverter* converter, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return nullptr;
    return ucnv_getName(Object(converter), error);
}

std::int32_t APS5_VABI ucnv_getCCSID_nid_postfix(const UConverter* converter, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return -1;
    return ucnv_getCCSID(Object(converter), error);
}

std::int32_t APS5_VABI ucnv_getPlatform_nid_postfix(const UConverter* converter, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return UCNV_UNKNOWN;
    return ucnv_getPlatform(Object(converter), error);
}

std::int32_t APS5_VABI ucnv_getType_nid_postfix(const UConverter* converter) {
    return ucnv_getType(Object(converter));
}

void APS5_VABI ucnv_getStarters_nid_postfix(const UConverter* converter, std::int8_t* starters, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    if (!Proceeds(status)) return;
    if (Object(converter)->sharedData->impl->getStarters) IcuBackend::RequireObject(starters);
    ucnv_getStarters(converter, Flag(starters), error);
}

void APS5_VABI ucnv_fixFileSeparator_nid_postfix(const UConverter* converter, char16_t* source, std::int32_t length) {
    IcuBackend::Ensure();
    ucnv_fixFileSeparator(converter, source, length);
}

std::int8_t APS5_VABI ucnv_isAmbiguous_nid_postfix(const UConverter* converter) {
    IcuBackend::Ensure();
    return ucnv_isAmbiguous(converter);
}

void APS5_VABI ucnv_setFallback_nid_postfix(UConverter* converter, std::int8_t usesFallback) {
    ucnv_setFallback(Object(converter), usesFallback);
}

std::int8_t APS5_VABI ucnv_usesFallback_nid_postfix(const UConverter* converter) {
    return ucnv_usesFallback(Object(converter));
}

std::int32_t APS5_VABI ucnv_fromUCountPending_nid_postfix(const UConverter* converter, std::int32_t* status) {
    return ucnv_fromUCountPending(converter, Optional(status));
}

std::int32_t APS5_VABI ucnv_toUCountPending_nid_postfix(const UConverter* converter, std::int32_t* status) {
    return ucnv_toUCountPending(converter, Optional(status));
}

std::int8_t APS5_VABI ucnv_isFixedWidth_nid_postfix(UConverter* converter, std::int32_t* status) {
    return ucnv_isFixedWidth(converter, Status(status));
}

void APS5_VABI ucnv_fromUnicode_nid_postfix(UConverter* converter, char** target, const char* targetLimit, const char16_t** source, const char16_t* sourceLimit, std::int32_t* offsets, std::int8_t flush, std::int32_t* status) {
    ucnv_fromUnicode(converter, target, targetLimit, source, sourceLimit, offsets, flush, Optional(status));
}

void APS5_VABI ucnv_toUnicode_nid_postfix(UConverter* converter, char16_t** target, const char16_t* targetLimit, const char** source, const char* sourceLimit, std::int32_t* offsets, std::int8_t flush, std::int32_t* status) {
    ucnv_toUnicode(converter, target, targetLimit, source, sourceLimit, offsets, flush, Optional(status));
}

std::int32_t APS5_VABI ucnv_fromUChars_nid_postfix(UConverter* converter, char* destination, std::int32_t capacity, const char16_t* source, std::int32_t length, std::int32_t* status) {
    return ucnv_fromUChars(converter, destination, capacity, source, length, Optional(status));
}

std::int32_t APS5_VABI ucnv_toUChars_nid_postfix(UConverter* converter, char16_t* destination, std::int32_t capacity, const char* source, std::int32_t length, std::int32_t* status) {
    return ucnv_toUChars(converter, destination, capacity, source, length, Optional(status));
}

std::int32_t APS5_VABI ucnv_getNextUChar_nid_postfix(UConverter* converter, const char** source, const char* sourceLimit, std::int32_t* status) {
    return ucnv_getNextUChar(converter, source, sourceLimit, Optional(status));
}

void APS5_VABI ucnv_convertEx_nid_postfix(UConverter* targetConverter, UConverter* sourceConverter, char** target, const char* targetLimit, const char** source, const char* sourceLimit, char16_t* pivotStart, char16_t** pivotSource, char16_t** pivotTarget, const char16_t* pivotLimit, std::int8_t reset, std::int8_t flush, std::int32_t* status) {
    ucnv_convertEx(targetConverter, sourceConverter, target, targetLimit, source, sourceLimit, pivotStart, pivotSource, pivotTarget, pivotLimit, reset, flush, Optional(status));
}

std::int32_t APS5_VABI ucnv_convert_nid_postfix(const char* toName, const char* fromName, char* target, std::int32_t capacity, const char* source, std::int32_t length, std::int32_t* status) {
    UErrorCode* error = Optional(status);
    if (Proceeds(status) && source && length >= -1 && capacity >= 0 && (capacity == 0 || target) && length != 0 && (length > 0 || *source != 0)) {
        IcuBackend::Converter(fromName);
        if (!toName) {
            UErrorCode probe = U_ZERO_ERROR;
            ucnv_close(ucnv_open(fromName, &probe));
            if (U_SUCCESS(probe)) IcuBackend::Converter(toName);
        }
    }
    return ucnv_convert(toName, fromName, target, capacity, source, length, error);
}

std::int32_t APS5_VABI ucnv_toAlgorithmic_nid_postfix(std::int32_t type, UConverter* converter, char* target, std::int32_t capacity, const char* source, std::int32_t length, std::int32_t* status) {
    return ucnv_toAlgorithmic(static_cast<UConverterType>(type), converter, target, capacity, source, length, Optional(status));
}

std::int32_t APS5_VABI ucnv_fromAlgorithmic_nid_postfix(UConverter* converter, std::int32_t type, char* target, std::int32_t capacity, const char* source, std::int32_t length, std::int32_t* status) {
    return ucnv_fromAlgorithmic(converter, static_cast<UConverterType>(type), target, capacity, source, length, Optional(status));
}

UConverter* APS5_VABI ucnv_safeClone_nid_postfix(const UConverter* converter, void* stackBuffer, std::int32_t* bufferSize, std::int32_t* status) {
    UConverter* clone = ucnv_safeClone(converter, stackBuffer, bufferSize, Optional(status));
    if (!clone) return clone;
    OwnClone<FromSide>(clone);
    OwnClone<ToSide>(clone);
    return clone;
}

void APS5_VABI ucnv_getFromUCallBack_nid_postfix(const UConverter* converter, Icu::ConverterFromUCallback* action, const void** context) {
    GetCallback<FromSide>(converter, action, context);
}

void APS5_VABI ucnv_getToUCallBack_nid_postfix(const UConverter* converter, Icu::ConverterToUCallback* action, const void** context) {
    GetCallback<ToSide>(converter, action, context);
}

void APS5_VABI ucnv_setFromUCallBack_nid_postfix(UConverter* converter, Icu::ConverterFromUCallback action, const void* context, Icu::ConverterFromUCallback* oldAction, const void** oldContext, std::int32_t* status) {
    SetCallback<FromSide>(converter, action, context, oldAction, oldContext, status);
}

void APS5_VABI ucnv_setToUCallBack_nid_postfix(UConverter* converter, Icu::ConverterToUCallback action, const void* context, Icu::ConverterToUCallback* oldAction, const void** oldContext, std::int32_t* status) {
    SetCallback<ToSide>(converter, action, context, oldAction, oldContext, status);
}

void APS5_VABI UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix(const void* context, Icu::ConverterFromUnicodeArgs* arguments, const char16_t* codeUnits, std::int32_t length, std::int32_t codePoint, std::int32_t reason, std::int32_t* status) {
    IcuBackend::Ensure();
    if (reason <= UCNV_IRREGULAR) {
        const bool ignorable = reason == UCNV_UNASSIGNED && Ignorable(codePoint);
        if (ignorable || Substitutes(context, reason)) Object(status);
        if (!ignorable && Substitutes(context, reason)) WithConverter(FromArgs(arguments));
    }
    UCNV_FROM_U_CALLBACK_SUBSTITUTE(context, FromArgs(arguments), codeUnits, length, codePoint, static_cast<UConverterCallbackReason>(reason), reinterpret_cast<UErrorCode*>(status));
}

void APS5_VABI UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix(const void* context, Icu::ConverterFromUnicodeArgs* arguments, const char16_t* codeUnits, std::int32_t length, std::int32_t codePoint, std::int32_t reason, std::int32_t* status) {
    IcuBackend::Ensure();
    if (reason <= UCNV_IRREGULAR) {
        Object(status);
        if (!(reason == UCNV_UNASSIGNED && Ignorable(codePoint))) {
            WithConverter(FromArgs(arguments));
            const char kind = context ? *static_cast<const char*>(context) : 0;
            const bool perUnit = !context || (kind != 'C' && kind != 'D' && kind != 'X' && kind != 'U' && kind != 'S');
            if (perUnit && length > 8) APS5_INVALID_ARG_EX;
            if ((perUnit && length > 0) || (!perUnit && kind != 'S' && length != 2)) IcuBackend::RequireObject(codeUnits);
        }
    }
    UCNV_FROM_U_CALLBACK_ESCAPE(context, FromArgs(arguments), codeUnits, length, codePoint, static_cast<UConverterCallbackReason>(reason), reinterpret_cast<UErrorCode*>(status));
}

void APS5_VABI UCNV_TO_U_CALLBACK_SUBSTITUTE_nid_postfix(const void* context, Icu::ConverterToUnicodeArgs* arguments, const char* codeUnits, std::int32_t length, std::int32_t reason, std::int32_t* status) {
    IcuBackend::Ensure();
    if (reason <= UCNV_IRREGULAR && Substitutes(context, reason)) {
        Object(status);
        WithConverter(ToArgs(arguments));
    }
    UCNV_TO_U_CALLBACK_SUBSTITUTE(context, ToArgs(arguments), codeUnits, length, static_cast<UConverterCallbackReason>(reason), reinterpret_cast<UErrorCode*>(status));
}

void APS5_VABI UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix(const void* context, Icu::ConverterToUnicodeArgs* arguments, const char* codeUnits, std::int32_t length, std::int32_t reason, std::int32_t* status) {
    IcuBackend::Ensure();
    if (reason <= UCNV_IRREGULAR) {
        Object(status);
        WithConverter(ToArgs(arguments));
        if (length > 0) IcuBackend::RequireObject(codeUnits);
        if (ToEscapeLength(context, codeUnits, length) > 48) APS5_INVALID_ARG_EX;
    }
    UCNV_TO_U_CALLBACK_ESCAPE(context, ToArgs(arguments), codeUnits, length, static_cast<UConverterCallbackReason>(reason), reinterpret_cast<UErrorCode*>(status));
}

void APS5_VABI ucnv_cbFromUWriteBytes_nid_postfix(Icu::ConverterFromUnicodeArgs* arguments, const char* source, std::int32_t length, std::int32_t offsetIndex, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return;
    UConverterFromUnicodeArgs* args = Object(FromArgs(arguments));
    if (length > 0) IcuBackend::RequireObject(source);
    Overflow(args, length, 1);
    ucnv_cbFromUWriteBytes(args, source, length, offsetIndex, error);
}

void APS5_VABI ucnv_cbFromUWriteSub_nid_postfix(Icu::ConverterFromUnicodeArgs* arguments, std::int32_t offsetIndex, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return;
    ucnv_cbFromUWriteSub(WithConverter(FromArgs(arguments)), offsetIndex, error);
}

void APS5_VABI ucnv_cbFromUWriteUChars_nid_postfix(Icu::ConverterFromUnicodeArgs* arguments, const char16_t** source, const char16_t* sourceLimit, std::int32_t offsetIndex, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return;
    ucnv_cbFromUWriteUChars(Object(FromArgs(arguments)), source, sourceLimit, offsetIndex, error);
}

void APS5_VABI ucnv_cbToUWriteUChars_nid_postfix(Icu::ConverterToUnicodeArgs* arguments, const char16_t* source, std::int32_t length, std::int32_t offsetIndex, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (U_FAILURE(*error)) return;
    UConverterToUnicodeArgs* args = Object(ToArgs(arguments));
    if (length > 0) IcuBackend::RequireObject(source);
    Overflow(args, length, sizeof(UChar));
    ucnv_cbToUWriteUChars(args, source, length, offsetIndex, error);
}

void APS5_VABI ucnv_cbToUWriteSub_nid_postfix(Icu::ConverterToUnicodeArgs* arguments, std::int32_t offsetIndex, std::int32_t* status) {
    UErrorCode* error = Status(status);
    ucnv_cbToUWriteSub(WithConverter(ToArgs(arguments)), offsetIndex, error);
}

std::int32_t APS5_VABI ucnv_flushCache_nid_postfix() {
    IcuBackend::Ensure();
    return ucnv_flushCache();
}

void APS5_VABI ucnv_enableCleanup_nid_postfix() {
    IcuBackend::Ensure();
    ucnv_enableCleanup();
}

std::int32_t APS5_VABI ucnv_countAvailable_nid_postfix() {
    IcuBackend::ConverterList();
    const std::int32_t count = ucnv_countAvailable();
    IcuBackend::ConverterListBuilt();
    return count;
}

const char* APS5_VABI ucnv_getAvailableName_nid_postfix(std::int32_t index) {
    IcuBackend::Ensure();
    if (index < 0 || index > 0xffff) return nullptr;
    IcuBackend::ConverterList();
    const char* name = ucnv_getAvailableName(index);
    IcuBackend::ConverterListBuilt();
    return name;
}

std::uint16_t APS5_VABI ucnv_countAliases_nid_postfix(const char* alias, std::int32_t* status) {
    return ucnv_countAliases(alias, Status(status));
}

const char* APS5_VABI ucnv_getAlias_nid_postfix(const char* alias, std::uint16_t index, std::int32_t* status) {
    return ucnv_getAlias(alias, index, Status(status));
}

void APS5_VABI ucnv_getAliases_nid_postfix(const char* alias, const char** aliases, std::int32_t* status) {
    UErrorCode* error = Status(status);
    if (!aliases) {
        UErrorCode probe = *error;
        if (ucnv_countAliases(alias, &probe) > 0) APS5_INVALID_ARG_EX;
    }
    ucnv_getAliases(alias, aliases, error);
}

std::uint16_t APS5_VABI ucnv_countStandards_nid_postfix() {
    IcuBackend::Ensure();
    return ucnv_countStandards();
}

const char* APS5_VABI ucnv_getStandard_nid_postfix(std::uint16_t index, std::int32_t* status) {
    return ucnv_getStandard(index, Status(status));
}

const char* APS5_VABI ucnv_getStandardName_nid_postfix(const char* name, const char* standard, std::int32_t* status) {
    return ucnv_getStandardName(name, standard, Status(status));
}

const char* APS5_VABI ucnv_getCanonicalName_nid_postfix(const char* alias, const char* standard, std::int32_t* status) {
    return ucnv_getCanonicalName(alias, standard, Status(status));
}

std::uint16_t APS5_VABI ucnv_io_countKnownConverters_nid_postfix(std::int32_t* status) {
    return ucnv_io_countKnownConverters(Status(status));
}

const char* APS5_VABI ucnv_io_getConverterName_nid_postfix(const char* alias, std::int8_t* containsOption, std::int32_t* status) {
    return ucnv_io_getConverterName(alias, Flag(containsOption), Status(status));
}

}
