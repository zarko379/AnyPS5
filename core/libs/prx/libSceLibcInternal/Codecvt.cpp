#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr int CodecvtNoconv = 3;

int NoUnshift(char* first2, char** next2, const char* function) {
    if (next2 == nullptr) throw std::invalid_argument(std::string(function) + ": null next pointer");
    *next2 = first2;
    return CodecvtNoconv;
}

}

extern "C" {

int APS5_VABI sceLibcInternalCodecvtChar32DoUnshift_nid_no_patch(const GuestLocale::Facet*, std::uint64_t*, char* first2, char*, char** next2) {
    return NoUnshift(first2, next2, "codecvt<char32_t, char>::do_unshift");
}
APS5_EXPORT("c6Lyc6xOp4o", sceLibcInternalCodecvtChar32DoUnshift_nid_no_patch);

}
