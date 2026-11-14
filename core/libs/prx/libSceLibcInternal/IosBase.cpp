#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::int32_t FormatMask = 0xffff;
constexpr std::int32_t DecimalBase = 0x200;
constexpr std::int32_t OctalBase = 0x400;
constexpr std::int32_t HexadecimalBase = 0x800;
constexpr std::int32_t BaseField = DecimalBase | OctalBase | HexadecimalBase;

void APS5_VABI SetFormatFlags_nid_no_patch(GuestLocale::IosBase* stream, std::int32_t mask) {
    if (stream == nullptr) throw std::invalid_argument("setiosflags: null stream");
    stream->flags = (stream->flags & ~mask) | (mask & FormatMask);
}

void APS5_VABI ResetFormatFlags_nid_no_patch(GuestLocale::IosBase* stream, std::int32_t mask) {
    if (stream == nullptr) throw std::invalid_argument("resetiosflags: null stream");
    stream->flags &= ~mask;
}

void APS5_VABI SetFormatBase_nid_no_patch(GuestLocale::IosBase* stream, std::int32_t base) {
    if (stream == nullptr) throw std::invalid_argument("setbase: null stream");
    const std::int32_t field = base == 8 ? OctalBase : base == 10 ? DecimalBase : base == 16 ? HexadecimalBase : 0;
    stream->flags = (stream->flags & ~BaseField) | (field & BaseField & FormatMask);
}

}

extern "C" {

LibcInternalFormatManipulator APS5_VABI _ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(std::int32_t mask) {
    return {&SetFormatFlags_nid_no_patch, static_cast<std::uint32_t>(mask)};
}

LibcInternalFormatManipulator APS5_VABI _ZSt13resetiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(std::int32_t mask) {
    return {&ResetFormatFlags_nid_no_patch, static_cast<std::uint32_t>(mask)};
}

LibcInternalFormatManipulator APS5_VABI _ZSt7setbasei_nid_postfix(std::int32_t base) {
    return {&SetFormatBase_nid_no_patch, static_cast<std::uint32_t>(base)};
}

void APS5_VABI sceLibcInternalIosBaseCompleteDestructor_nid_no_patch(void* self) {
    _ZNSt8ios_baseD2Ev_nid_postfix(static_cast<GuestLocale::IosBase*>(self));
}
APS5_EXPORT("X9D8WWSG3As", sceLibcInternalIosBaseCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalIosBaseDeletingDestructor_nid_no_patch(void* self) {
    _ZNSt8ios_baseD2Ev_nid_postfix(static_cast<GuestLocale::IosBase*>(self));
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("I5jcbATyIWo", sceLibcInternalIosBaseDeletingDestructor_nid_no_patch);

}
