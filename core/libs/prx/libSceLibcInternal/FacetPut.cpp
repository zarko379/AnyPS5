#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

const GuestLocale::NumPutVtable& NumPut(const GuestLocale::Facet* self) {
    return *reinterpret_cast<const GuestLocale::NumPutVtable*>(self->vtable);
}

struct WideNumPutVtable {
    GuestLocale::FacetVtable facet;
    GuestLocale::OutputIterator (APS5_VABI *putBool)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, bool value);
    GuestLocale::OutputIterator (APS5_VABI *putLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::int64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putUnsignedLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::uint64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putLongLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::int64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putUnsignedLongLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::uint64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putDouble)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, double value);
    GuestLocale::OutputIterator (APS5_VABI *putLongDouble)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, long double value);
    GuestLocale::OutputIterator (APS5_VABI *putPointer)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const void* value);
};

static_assert(offsetof(WideNumPutVtable, putBool) == 0x20);
static_assert(offsetof(WideNumPutVtable, putLong) == 0x28);
static_assert(offsetof(WideNumPutVtable, putUnsignedLong) == 0x30);
static_assert(offsetof(WideNumPutVtable, putLongLong) == 0x38);
static_assert(offsetof(WideNumPutVtable, putUnsignedLongLong) == 0x40);
static_assert(offsetof(WideNumPutVtable, putDouble) == 0x48);
static_assert(offsetof(WideNumPutVtable, putLongDouble) == 0x50);
static_assert(offsetof(WideNumPutVtable, putPointer) == 0x58);

const GuestLocale::NumPutVtable& BaseNumPut() {
    static const GuestLocale::NumPutVtable* const base = [] {
        GuestLocale::Facet* facet = nullptr;
        _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&facet, nullptr);
        if (facet == nullptr || facet->vtable == nullptr) throw std::runtime_error("num_put<char>::do_put: libc did not create its num_put<char> facet");
        return reinterpret_cast<const GuestLocale::NumPutVtable*>(facet->vtable);
    }();
    return *base;
}

const WideNumPutVtable& WideNumPut(const GuestLocale::Facet* self) {
    return *reinterpret_cast<const WideNumPutVtable*>(self->vtable);
}

struct WideTimePutVtable {
    GuestLocale::FacetVtable facet;
    GuestLocale::OutputIterator (APS5_VABI *put)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const GuestTm* time, char format, char modifier);
};

static_assert(offsetof(WideTimePutVtable, put) == 0x20);

}

