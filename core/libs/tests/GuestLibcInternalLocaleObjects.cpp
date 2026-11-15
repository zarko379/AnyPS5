#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal locale objects: %s\n", message);
    std::abort();
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
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

void CheckLocinfo() {
    alignas(16) std::array<std::byte, 80> bytes;
    bytes.fill(std::byte{0x5a});
    void* storage = bytes.data() + 8;
    sceLibcInternalLocinfoBaseConstructor_nid_no_patch(storage, "C");
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        Require(bytes[index] == (index >= 8 && index < 72 ? std::byte{} : std::byte{0x5a}), "the C2 constructor clears only the 64-byte object");
    }
    sceLibcInternalLocinfoBaseDestructor_nid_no_patch(storage);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        Require(bytes[index] == (index >= 8 && index < 72 ? std::byte{} : std::byte{0x5a}), "the D2 destructor leaves the storage untouched");
    }

    Require(Throws([&] { sceLibcInternalLocinfoBaseConstructor_nid_no_patch(storage, "unknown-locale"); }), "an unsupported locale name is rejected");
    Require(Throws([] { sceLibcInternalLocinfoBaseConstructor_nid_no_patch(nullptr, "C"); }), "a null object is rejected");
    Require(Throws([] { sceLibcInternalLocinfoBaseDestructor_nid_no_patch(nullptr); }), "destroying a null object is rejected");
}

void CheckLocimpCopy() {
    std::array<GuestLocale::Facet, 2> facets{};
    std::array<GuestLocale::Facet*, 3> slots{&facets[0], nullptr, &facets[1]};
    GuestLocale::Implementation source{};
    source.base.references = 7;
    source.facets = slots.data();
    source.facetCount = slots.size();
    source.category = 0x3f;
    source.transparent = true;
    source.name = "C";

    GuestLocale::Implementation expected{};
    GuestLocale::Implementation actual{};
    _ZNSt6locale7_LocimpC1ERKS0__nid_postfix(&expected, &source);
    sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(&actual, &source);

    Require(actual.base.vtable == expected.base.vtable && actual.base.vtable != nullptr, "the copy gets the same vtable as the C1 copy");
    Require(actual.base.references == 1, "the copy starts with one reference");
    Require(actual.facetCount == source.facetCount && actual.category == source.category, "the copy keeps the facet count and categories");
    Require(actual.transparent == source.transparent && actual.name == source.name, "the copy keeps the name and transparency");
    Require(actual.facets != nullptr && actual.facets != source.facets, "the copy owns a new facet vector");
    Require(std::memcmp(actual.facets, slots.data(), sizeof(slots)) == 0, "the copy shares the source facets");
    Require(source.base.references == 7 && source.facets == slots.data(), "the source is unchanged");

    GuestLocale::Implementation empty{};
    empty.name = "C";
    GuestLocale::Implementation emptyCopy{};
    sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(&emptyCopy, &empty);
    Require(emptyCopy.facetCount == 0 && emptyCopy.facets == nullptr, "a locale without facets copies no vector");

    Require(Throws([&] { sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(nullptr, &source); }), "a null object is rejected");
    Require(Throws([&] { sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(&emptyCopy, nullptr); }), "a null source is rejected");
}

template <typename Construct>
void CheckLocinfoForm(Construct construct, const char* form) {
    alignas(16) std::array<std::byte, 80> bytes;
    bytes.fill(std::byte{0x5a});
    construct(bytes.data() + 8);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (bytes[index] != (index >= 8 && index < 72 ? std::byte{} : std::byte{0x5a})) {
            std::fprintf(stderr, "libc internal locale objects: %s does not build the C object in place\n", form);
            std::abort();
        }
    }
    sceLibcInternalLocinfoBaseDestructor_nid_no_patch(bytes.data() + 8);
}

GuestLocale::String InlineString(const char* text) {
    GuestLocale::String string{};
    string.size = std::strlen(text);
    string.capacity = GuestLocale::StringInlineCapacity;
    std::memcpy(string.buffer, text, string.size + 1);
    return string;
}

GuestLocale::String HeapString(char* text, std::size_t capacity) {
    GuestLocale::String string{};
    string.size = std::strlen(text);
    string.capacity = capacity;
    string.pointer = text;
    return string;
}

