#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value, const char* what, int row) {
    if (value) return;
    std::fprintf(stderr, "failed: %s (row %d)\n", what, row);
    std::abort();
}

struct SinRow {
    std::uint64_t x, y;
    int iy;
    std::uint64_t result;
};

struct CosRow {
    std::uint64_t x, y, result;
};

struct FloatRow {
    std::uint64_t x;
    std::uint32_t sine, cosine;
};

const SinRow SinRows[] = {
    {0x3fe0000000000000ull, 0x0000000000000000ull, 0, 0x3fdeaee8744b05f0ull},
    {0x3fe0000000000000ull, 0x3c670ef54646d497ull, 1, 0x3fdeaee8744b05f0ull},
    {0xbfe6666666666666ull, 0xbc770ef54646d497ull, 1, 0xbfe49d6e694619b8ull},
    {0x3fe921fb54442d15ull, 0x3c81a3c350a443e9ull, 1, 0x3fe6a09e667f3bcaull},
    {0x3fd0000000000000ull, 0x0000000000000000ull, 0, 0x3fcfaaeed4f31577ull},
    {0x3ee4f8b588e368f1ull, 0x0000000000000000ull, 0, 0x3ee4f8b588e1e8a2ull},
    {0x0000000000000000ull, 0x0000000000000000ull, 0, 0x0000000000000000ull},
    {0xbfd3333333333333ull, 0x0000000000000000ull, 0, 0xbfd2e9cd95baba33ull},
    {0x3fe0000000000000ull, 0x3e112e0be826d695ull, 1, 0x3fdeaee8753b8a97ull},
    {0x3fe0000000000000ull, 0x3e112e0be826d695ull, 0, 0x3fdeaee8744b05f0ull},
    {0x3fe0000000000000ull, 0x3e112e0be826d695ull, 7, 0x3fdeaee8753b8a97ull},
    {0xbfe3333333333333ull, 0xbdf49da7e361ce4cull, 1, 0xbfe2118d17c710b4ull},
};

const CosRow CosRows[] = {
    {0x3fe0000000000000ull, 0x0000000000000000ull, 0x3fec1528065b7d50ull},
    {0x3fe0000000000000ull, 0x3c670ef54646d497ull, 0x3fec1528065b7d50ull},
    {0xbfe6666666666666ull, 0xbc770ef54646d497ull, 0x3fe87996529f9d93ull},
    {0x3fe921fb54442d15ull, 0x3c81a3c350a443e9ull, 0x3fe6a09e667f3bcfull},
    {0x0000000000000000ull, 0x0000000000000000ull, 0x3ff0000000000000ull},
    {0x3e112e0be826d695ull, 0x0000000000000000ull, 0x3ff0000000000000ull},
    {0x3fd0000000000000ull, 0xbc32725dd1d243acull, 0x3fef01549f7deea1ull},
    {0x3fe0000000000000ull, 0x3e112e0be826d695ull, 0x3fec15280616c520ull},
    {0xbfe3333333333333ull, 0xbdf49da7e361ce4cull, 0x3fea69263c2f9de5ull},
};

const FloatRow FloatRows[] = {
    {0x3fe0000000000000ull, 0x3ef57744u, 0x3f60a940u},
    {0xbfe921fb54442d15ull, 0xbf3504f3u, 0x3f3504f3u},
    {0x3fb999999999999aull, 0x3dcc7576u, 0x3f7eb898u},
    {0x0000000000000000ull, 0x00000000u, 0x3f800000u},
    {0x8000000000000000ull, 0x00000000u, 0x3f800000u},
    {0x3fe921fb323ae5afull, 0x3f3504f2u, 0x3f3504f4u},
};

double D(std::uint64_t bits) {
    return std::bit_cast<double>(bits);
}

std::uint64_t B(double value) {
    return std::bit_cast<std::uint64_t>(value);
}

std::uint32_t B(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

}

int main() {
    int row = 0;
    for (const SinRow& entry : SinRows) Require(B(__kernel_sin_nid_postfix(D(entry.x), D(entry.y), entry.iy)) == entry.result, "__kernel_sin", row++);
    row = 0;
    for (const CosRow& entry : CosRows) Require(B(__kernel_cos_nid_postfix(D(entry.x), D(entry.y))) == entry.result, "__kernel_cos", row++);
    row = 0;
    for (const FloatRow& entry : FloatRows) {
        Require(B(__kernel_sindf_nid_postfix(D(entry.x))) == entry.sine, "__kernel_sindf", row);
        Require(B(__kernel_cosdf_nid_postfix(D(entry.x))) == entry.cosine, "__kernel_cosdf", row);
        ++row;
    }
    std::puts("trig kernels ok");
    return 0;
}
