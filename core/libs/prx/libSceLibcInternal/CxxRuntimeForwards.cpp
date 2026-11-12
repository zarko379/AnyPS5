#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <system_error>

extern "C" {

void APS5_VABI sceLibcInternalSizedNothrowDelete_nid_no_patch(void* pointer, std::size_t size, const void*) noexcept {
    _ZdlPvm_nid_postfix(pointer, size);
}
APS5_EXPORT("7VPIYFpwU2A", sceLibcInternalSizedNothrowDelete_nid_no_patch);

void APS5_VABI sceLibcInternalSizedNothrowArrayDelete_nid_no_patch(void* pointer, std::size_t size, const void*) noexcept {
    _ZdaPvm_nid_postfix(pointer, size);
}
APS5_EXPORT("7lCihI18N9I", sceLibcInternalSizedNothrowArrayDelete_nid_no_patch);

const std::error_category* APS5_VABI sceLibcInternalIostreamCategory_nid_no_patch() {
    return IostreamCategory_nid_no_patch();
}
APS5_EXPORT("V23qt24VPVs", sceLibcInternalIostreamCategory_nid_no_patch);

float APS5_VABI _ZSt8_XLgammaf_nid_postfix(float x) {
    return _ZNSt8__sce_v28_XLgammaEf_nid_postfix(x);
}

double APS5_VABI _ZSt8_XLgammad_nid_postfix(double x) {
    return _ZNSt8__sce_v28_XLgammaEd_nid_postfix(x);
}

long double APS5_VABI _ZSt8_XLgammae_nid_postfix(long double x) {
    return _ZNSt8__sce_v28_XLgammaEe_nid_postfix(x);
}

}
