#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

extern "C" {
void APS5_VABI sceLibcInternalUnknown6_112_6yplvTHbxpE_nid_no_patch(void* self);
}

namespace {

unsigned releases = 0;
void* lastReleased = nullptr;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal deleting destructors: %s\n", message);
    std::abort();
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) {
    ++releases;
    lastReleased = pointer;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    using Destructor = void (APS5_VABI*)(void*);
    const Destructor destructors[] = {
        sceLibcInternalUnknown6_112_6yplvTHbxpE_nid_no_patch,
        _ZNSt7codecvtIwc9_MbstatetED0Ev_nid_postfix,
        _ZNSt8messagesIwED0Ev_nid_postfix,
        _ZNSt24_Iostream_error_categoryD0Ev_nid_postfix,
        _ZNSt8messagesIcED0Ev_nid_postfix,
        _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED0Ev_nid_postfix,
        libSceLibcInternalInventory_020_nid_no_patch,
        libSceLibcInternalInventory_096_nid_no_patch,
        libSceLibcInternalInventory_101_nid_no_patch,
        libSceLibcInternalInventory_110_nid_no_patch,
        libSceLibcInternalInventory_116_nid_no_patch,
        libSceLibcInternalInventory_124_nid_no_patch,
        libSceLibcInternalInventory_131_nid_no_patch,
        libSceLibcInternalInventory_136_nid_no_patch,
        libSceLibcInternalInventory_212_nid_no_patch,
        libSceLibcInternalInventory_223_nid_no_patch,
        libSceLibcInternalInventory_232_nid_no_patch,
    };

    for (Destructor destructor : destructors) {
        void* object = _Znwm_nid_postfix(0x40);
        Require(object != nullptr, "the guest heap allocates the object");
        releases = 0;
        lastReleased = nullptr;
        destructor(object);
        Require(releases == 1 && lastReleased == object, "the deleting destructor releases the object");
    }

    std::array<unsigned char, 16> category{};
    category.fill(0xa5);
    releases = 0;
    libSceLibcInternalInventory_058_nid_no_patch(category.data());
    Require(releases == 0, "the _Iostream_error_category complete-object destructor releases nothing");
    for (unsigned char byte : category) Require(byte == 0xa5, "the _Iostream_error_category complete-object destructor leaves the object untouched");
    std::puts("libc internal deleting destructor checks passed");
    return 0;
}
