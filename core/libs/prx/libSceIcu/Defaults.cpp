#include "prx/libSceIcu/Defaults.hpp"
#include "prx/libSceIcu/Backend.hpp"
#include <unicode/ucnv.h>
#include <unicode/uloc.h>

extern "C" {

void APS5_VABI uloc_setDefault_nid_postfix(const char* locale, std::int32_t* status) {
    UErrorCode* error = reinterpret_cast<UErrorCode*>(IcuBackend::RequireStatus(status));
    if (U_FAILURE(*error)) return;
    uloc_setDefault(locale, error);
    IcuBackend::SetGuestDefaultLocale(locale != nullptr);
}

const char* APS5_VABI uloc_getDefault_nid_postfix() {
    IcuBackend::Locale(nullptr);
    return uloc_getDefault();
}

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
