#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <system_error>

namespace {

alignas(64) std::array<std::byte, 64> storage{};
unsigned releases = 0;
void* lastReleased = nullptr;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal C++ runtime forwards: %s\n", message);
    std::abort();
}

void* APS5_VABI allocate(std::size_t) { return storage.data(); }

void APS5_VABI release(void* pointer) {
    ++releases;
    lastReleased = pointer;
}

void* APS5_VABI allocateZeroed(std::size_t, std::size_t) { return storage.data(); }
void* APS5_VABI reallocate(void*, std::size_t) { return storage.data(); }
void* APS5_VABI align(std::size_t, std::size_t) { return storage.data(); }
void* APS5_VABI realign(void*, std::size_t, std::size_t) { return storage.data(); }
int APS5_VABI posixAlign(void** pointer, std::size_t, std::size_t) {
    *pointer = storage.data();
    return 0;
}

void Reset() {
    releases = 0;
    lastReleased = nullptr;
}

template<typename TFloat>
bool Same(TFloat left, TFloat right) {
    return std::isnan(left) ? std::isnan(right) : left == right && std::signbit(left) == std::signbit(right);
}

void CheckLogGamma() {
    const double inputs[] = {1e-6, 0.001, 0.25, 0.5, 1.0, 1.5, 2.0, 3.5, 7.0, 10.0, 33.25, 100.0, 1e5, 1e30, -0.5, -2.5, 0.0, -0.0,
        INFINITY, -INFINITY, NAN};
    for (const double input : inputs) {
        const auto single = static_cast<float>(input);
        const auto extended = static_cast<long double>(input);
        Require(Same(_ZSt8_XLgammaf_nid_postfix(single), _ZNSt8__sce_v28_XLgammaEf_nid_postfix(single)), "_XLgamma(float) matches the __sce_v2 overload");
        Require(Same(_ZSt8_XLgammad_nid_postfix(input), _ZNSt8__sce_v28_XLgammaEd_nid_postfix(input)), "_XLgamma(double) matches the __sce_v2 overload");
        Require(Same(_ZSt8_XLgammae_nid_postfix(extended), _ZNSt8__sce_v28_XLgammaEe_nid_postfix(extended)), "_XLgamma(long double) matches the __sce_v2 overload");
    }
    Require(std::fabs(_ZSt8_XLgammaf_nid_postfix(1.0f)) < 1e-5f, "_XLgamma(float) of one is zero");
    Require(std::fabs(_ZSt8_XLgammaf_nid_postfix(4.0f) - std::log(6.0f)) < 1e-5f, "_XLgamma(float) of four is log 6");
    Require(std::fabs(_ZSt8_XLgammad_nid_postfix(5.0) - std::log(24.0)) < 1e-7, "_XLgamma(double) of five is log 24");
    Require(std::fabs(_ZSt8_XLgammad_nid_postfix(0.5) - 0.5 * std::log(std::acos(-1.0))) < 1e-7, "_XLgamma(double) of one half is log sqrt pi");
    Require(std::fabs(_ZSt8_XLgammae_nid_postfix(10.0L) - std::log(362880.0L)) < 1e-7L, "_XLgamma(long double) of ten is log 9!");
    Require(std::isnan(_ZSt8_XLgammaf_nid_postfix(NAN)) && std::isnan(_ZSt8_XLgammad_nid_postfix(NAN)) && std::isnan(_ZSt8_XLgammae_nid_postfix(NAN)), "_XLgamma propagates NaN");
}

}

int main() {
    std::array<void*, 10> api{};
    api[0] = reinterpret_cast<void*>(&allocate);
    api[1] = reinterpret_cast<void*>(&release);
    api[2] = reinterpret_cast<void*>(&allocateZeroed);
    api[3] = reinterpret_cast<void*>(&reallocate);
    api[4] = reinterpret_cast<void*>(&align);
    api[5] = reinterpret_cast<void*>(&realign);
    api[6] = reinterpret_cast<void*>(&posixAlign);
    ApplicationHeapRegister_nid_no_patch(api.data());

    const int nothrowTag = 0;
    Reset();
    sceLibcInternalSizedNothrowDelete_nid_no_patch(storage.data(), 24, &nothrowTag);
    Require(releases == 1 && lastReleased == storage.data(), "the sized nothrow delete releases the block");

    Reset();
    sceLibcInternalSizedNothrowArrayDelete_nid_no_patch(storage.data(), 64, nullptr);
    Require(releases == 1 && lastReleased == storage.data(), "the sized nothrow array delete releases the block");

    Reset();
    sceLibcInternalSizedNothrowDelete_nid_no_patch(nullptr, 24, &nothrowTag);
    sceLibcInternalSizedNothrowArrayDelete_nid_no_patch(nullptr, 64, &nothrowTag);
    Require(releases == 0, "deleting null releases nothing");

    const std::error_category* const category = sceLibcInternalIostreamCategory_nid_no_patch();
    Require(category != nullptr, "the iostream category exists");
    Require(category == _ZSt17iostream_categoryv_nid_postfix(), "the iostream category is the libc object");
    Require(category == IostreamCategory_nid_no_patch(), "the forward and the libc export share one body");
    Require(category == sceLibcInternalIostreamCategory_nid_no_patch(), "the iostream category is a singleton");
    Require(std::strcmp(category->name(), "iostream") == 0, "the category is named iostream");

    CheckLogGamma();

    std::puts("libc internal C++ runtime forward checks passed");
    return 0;
}
