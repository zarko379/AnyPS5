#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal num_get: %s\n", message);
    std::exit(1);
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

#ifdef _WIN32
HMODULE g_library = nullptr;
#else
void* g_library = nullptr;
#endif

template<typename TFunction>
TFunction Export(const char* nid) {
    const std::string name = std::string(nid) + "_nid_no_patch_cut";
#ifdef _WIN32
    auto* address = reinterpret_cast<void*>(GetProcAddress(g_library, name.c_str()));
#else
    void* address = dlsym(g_library, name.c_str());
#endif
    Require(address != nullptr, "missing export");
    return reinterpret_cast<TFunction>(address);
}

GuestLocale::InputIterator Iterator(std::uint64_t low, std::uint64_t high) {
    return GuestLocale::InputIterator{{low, high}};
}

bool Same(const GuestLocale::InputIterator& left, const GuestLocale::InputIterator& right) {
    return left.words[0] == right.words[0] && left.words[1] == right.words[1];
}

constexpr std::uint64_t kReturnedLow = 0x0123456789abcdefull;
constexpr std::uint64_t kReturnedHigh = 0x0000000000000a01ull;

struct Call {
    int slot;
    int calls;
    const GuestLocale::Facet* self;
    GuestLocale::InputIterator first;
    GuestLocale::InputIterator last;
    GuestLocale::IosBase* stream;
    std::int32_t* state;
    const void* value;
};

Call g_call{};

void APS5_VABI TestFacetNop(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI TestFacetRelease(GuestLocale::Facet*) { return nullptr; }

template<int TSlot, typename TValue>
GuestLocale::InputIterator APS5_VABI Record(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, TValue* value) {
    g_call = Call{TSlot, g_call.calls + 1, self, first, last, stream, state, value};
    *state |= 2;
    return Iterator(kReturnedLow + TSlot, kReturnedHigh);
}

struct NumGetVtable {
    GuestLocale::FacetVtable facet;
    decltype(&Record<0, bool>) getBool;
    decltype(&Record<1, std::uint16_t>) getUnsignedShort;
    decltype(&Record<2, std::uint32_t>) getUnsignedInt;
    decltype(&Record<3, std::int64_t>) getLong;
    decltype(&Record<4, std::uint64_t>) getUnsignedLong;
    decltype(&Record<5, std::int64_t>) getLongLong;
    decltype(&Record<6, std::uint64_t>) getUnsignedLongLong;
    decltype(&Record<7, float>) getFloat;
    decltype(&Record<8, double>) getDouble;
    decltype(&Record<9, long double>) getLongDouble;
    decltype(&Record<10, void*>) getPointer;
};

static_assert(offsetof(NumGetVtable, getBool) == 0x20);
static_assert(offsetof(NumGetVtable, getLong) == 0x38);
static_assert(offsetof(NumGetVtable, getPointer) == 0x70);

const NumGetVtable g_recordingNumGet{{TestFacetNop, TestFacetNop, TestFacetNop, TestFacetRelease},
    Record<0, bool>, Record<1, std::uint16_t>, Record<2, std::uint32_t>, Record<3, std::int64_t>, Record<4, std::uint64_t>,
    Record<5, std::int64_t>, Record<6, std::uint64_t>, Record<7, float>, Record<8, double>, Record<9, long double>, Record<10, void*>};

template<typename TValue>
using NumGetExport = GuestLocale::InputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::InputIterator, GuestLocale::InputIterator, GuestLocale::IosBase*, std::int32_t*, TValue*);

template<typename TValue>
void CheckGet(const char* nid, int slot) {
    const auto get = Export<NumGetExport<TValue>>(nid);
    GuestLocale::Facet facet{&g_recordingNumGet.facet, 1, 0};
    GuestLocale::IosBase stream{};
    std::int32_t state = 4;
    TValue value{};
    const GuestLocale::InputIterator first = Iterator(0x1111222233334444ull + slot, 0x0000000000004101ull);
    const GuestLocale::InputIterator last = Iterator(0x5555666677778888ull, 0x0000000000000001ull + slot);
    const int calls = g_call.calls;
    const GuestLocale::InputIterator result = get(&facet, first, last, &stream, &state, &value);
    Require(g_call.calls == calls + 1, "get calls do_get exactly once");
    Require(g_call.slot == slot, "get calls the do_get slot of its value type");
    Require(g_call.self == &facet && g_call.stream == &stream && g_call.state == &state && g_call.value == &value, "get passes the facet, stream, state and value");
    Require(Same(g_call.first, first) && Same(g_call.last, last), "get passes both iterators unchanged");
    Require(Same(result, Iterator(kReturnedLow + slot, kReturnedHigh)), "get returns the iterator do_get returns");
    Require(state == 6, "get leaves the state to do_get");

    bool rejected = false;
    try { get(nullptr, first, last, &stream, &state, &value); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected && g_call.calls == calls + 1, "get on a null facet throws without calling do_get");
}

void CheckNumGet(const std::array<const char*, 11>& nids) {
    CheckGet<bool>(nids[0], 0);
    CheckGet<std::uint16_t>(nids[1], 1);
    CheckGet<std::uint32_t>(nids[2], 2);
    CheckGet<std::int64_t>(nids[3], 3);
    CheckGet<std::uint64_t>(nids[4], 4);
    CheckGet<std::int64_t>(nids[5], 5);
    CheckGet<std::uint64_t>(nids[6], 6);
    CheckGet<float>(nids[7], 7);
    CheckGet<double>(nids[8], 8);
    CheckGet<long double>(nids[9], 9);
    CheckGet<void*>(nids[10], 10);
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    Require(argc == 2, "expected the libSceLibcInternal path");
#ifdef _WIN32
    g_library = LoadLibraryA(argv[1]);
#else
    g_library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
#endif
    Require(g_library != nullptr, "load libSceLibcInternal");
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckNumGet({"OWO5cpNw3NA", "gozsp4urvq8", "N1VqUWz2OEI", "I2UzwkwwEPs", "2bfL3yIBi5k", "4hiQK82QuLc", "eZfFLyWCkvg", "6RGkooTERsE", "mAwXCpkWaYc", "wUCRGap1j0U", "my9ujasm6-0"});
    CheckNumGet({"vW-nnV62ea4", "bZ+lKHGvOr8", "Y3hBU5FYmhM", "-m2YPwVCwJQ", "94ZLp2+AOq0", "cG5hQhjFGog", "banNSumaAZ0", "2mb8FYgER+E", "+hjXHfvy1Mg", "xLZr4GJRMLo", "zomvAQ5RFdA"});

    std::puts("libc internal num_get checks passed");
    return 0;
}