extern "C" {

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const void* value) {
    return NumPut(self).putPointer(self, out, stream, fill, value);
}
APS5_EXPORT("8NVUcufbklM", sceLibcInternalNumPutPointer_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, bool value) {
    return NumPut(self).putBool(self, out, stream, fill, value);
}
APS5_EXPORT("nftirmo6hBg", sceLibcInternalNumPutBool_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, double value) {
    return NumPut(self).putDouble(self, out, stream, fill, value);
}
APS5_EXPORT("w9NzCYAjEpQ", sceLibcInternalNumPutDouble_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, long double value) {
    return NumPut(self).putLongDouble(self, out, stream, fill, value);
}
APS5_EXPORT("VPcTGA-LwSo", sceLibcInternalNumPutLongDouble_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::int64_t value) {
    return NumPut(self).putLong(self, out, stream, fill, value);
}
APS5_EXPORT("ffnhh0HcxJ4", sceLibcInternalNumPutLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::uint64_t value) {
    return NumPut(self).putUnsignedLong(self, out, stream, fill, value);
}
APS5_EXPORT("uODuM76vS4U", sceLibcInternalNumPutUnsignedLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::int64_t value) {
    return NumPut(self).putLongLong(self, out, stream, fill, value);
}
APS5_EXPORT("NJtKruu9qOs", sceLibcInternalNumPutLongLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::uint64_t value) {
    return NumPut(self).putUnsignedLongLong(self, out, stream, fill, value);
}
APS5_EXPORT("dep6W2Ix35s", sceLibcInternalNumPutUnsignedLongLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const void* value) {
    return BaseNumPut().putPointer(self, out, stream, fill, value);
}
APS5_EXPORT("0xgFRKf0Lc4", sceLibcInternalNumPutDoPointer_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, bool value) {
    return BaseNumPut().putBool(self, out, stream, fill, value);
}
APS5_EXPORT("ke36E2bqNmI", sceLibcInternalNumPutDoBool_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, double value) {
    return BaseNumPut().putDouble(self, out, stream, fill, value);
}
APS5_EXPORT("F+cp2B3cWNU", sceLibcInternalNumPutDoDouble_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, long double value) {
    return BaseNumPut().putLongDouble(self, out, stream, fill, value);
}
APS5_EXPORT("rLiFc4+HyHw", sceLibcInternalNumPutDoLongDouble_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::int64_t value) {
    return BaseNumPut().putLong(self, out, stream, fill, value);
}
APS5_EXPORT("I3+xmBWGPGo", sceLibcInternalNumPutDoLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::uint64_t value) {
    return BaseNumPut().putUnsignedLong(self, out, stream, fill, value);
}
APS5_EXPORT("nlAk46weq1w", sceLibcInternalNumPutDoUnsignedLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::int64_t value) {
    return BaseNumPut().putLongLong(self, out, stream, fill, value);
}
APS5_EXPORT("H2KGT3vA7yQ", sceLibcInternalNumPutDoLongLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::uint64_t value) {
    return BaseNumPut().putUnsignedLongLong(self, out, stream, fill, value);
}
APS5_EXPORT("Vbeoft607aI", sceLibcInternalNumPutDoUnsignedLongLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const void* value) {
    return WideNumPut(self).putPointer(self, out, stream, fill, value);
}
APS5_EXPORT("xfOSCbCiY44", sceLibcInternalWideNumPutPointer_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, bool value) {
    return WideNumPut(self).putBool(self, out, stream, fill, value);
}
APS5_EXPORT("mY9FWooxqJY", sceLibcInternalWideNumPutBool_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, double value) {
    return WideNumPut(self).putDouble(self, out, stream, fill, value);
}
APS5_EXPORT("V7aIsVIsIIA", sceLibcInternalWideNumPutDouble_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, long double value) {
    return WideNumPut(self).putLongDouble(self, out, stream, fill, value);
}
APS5_EXPORT("vCIFGeI6adI", sceLibcInternalWideNumPutLongDouble_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::int64_t value) {
    return WideNumPut(self).putLong(self, out, stream, fill, value);
}
APS5_EXPORT("USLhWp7sZoU", sceLibcInternalWideNumPutLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::uint64_t value) {
    return WideNumPut(self).putUnsignedLong(self, out, stream, fill, value);
}
APS5_EXPORT("qtpzdwMMCPc", sceLibcInternalWideNumPutUnsignedLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::int64_t value) {
    return WideNumPut(self).putLongLong(self, out, stream, fill, value);
}
APS5_EXPORT("ryykbHJ04Cw", sceLibcInternalWideNumPutLongLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::uint64_t value) {
    return WideNumPut(self).putUnsignedLongLong(self, out, stream, fill, value);
}
APS5_EXPORT("lmb3oBpMNPU", sceLibcInternalWideNumPutUnsignedLongLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalTimePutPattern_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const GuestTm* time, const char* first, const char* last) {
    return LibcTimePutPattern_nid_no_patch(self, out, stream, fill, time, first, last);
}
APS5_EXPORT("j9LU8GsuEGw", sceLibcInternalTimePutPattern_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalTimePut_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const GuestTm* time, char format, char modifier) {
    return reinterpret_cast<const GuestLocale::TimePutVtable*>(self->vtable)->put(self, out, stream, fill, time, format, modifier);
}
APS5_EXPORT("qkuA-unH7PU", sceLibcInternalTimePut_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI _ZNKSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewPK2tmcc_nid_postfix(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const GuestTm* time, char format, char modifier) {
    return reinterpret_cast<const WideTimePutVtable*>(self->vtable)->put(self, out, stream, fill, time, format, modifier);
}

}
