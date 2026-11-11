#include "prx/libSceIcu/Backend.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/udata.h>
#include <mutex>
#include <stdexcept>

namespace IcuBackend {

void Ensure() {
    static std::once_flag once;
    std::call_once(once, [] {
        UErrorCode status = U_ZERO_ERROR;
        udata_setFileAccess(UDATA_NO_FILES, &status);
        if (U_FAILURE(status)) throw std::runtime_error("libSceIcu backend: udata_setFileAccess failed");
    });
}

std::int32_t* RequireStatus(std::int32_t* status) {
    if (!status) APS5_INVALID_ARG_EX;
    Ensure();
    return status;
}

const void* RequireObject(const void* object) {
    if (!object) APS5_INVALID_ARG_EX;
    return object;
}

}
