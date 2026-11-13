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
    std::fprintf(stderr, "libc internal codecvt: %s\n", message);
    std::exit(1);
}

template<typename TAction>
void Reject(TAction action, const char* message) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, message);
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

using Unshift = int (APS5_VABI*)(const GuestLocale::Facet*, std::uint64_t*, char*, char*, char**);

struct Call {
    int slot;
    GuestLocale::Facet* self;
    std::uint64_t* state;
    const char* first1;
    const char* last1;
    const char** next1;
    char* first3;
    char* last3;
    char** next3;
    std::size_t count;
};

Call g_call{};

void APS5_VABI TestFacetNop(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI TestFacetRelease(GuestLocale::Facet*) { return nullptr; }

bool APS5_VABI RecordAlwaysNoconv(const GuestLocale::Facet*) { return false; }
int APS5_VABI RecordConstant(const GuestLocale::Facet*) { return 0; }

int APS5_VABI RecordIn(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, const char** next1, char* first3, char* last3, char** next3) {
    g_call = Call{1, self, state, first1, last1, next1, first3, last3, next3, 0};
    return 2;
}

int APS5_VABI RecordOut(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, const char** next1, char* first3, char* last3, char** next3) {
    g_call = Call{2, self, state, first1, last1, next1, first3, last3, next3, 0};
    return 1;
}

int APS5_VABI RecordUnshift(GuestLocale::Facet* self, std::uint64_t* state, char* first3, char* last3, char** next3) {
    g_call = Call{3, self, state, nullptr, nullptr, nullptr, first3, last3, next3, 0};
    return 2;
}

std::size_t APS5_VABI RecordLength(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, std::size_t count) {
    g_call = Call{4, self, state, first1, last1, nullptr, nullptr, nullptr, nullptr, count};
    return 0xffffffff00000007ull;
}

const GuestLocale::CodecvtVtable g_recordingCodecvt{{TestFacetNop, TestFacetNop, TestFacetNop, TestFacetRelease}, RecordAlwaysNoconv, RecordConstant, RecordConstant, RecordIn, RecordOut, RecordUnshift, RecordLength};

void CheckChar32Unshift() {
    GuestLocale::Facet facet{&g_recordingCodecvt.facet, 1, 0};
    std::uint64_t state[2]{0x1234, 0x5678};
    char destination[4]{'x', 'y', 'z', 'w'};
    char* next3 = nullptr;

    for (const char* nid : {"c6Lyc6xOp4o"}) {
        next3 = nullptr;
        g_call = {};
        Require(Export<Unshift>(nid)(&facet, state, destination + 1, destination + 4, &next3) == 3, "do_unshift returns noconv");
        Require(next3 == destination + 1 && destination[1] == 'y' && state[0] == 0x1234, "do_unshift writes nothing and leaves next at the start");
        Require(g_call.slot == 0, "do_unshift does not dispatch through the vtable");
        Reject([&] { Export<Unshift>(nid)(&facet, state, destination, destination + 4, nullptr); }, "a null next pointer throws");
    }
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

    CheckChar32Unshift();

    std::puts("libc internal codecvt<char32_t, char>::do_unshift checks passed");
    return 0;
}
