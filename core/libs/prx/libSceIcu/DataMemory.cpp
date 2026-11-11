#include "prx/libSceIcu/DataMemory.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/udata.h>
#include "ucmndata.h"
#include "udatamem.h"
#include <cstddef>
#include <cstring>

namespace {

static_assert(sizeof(UDataInfo) == sizeof(GuestDataInfo) && sizeof(UDataInfo) == 20);
static_assert(offsetof(UDataInfo, size) == offsetof(GuestDataInfo, size) && offsetof(UDataInfo, reservedWord) == offsetof(GuestDataInfo, reservedWord));
static_assert(offsetof(UDataInfo, isBigEndian) == offsetof(GuestDataInfo, isBigEndian) && offsetof(UDataInfo, charsetFamily) == offsetof(GuestDataInfo, charsetFamily));
static_assert(offsetof(UDataInfo, sizeofUChar) == offsetof(GuestDataInfo, sizeofUChar) && offsetof(UDataInfo, reservedByte) == offsetof(GuestDataInfo, reservedByte));
static_assert(offsetof(UDataInfo, dataFormat) == offsetof(GuestDataInfo, dataFormat) && offsetof(UDataInfo, formatVersion) == offsetof(GuestDataInfo, formatVersion));
static_assert(offsetof(UDataInfo, dataVersion) == offsetof(GuestDataInfo, dataVersion));

bool IcuTree(const char* path) {
    return !path || std::strcmp(path, U_ICUDATA_ALIAS) == 0 || std::strncmp(path, U_ICUDATA_NAME U_TREE_SEPARATOR_STRING, std::strlen(U_ICUDATA_NAME U_TREE_SEPARATOR_STRING)) == 0 ||
           std::strncmp(path, U_ICUDATA_ALIAS U_TREE_SEPARATOR_STRING, std::strlen(U_ICUDATA_ALIAS U_TREE_SEPARATOR_STRING)) == 0;
}

void CheckSeparators(const char* path) {
    if (path && (std::strchr(path, '/') || std::strchr(path, '\\'))) NotImplemented_nid_no_patch("libSceIcu: a data path with a separator (ICU 68.2 splits it at the host's file separator)");
}

struct AcceptableForward {
    GuestDataAcceptable function;
    void* context;
};

UBool U_CALLCONV ForwardAcceptable(void* context, const char* type, const char* name, const UDataInfo* info) {
    const auto* forward = static_cast<const AcceptableForward*>(context);
    return forward->function(forward->context, type, name, reinterpret_cast<const GuestDataInfo*>(info));
}

template<typename TOpen>
UDataMemory* Open(const char* path, const char* type, const char* name, bool proceeds, std::int32_t* status, TOpen open) {
    auto* error = reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
    if (!proceeds || U_FAILURE(*error) || !name || !*name) return open(error);
    CheckSeparators(path);
    const bool tree = IcuTree(path);
    if (tree && !type) APS5_INVALID_ARG_EX;
    UDataMemory* data = open(error);
    if (!tree && !data) NotImplemented_nid_no_patch("libSceIcu: a data package that the guest did not register in memory (ICU would look for it in the file system)");
    return data;
}

}

extern "C" {

UDataMemory* APS5_VABI udata_open_nid_postfix(const char* path, const char* type, const char* name, std::int32_t* status) {
    return Open(path, type, name, true, status, [&](UErrorCode* error) {
        return udata_open(path, type, name, error);
    });
}

UDataMemory* APS5_VABI udata_openChoice_nid_postfix(const char* path, const char* type, const char* name, GuestDataAcceptable isAcceptable, void* context, std::int32_t* status) {
    AcceptableForward forward{isAcceptable, context};
    return Open(path, type, name, isAcceptable != nullptr, status, [&](UErrorCode* error) {
        return udata_openChoice(path, type, name, isAcceptable ? ForwardAcceptable : nullptr, &forward, error);
    });
}

void APS5_VABI udata_close_nid_postfix(UDataMemory* data) {
    IcuBackend::Ensure();
    udata_close(data);
}

const void* APS5_VABI udata_getMemory_nid_postfix(UDataMemory* data) {
    IcuBackend::Ensure();
    return udata_getMemory(data);
}

const void* APS5_VABI udata_getRawMemory_nid_postfix(const UDataMemory* data) {
    IcuBackend::Ensure();
    return udata_getRawMemory(data);
}

std::int32_t APS5_VABI udata_getLength_nid_postfix(const UDataMemory* data) {
    IcuBackend::Ensure();
    return udata_getLength(data);
}

void APS5_VABI udata_getInfo_nid_postfix(UDataMemory* data, GuestDataInfo* info) {
    IcuBackend::Ensure();
    if (info && data && data->pHeader && info->size < 2) APS5_INVALID_ARG_EX;
    udata_getInfo(data, reinterpret_cast<UDataInfo*>(info));
}

void APS5_VABI udata_setAppData_nid_postfix(const char* path, const void* data, std::int32_t* status) {
    IcuBackend::Ensure();
    auto* error = reinterpret_cast<UErrorCode*>(status);
    if (error && U_SUCCESS(*error) && data) {
        UDataMemory probe;
        UDataMemory_init(&probe);
        UDataMemory_setData(&probe, data);
        UErrorCode probeStatus = U_ZERO_ERROR;
        udata_checkCommonData(&probe, &probeStatus);
        if (U_SUCCESS(probeStatus)) {
            if (!path) APS5_INVALID_ARG_EX;
            CheckSeparators(path);
            if (std::strcmp(path, U_ICUDATA_NAME) == 0) NotImplemented_nid_no_patch("libSceIcu: a guest package under ICU's own data name (ICU 68.2 can add it to its data, whose name on the console is unknown)");
        }
    }
    udata_setAppData(path, data, error);
}

}
