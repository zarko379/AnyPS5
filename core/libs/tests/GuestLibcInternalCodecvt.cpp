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

using AlwaysNoconv = bool (APS5_VABI*)(const GuestLocale::Facet*);
using Convert = int (APS5_VABI*)(const GuestLocale::Facet*, std::uint64_t*, const char*, const char*, const char**, char*, char*, char**);
using Unshift = int (APS5_VABI*)(const GuestLocale::Facet*, std::uint64_t*, char*, char*, char**);
using Length = int (APS5_VABI*)(const GuestLocale::Facet*, std::uint64_t*, const char*, const char*, std::size_t);

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

void CheckBaseMembers() {
    GuestLocale::Facet facet{&g_recordingCodecvt.facet, 1, 0};
    std::uint64_t state[2]{0x1234, 0x5678};
    const char source[] = "abc";
    char destination[4]{'x', 'y', 'z', 'w'};
    const char* next1 = nullptr;
    char* next3 = nullptr;

    Require(Export<AlwaysNoconv>("7tIwDZyyKHo")(&facet), "do_always_noconv is true for codecvt<char, char>");

    for (const char* nid : {"P+q1OLiErP0", "Uc+-Sx0UZ3U"}) {
        next1 = nullptr;
        next3 = nullptr;
        g_call = {};
        Require(Export<Convert>(nid)(&facet, state, source, source + 3, &next1, destination, destination + 4, &next3) == 3, "do_in and do_out return noconv");
        Require(next1 == source && next3 == destination, "do_in and do_out leave both next pointers at the start");
        Require(destination[0] == 'x' && destination[3] == 'w' && state[0] == 0x1234 && state[1] == 0x5678, "do_in and do_out copy nothing and leave the state");
        Require(g_call.slot == 0, "do_in and do_out do not dispatch through the vtable");
        Require(Export<Convert>(nid)(&facet, state, nullptr, nullptr, &next1, nullptr, nullptr, &next3) == 3 && next1 == nullptr && next3 == nullptr, "an empty conversion is noconv");
        Reject([&] { Export<Convert>(nid)(&facet, state, source, source + 3, nullptr, destination, destination + 4, &next3); }, "a null source next pointer throws");
        Reject([&] { Export<Convert>(nid)(&facet, state, source, source + 3, &next1, destination, destination + 4, nullptr); }, "a null destination next pointer throws");
    }

    {
        bool unimplemented = false;
        try { Export<Unshift>("iCWgjeqMHvg")(&facet, state, destination + 1, destination + 4, &next3); } catch (const std::runtime_error&) { unimplemented = true; }
        Require(unimplemented, "codecvt<char, char>::do_unshift throws while its result is unsettled");
    }
    for (const char* nid : {"c6Lyc6xOp4o"}) {
        next3 = nullptr;
        g_call = {};
        Require(Export<Unshift>(nid)(&facet, state, destination + 1, destination + 4, &next3) == 3, "do_unshift returns noconv");
        Require(next3 == destination + 1 && destination[1] == 'y' && state[0] == 0x1234, "do_unshift writes nothing and leaves next at the start");
        Require(g_call.slot == 0, "do_unshift does not dispatch through the vtable");
        Reject([&] { Export<Unshift>(nid)(&facet, state, destination, destination + 4, nullptr); }, "a null next pointer throws");
    }
}

void CheckForwarding() {
    GuestLocale::Facet facet{&g_recordingCodecvt.facet, 1, 0};
    std::uint64_t state[2]{};
    const char source[] = "abcdef";
    char destination[8]{};
    const char* next1 = nullptr;
    char* next3 = nullptr;

    g_call = {};
    Require(Export<Convert>("TNexGlwiVEQ")(&facet, state, source, source + 6, &next1, destination, destination + 8, &next3) == 2, "in returns what do_in returns");
    Require(g_call.slot == 1, "in calls the do_in slot");
    Require(g_call.self == &facet && g_call.state == state && g_call.first1 == source && g_call.last1 == source + 6 && g_call.next1 == &next1, "in passes the facet, state and source range");
    Require(g_call.first3 == destination && g_call.last3 == destination + 8 && g_call.next3 == &next3, "in passes the destination range and next pointer");

    g_call = {};
    Require(Export<Convert>("14xKj+SV72o")(&facet, state, source + 1, source + 5, &next1, destination + 2, destination + 7, &next3) == 1, "out returns what do_out returns");
    Require(g_call.slot == 2, "out calls the do_out slot");
    Require(g_call.self == &facet && g_call.state == state && g_call.first1 == source + 1 && g_call.last1 == source + 5 && g_call.next1 == &next1, "out passes the facet, state and source range");
    Require(g_call.first3 == destination + 2 && g_call.last3 == destination + 7 && g_call.next3 == &next3, "out passes the destination range and next pointer");

    g_call = {};
    Require(Export<Unshift>("N7z+Dnk1cS0")(&facet, state, destination + 3, destination + 5, &next3) == 2, "unshift returns what do_unshift returns");
    Require(g_call.slot == 3 && g_call.self == &facet && g_call.state == state && g_call.first3 == destination + 3 && g_call.last3 == destination + 5 && g_call.next3 == &next3, "unshift calls the do_unshift slot with its arguments");

    g_call = {};
    Require(Export<Length>("ikBt0lqkxPc")(&facet, state, source, source + 6, 0x123456789ull) == 7, "length returns the int do_length returns");
    Require(g_call.slot == 4 && g_call.self == &facet && g_call.state == state && g_call.first1 == source && g_call.last1 == source + 6 && g_call.count == 0x123456789ull, "length calls the do_length slot with its arguments");

    Reject([&] { Export<Convert>("TNexGlwiVEQ")(nullptr, state, source, source, &next1, destination, destination, &next3); }, "in on a null facet throws");
    Reject([&] { Export<Convert>("14xKj+SV72o")(nullptr, state, source, source, &next1, destination, destination, &next3); }, "out on a null facet throws");
    Reject([&] { Export<Unshift>("N7z+Dnk1cS0")(nullptr, state, destination, destination, &next3); }, "unshift on a null facet throws");
    Reject([&] { Export<Length>("ikBt0lqkxPc")(nullptr, state, source, source, 1); }, "length on a null facet throws");
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

    CheckBaseMembers();
    CheckForwarding();

    std::puts("libc internal codecvt<char, char> member checks passed");
    return 0;
}