void CheckLocinfoVariants() {
    for (const int category : {0, 1, 0x3f, 0x7fffffff, -1}) {
        CheckLocinfoForm([&](void* storage) { _ZNSt8_LocinfoC1EiPKc_nid_postfix(static_cast<GuestLocale::LocinfoStorage*>(storage), category, "C"); }, "the category C1 constructor");
        CheckLocinfoForm([&](void* storage) { sceLibcInternalLocinfoCategoryBaseConstructor_nid_no_patch(storage, category, "C"); }, "the category C2 constructor");
    }
    alignas(16) std::array<std::byte, 64> storage{};
    Require(Throws([&] { _ZNSt8_LocinfoC1EiPKc_nid_postfix(reinterpret_cast<GuestLocale::LocinfoStorage*>(storage.data()), 0x3f, "POSIX"); }), "the category C1 constructor rejects an unsupported name");
    Require(Throws([&] { _ZNSt8_LocinfoC1EiPKc_nid_postfix(reinterpret_cast<GuestLocale::LocinfoStorage*>(storage.data()), 0, nullptr); }), "the category C1 constructor rejects a null name");
    Require(Throws([] { _ZNSt8_LocinfoC1EiPKc_nid_postfix(nullptr, 0x3f, "C"); }), "the category C1 constructor rejects a null object");
    Require(Throws([&] { sceLibcInternalLocinfoCategoryBaseConstructor_nid_no_patch(storage.data(), 1, ""); }), "the category C2 constructor rejects an unsupported name");
    Require(Throws([] { sceLibcInternalLocinfoCategoryBaseConstructor_nid_no_patch(nullptr, 1, "C"); }), "the category C2 constructor rejects a null object");

    const GuestLocale::String inlineName = InlineString("C");
    char heapText[32] = "C";
    const GuestLocale::String heapName = HeapString(heapText, GuestLocale::StringInlineCapacity + 1);
    CheckLocinfoForm([&](void* object) { sceLibcInternalLocinfoStringConstructor_nid_no_patch(object, &inlineName); }, "the string C1 constructor with an inline name");
    CheckLocinfoForm([&](void* object) { sceLibcInternalLocinfoStringConstructor_nid_no_patch(object, &heapName); }, "the string C1 constructor with an allocated name");
    CheckLocinfoForm([&](void* object) { sceLibcInternalLocinfoStringBaseConstructor_nid_no_patch(object, &inlineName); }, "the string C2 constructor with an inline name");
    CheckLocinfoForm([&](void* object) { sceLibcInternalLocinfoStringBaseConstructor_nid_no_patch(object, &heapName); }, "the string C2 constructor with an allocated name");

    GuestLocale::String embedded = InlineString("C");
    embedded.size = 3;
    embedded.buffer[2] = 'x';
    embedded.buffer[3] = 0;
    CheckLocinfoForm([&](void* object) { sceLibcInternalLocinfoStringConstructor_nid_no_patch(object, &embedded); }, "the string constructor reading the name up to its first null");

    GuestLocale::String inlineOther = InlineString("POSIX");
    char heapOther[32] = "C.UTF-8";
    const GuestLocale::String heapOtherName = HeapString(heapOther, 16);
    GuestLocale::String inlineDisguised = InlineString("xx");
    inlineDisguised.pointer = heapText;
    Require(Throws([&] { sceLibcInternalLocinfoStringConstructor_nid_no_patch(storage.data(), &inlineOther); }), "the string C1 constructor rejects an unsupported inline name");
    Require(Throws([&] { sceLibcInternalLocinfoStringConstructor_nid_no_patch(storage.data(), &heapOtherName); }), "the string C1 constructor reads an allocated name past the inline capacity");
    Require(Throws([&] { sceLibcInternalLocinfoStringBaseConstructor_nid_no_patch(storage.data(), &inlineDisguised); }), "the string C2 constructor reads an inline name from the buffer");
    Require(Throws([&] { sceLibcInternalLocinfoStringConstructor_nid_no_patch(storage.data(), nullptr); }), "the string C1 constructor rejects a null string");
    Require(Throws([&] { sceLibcInternalLocinfoStringBaseConstructor_nid_no_patch(storage.data(), nullptr); }), "the string C2 constructor rejects a null string");
    Require(Throws([&] { sceLibcInternalLocinfoStringConstructor_nid_no_patch(nullptr, &inlineName); }), "the string C1 constructor rejects a null object");
}

void CheckFacetReferences() {
    GuestLocale::Facet facet{};
    facet.references = 1;
    facet.reserved = 0xa5a5a5a5u;
    _ZNSt6locale5facet7_IncrefEv_nid_postfix(&facet);
    Require(facet.references == 2 && facet.reserved == 0xa5a5a5a5u, "_Incref adds one reference and touches nothing else");
    Require(_ZNSt6locale5facet7_DecrefEv_nid_postfix(&facet) == nullptr && facet.references == 1, "_Decref above one returns null");
    Require(_ZNSt6locale5facet7_DecrefEv_nid_postfix(&facet) == &facet && facet.references == 0, "_Decref to zero returns the facet");
    Require(_ZNSt6locale5facet7_DecrefEv_nid_postfix(&facet) == nullptr && facet.references == 0xffffffffu, "_Decref from zero wraps and returns null");
    _ZNSt6locale5facet7_IncrefEv_nid_postfix(&facet);
    Require(facet.references == 0, "_Incref from the maximum count wraps to zero");
    Require(facet.vtable == nullptr && facet.reserved == 0xa5a5a5a5u, "the vtable and padding are untouched");

    GuestLocale::Facet shared{};
    shared.references = 1;
    constexpr int Threads = 4;
    constexpr int Rounds = 20000;
    std::vector<std::thread> workers;
    for (int index = 0; index < Threads; ++index) workers.emplace_back([&] { for (int round = 0; round < Rounds; ++round) _ZNSt6locale5facet7_IncrefEv_nid_postfix(&shared); });
    for (std::thread& worker : workers) worker.join();
    Require(shared.references == 1u + Threads * Rounds, "concurrent _Incref calls are not lost");
    workers.clear();
    std::array<int, Threads> lastReleases{};
    for (int index = 0; index < Threads; ++index) workers.emplace_back([&, index] { for (int round = 0; round < Rounds; ++round) lastReleases[index] += _ZNSt6locale5facet7_DecrefEv_nid_postfix(&shared) == &shared; });
    for (std::thread& worker : workers) worker.join();
    Require(shared.references == 1 && lastReleases == std::array<int, Threads>{}, "concurrent _Decref calls are not lost");
    Require(_ZNSt6locale5facet7_DecrefEv_nid_postfix(&shared) == &shared, "the last reference is released once");

    Require(Throws([] { _ZNSt6locale5facet7_IncrefEv_nid_postfix(nullptr); }), "_Incref rejects a null facet");
    Require(Throws([] { _ZNSt6locale5facet7_DecrefEv_nid_postfix(nullptr); }), "_Decref rejects a null facet");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckLocinfo();
    CheckLocimpCopy();
    CheckFacetReferences();
    CheckLocinfoVariants();

    std::puts("libc internal _Locinfo and _Locimp base-object checks passed");
    return 0;
}
