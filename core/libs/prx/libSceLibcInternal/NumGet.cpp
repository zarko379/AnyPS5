#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

struct NumGetVtable {
    GuestLocale::FacetVtable facet;
    GuestLocale::InputIterator (APS5_VABI *getBool)(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, bool* value);
    GuestLocale::InputIterator (APS5_VABI *getUnsignedShort)(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint16_t* value);
    GuestLocale::InputIterator (APS5_VABI *getUnsignedInt)(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint32_t* value);
    GuestLocale::InputIterator (APS5_VABI *getLong)(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value);
    GuestLocale::InputIterator (APS5_VABI *getUnsignedLong)(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value);
    GuestLocale::InputIterator (APS5_VABI *getLongLong)(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value);
    GuestLocale::InputIterator (APS5_VABI *getUnsignedLongLong)(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value);
    GuestLocale::InputIterator (APS5_VABI *getFloat)(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, float* value);
    GuestLocale::InputIterator (APS5_VABI *getDouble)(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, double* value);
    GuestLocale::InputIterator (APS5_VABI *getLongDouble)(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, long double* value);
    GuestLocale::InputIterator (APS5_VABI *getPointer)(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, void** value);
};

static_assert(offsetof(NumGetVtable, getBool) == 0x20);
static_assert(offsetof(NumGetVtable, getUnsignedShort) == 0x28);
static_assert(offsetof(NumGetVtable, getUnsignedInt) == 0x30);
static_assert(offsetof(NumGetVtable, getLong) == 0x38);
static_assert(offsetof(NumGetVtable, getUnsignedLong) == 0x40);
static_assert(offsetof(NumGetVtable, getLongLong) == 0x48);
static_assert(offsetof(NumGetVtable, getUnsignedLongLong) == 0x50);
static_assert(offsetof(NumGetVtable, getFloat) == 0x58);
static_assert(offsetof(NumGetVtable, getDouble) == 0x60);
static_assert(offsetof(NumGetVtable, getLongDouble) == 0x68);
static_assert(offsetof(NumGetVtable, getPointer) == 0x70);
static_assert(sizeof(NumGetVtable) == 0x78);

const NumGetVtable& NumGet(const GuestLocale::Facet* self, const char* function) {
    if (self == nullptr || self->vtable == nullptr) throw std::invalid_argument(std::string(function) + ": null facet");
    return *reinterpret_cast<const NumGetVtable*>(self->vtable);
}

}

extern "C" {

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, bool* value) {
    return NumGet(self, "num_get<char>::get").getBool(self, first, last, stream, state, value);
}
APS5_EXPORT("OWO5cpNw3NA", sceLibcInternalNumGetBool_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetUnsignedShort_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint16_t* value) {
    return NumGet(self, "num_get<char>::get").getUnsignedShort(self, first, last, stream, state, value);
}
APS5_EXPORT("gozsp4urvq8", sceLibcInternalNumGetUnsignedShort_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetUnsignedInt_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint32_t* value) {
    return NumGet(self, "num_get<char>::get").getUnsignedInt(self, first, last, stream, state, value);
}
APS5_EXPORT("N1VqUWz2OEI", sceLibcInternalNumGetUnsignedInt_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return NumGet(self, "num_get<char>::get").getLong(self, first, last, stream, state, value);
}
APS5_EXPORT("I2UzwkwwEPs", sceLibcInternalNumGetLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return NumGet(self, "num_get<char>::get").getUnsignedLong(self, first, last, stream, state, value);
}
APS5_EXPORT("2bfL3yIBi5k", sceLibcInternalNumGetUnsignedLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return NumGet(self, "num_get<char>::get").getLongLong(self, first, last, stream, state, value);
}
APS5_EXPORT("4hiQK82QuLc", sceLibcInternalNumGetLongLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return NumGet(self, "num_get<char>::get").getUnsignedLongLong(self, first, last, stream, state, value);
}
APS5_EXPORT("eZfFLyWCkvg", sceLibcInternalNumGetUnsignedLongLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetFloat_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, float* value) {
    return NumGet(self, "num_get<char>::get").getFloat(self, first, last, stream, state, value);
}
APS5_EXPORT("6RGkooTERsE", sceLibcInternalNumGetFloat_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, double* value) {
    return NumGet(self, "num_get<char>::get").getDouble(self, first, last, stream, state, value);
}
APS5_EXPORT("mAwXCpkWaYc", sceLibcInternalNumGetDouble_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, long double* value) {
    return NumGet(self, "num_get<char>::get").getLongDouble(self, first, last, stream, state, value);
}
APS5_EXPORT("wUCRGap1j0U", sceLibcInternalNumGetLongDouble_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, void** value) {
    return NumGet(self, "num_get<char>::get").getPointer(self, first, last, stream, state, value);
}
APS5_EXPORT("my9ujasm6-0", sceLibcInternalNumGetPointer_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, bool* value) {
    return NumGet(self, "num_get<wchar_t>::get").getBool(self, first, last, stream, state, value);
}
APS5_EXPORT("vW-nnV62ea4", sceLibcInternalWideNumGetBool_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetUnsignedShort_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint16_t* value) {
    return NumGet(self, "num_get<wchar_t>::get").getUnsignedShort(self, first, last, stream, state, value);
}
APS5_EXPORT("bZ+lKHGvOr8", sceLibcInternalWideNumGetUnsignedShort_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetUnsignedInt_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint32_t* value) {
    return NumGet(self, "num_get<wchar_t>::get").getUnsignedInt(self, first, last, stream, state, value);
}
APS5_EXPORT("Y3hBU5FYmhM", sceLibcInternalWideNumGetUnsignedInt_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return NumGet(self, "num_get<wchar_t>::get").getLong(self, first, last, stream, state, value);
}
APS5_EXPORT("-m2YPwVCwJQ", sceLibcInternalWideNumGetLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return NumGet(self, "num_get<wchar_t>::get").getUnsignedLong(self, first, last, stream, state, value);
}
APS5_EXPORT("94ZLp2+AOq0", sceLibcInternalWideNumGetUnsignedLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return NumGet(self, "num_get<wchar_t>::get").getLongLong(self, first, last, stream, state, value);
}
APS5_EXPORT("cG5hQhjFGog", sceLibcInternalWideNumGetLongLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return NumGet(self, "num_get<wchar_t>::get").getUnsignedLongLong(self, first, last, stream, state, value);
}
APS5_EXPORT("banNSumaAZ0", sceLibcInternalWideNumGetUnsignedLongLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetFloat_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, float* value) {
    return NumGet(self, "num_get<wchar_t>::get").getFloat(self, first, last, stream, state, value);
}
APS5_EXPORT("2mb8FYgER+E", sceLibcInternalWideNumGetFloat_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, double* value) {
    return NumGet(self, "num_get<wchar_t>::get").getDouble(self, first, last, stream, state, value);
}
APS5_EXPORT("+hjXHfvy1Mg", sceLibcInternalWideNumGetDouble_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, long double* value) {
    return NumGet(self, "num_get<wchar_t>::get").getLongDouble(self, first, last, stream, state, value);
}
APS5_EXPORT("xLZr4GJRMLo", sceLibcInternalWideNumGetLongDouble_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, void** value) {
    return NumGet(self, "num_get<wchar_t>::get").getPointer(self, first, last, stream, state, value);
}
APS5_EXPORT("zomvAQ5RFdA", sceLibcInternalWideNumGetPointer_nid_no_patch);

}
