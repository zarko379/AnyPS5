#include "prx/libSceIcu/Defaults.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include <unicode/ucnv.h>
#include <unicode/uloc.h>

namespace IcuBackend {

bool SetDefaultLocale(const char* locale, std::int32_t* status) {
    return WithGuestStatus(status, "uloc_setDefault", [&](std::int32_t* local) {
        UErrorCode* error = reinterpret_cast<UErrorCode*>(local);
        if (U_FAILURE(*error)) return false;
        NewDefaultLocale(locale);
        uloc_setDefault(locale, error);
        SetGuestDefaultLocale(locale != nullptr);
        return true;
    });
}

const char* GuestDefaultLocale() {
    if (!GuestDefault()) return nullptr;
    return uloc_getDefault();
}

}

extern "C" {

void APS5_VABI ucnv_setDefaultName_nid_postfix(const char* name) {
    IcuBackend::Ensure();
    if (!name) {
        ucnv_setDefaultName(nullptr);
        IcuBackend::SetGuestDefaultConverter(false);
        return;
    }
    UErrorCode error = U_ZERO_ERROR;
    UConverter* converter = ucnv_open(name, &error);
    ucnv_close(converter);
    if (U_FAILURE(error)) return;
    ucnv_setDefaultName(name);
    IcuBackend::SetGuestDefaultConverter(true);
}

const char* APS5_VABI ucnv_getDefaultName_nid_postfix() {
    IcuBackend::Converter(nullptr);
    return ucnv_getDefaultName();
}

}
