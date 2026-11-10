#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr int CodecvtNoconv = 3;

const GuestLocale::CodecvtVtable& Codecvt(const GuestLocale::Facet* self, const char* function) {
    if (self == nullptr || self->vtable == nullptr) throw std::invalid_argument(std::string(function) + ": null facet");
    return *reinterpret_cast<const GuestLocale::CodecvtVtable*>(self->vtable);
}

template<typename TFrom, typename TTo>
int NoConversion(const TFrom* first1, const TFrom** next1, TTo* first2, TTo** next2, const char* function) {
    if (next1 == nullptr || next2 == nullptr) throw std::invalid_argument(std::string(function) + ": null next pointer");
    *next1 = first1;
    *next2 = first2;
    return CodecvtNoconv;
}

int NoUnshift(char* first2, char** next2, const char* function) {
    if (next2 == nullptr) throw std::invalid_argument(std::string(function) + ": null next pointer");
    *next2 = first2;
    return CodecvtNoconv;
}

}

extern "C" {

bool APS5_VABI sceLibcInternalCodecvtDoAlwaysNoconv_nid_no_patch(const GuestLocale::Facet*) {
    return true;
}
APS5_EXPORT("7tIwDZyyKHo", sceLibcInternalCodecvtDoAlwaysNoconv_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtDoIn_nid_no_patch(const GuestLocale::Facet*, std::uint64_t*, const char* first1, const char*, const char** next1, char* first2, char*, char** next2) {
    return NoConversion(first1, next1, first2, next2, "codecvt<char, char>::do_in");
}
APS5_EXPORT("P+q1OLiErP0", sceLibcInternalCodecvtDoIn_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtDoOut_nid_no_patch(const GuestLocale::Facet*, std::uint64_t*, const char* first1, const char*, const char** next1, char* first2, char*, char** next2) {
    return NoConversion(first1, next1, first2, next2, "codecvt<char, char>::do_out");
}
APS5_EXPORT("Uc+-Sx0UZ3U", sceLibcInternalCodecvtDoOut_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtDoUnshift_nid_no_patch(const GuestLocale::Facet*, std::uint64_t*, char* first2, char*, char** next2) {
    return NoUnshift(first2, next2, "codecvt<char, char>::do_unshift");
}
APS5_EXPORT("iCWgjeqMHvg", sceLibcInternalCodecvtDoUnshift_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtChar32DoUnshift_nid_no_patch(const GuestLocale::Facet*, std::uint64_t*, char* first2, char*, char** next2) {
    return NoUnshift(first2, next2, "codecvt<char32_t, char>::do_unshift");
}
APS5_EXPORT("c6Lyc6xOp4o", sceLibcInternalCodecvtChar32DoUnshift_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtIn_nid_no_patch(const GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, const char** next1, char* first2, char* last2, char** next2) {
    return Codecvt(self, "codecvt<char, char>::in").in(const_cast<GuestLocale::Facet*>(self), state, first1, last1, next1, first2, last2, next2);
}
APS5_EXPORT("TNexGlwiVEQ", sceLibcInternalCodecvtIn_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtOut_nid_no_patch(const GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, const char** next1, char* first2, char* last2, char** next2) {
    return Codecvt(self, "codecvt<char, char>::out").out(const_cast<GuestLocale::Facet*>(self), state, first1, last1, next1, first2, last2, next2);
}
APS5_EXPORT("14xKj+SV72o", sceLibcInternalCodecvtOut_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtUnshift_nid_no_patch(const GuestLocale::Facet* self, std::uint64_t* state, char* first2, char* last2, char** next2) {
    return Codecvt(self, "codecvt<char, char>::unshift").unshift(const_cast<GuestLocale::Facet*>(self), state, first2, last2, next2);
}
APS5_EXPORT("N7z+Dnk1cS0", sceLibcInternalCodecvtUnshift_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtLength_nid_no_patch(const GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, std::size_t count) {
    return static_cast<int>(Codecvt(self, "codecvt<char, char>::length").length(const_cast<GuestLocale::Facet*>(self), state, first1, last1, count));
}
APS5_EXPORT("ikBt0lqkxPc", sceLibcInternalCodecvtLength_nid_no_patch);

}
