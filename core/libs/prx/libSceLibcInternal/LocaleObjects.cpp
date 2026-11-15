#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstdint>
#include <stdexcept>

extern "C" {

void APS5_VABI sceLibcInternalLocinfoBaseConstructor_nid_no_patch(void* self, const char* localeName) {
    _ZNSt8_LocinfoC1EPKc_nid_postfix(static_cast<GuestLocale::LocinfoStorage*>(self), localeName);
}
APS5_EXPORT("pN02FS5SPgg", sceLibcInternalLocinfoBaseConstructor_nid_no_patch);

void APS5_VABI sceLibcInternalLocinfoBaseDestructor_nid_no_patch(void* self) {
    _ZNSt8_LocinfoD1Ev_nid_postfix(static_cast<GuestLocale::LocinfoStorage*>(self));
}
APS5_EXPORT("YXVCU6PdgZk", sceLibcInternalLocinfoBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(void* self, const void* source) {
    _ZNSt6locale7_LocimpC1ERKS0__nid_postfix(static_cast<GuestLocale::Implementation*>(self), static_cast<const GuestLocale::Implementation*>(source));
}
APS5_EXPORT("dsJKehuajH4", sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch);

void APS5_VABI _ZNSt8_LocinfoC1EiPKc_nid_postfix(GuestLocale::LocinfoStorage* self, int, const char* localeName) {
    _ZNSt8_LocinfoC1EPKc_nid_postfix(self, localeName);
}

void APS5_VABI sceLibcInternalLocinfoCategoryBaseConstructor_nid_no_patch(void* self, int category, const char* localeName) {
    _ZNSt8_LocinfoC1EiPKc_nid_postfix(static_cast<GuestLocale::LocinfoStorage*>(self), category, localeName);
}
APS5_EXPORT("i180MNC9p4c", sceLibcInternalLocinfoCategoryBaseConstructor_nid_no_patch);

void APS5_VABI sceLibcInternalLocinfoStringConstructor_nid_no_patch(void* self, const GuestLocale::String* localeName) {
    if (localeName == nullptr) throw std::invalid_argument("_Locinfo: null locale name string");
    const char* name = localeName->capacity > GuestLocale::StringInlineCapacity ? localeName->pointer : localeName->buffer;
    _ZNSt8_LocinfoC1EPKc_nid_postfix(static_cast<GuestLocale::LocinfoStorage*>(self), name);
}
APS5_EXPORT("2aSk2ruCP0E", sceLibcInternalLocinfoStringConstructor_nid_no_patch);

void APS5_VABI sceLibcInternalLocinfoStringBaseConstructor_nid_no_patch(void* self, const GuestLocale::String* localeName) {
    sceLibcInternalLocinfoStringConstructor_nid_no_patch(self, localeName);
}
APS5_EXPORT("ReK9U6EUWuU", sceLibcInternalLocinfoStringBaseConstructor_nid_no_patch);

void APS5_VABI _ZNSt6locale5facet7_IncrefEv_nid_postfix(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("locale::facet::_Incref: null facet");
    std::atomic_ref<std::uint32_t>(self->references).fetch_add(1);
}

GuestLocale::Facet* APS5_VABI _ZNSt6locale5facet7_DecrefEv_nid_postfix(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("locale::facet::_Decref: null facet");
    return std::atomic_ref<std::uint32_t>(self->references).fetch_sub(1) == 1 ? self : nullptr;
}

}
