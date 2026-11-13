#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int Secp256r1 = 415;
constexpr int Secp384r1 = 715;
constexpr std::uint64_t PassedNullParameter = 0x10fff043;
constexpr std::uint64_t PointAtInfinity = 0x10fff06a;
constexpr std::uint64_t PointIsNotOnCurve = 0x10fff06b;
constexpr const char* GeneratorX = "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296";
constexpr const char* GeneratorY = "4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5";
constexpr const char* GeneratorXPlusPrime = "16b17d1f1e12c4248f8bce6e563a440f277037d822deb33a0f4a13945d898c295";
constexpr const char* PrimeMinusGeneratorY = "b01cbd1c01e58065711814b583f061e9d431cca994cea1313449bf97c840ae0a";
constexpr const char* GeneratorYPlusOne = "4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f6";

void* APS5_VABI Allocate(std::size_t bytes) { return std::malloc(bytes ? bytes : 1); }
void APS5_VABI Release(void* block) { std::free(block); }
void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) { return std::calloc(count ? count : 1, bytes ? bytes : 1); }
void* APS5_VABI Reallocate(void* block, std::size_t bytes) { return std::realloc(block, bytes ? bytes : 1); }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EC point check failed at line %u\n", location.line());
    std::abort();
}

void RequireErrors(std::initializer_list<std::uint64_t> expected, std::source_location location = std::source_location::current()) {
    for (const std::uint64_t code : expected) Require(ERR_get_error_nid_postfix() == code, location);
    Require(ERR_get_error_nid_postfix() == 0, location);
}

template <typename TAction>
void RequireInvalid(TAction action, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, location);
}

std::vector<unsigned char> Bytes(const char* hex) {
    std::vector<unsigned char> bytes;
    std::string text(hex);
    if (text.size() % 2) text.insert(text.begin(), '0');
    for (std::size_t index = 0; index < text.size(); index += 2) bytes.push_back(static_cast<unsigned char>(std::stoul(text.substr(index, 2), nullptr, 16)));
    return bytes;
}

Bignum* Number(const char* hex) {
    const std::vector<unsigned char> bytes = Bytes(hex);
    Bignum* number = BN_bin2bn_nid_postfix(bytes.data(), static_cast<int>(bytes.size()), nullptr);
    Require(number != nullptr);
    return number;
}

std::string Hex(const Bignum* number) {
    std::vector<unsigned char> bytes((static_cast<std::size_t>(BN_num_bits_nid_postfix(number)) + 7) / 8);
    BN_bn2bin_nid_postfix(number, bytes.data());
    std::string text;
    char digits[3];
    for (const unsigned char byte : bytes) {
        std::snprintf(digits, sizeof(digits), "%02x", byte);
        text += digits;
    }
    if (number->neg) text.insert(text.begin(), '-');
    return text;
}

EcPoint* Generator(const EcGroup* group) {
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    Bignum* one = Number("01");
    Require(point && EC_POINT_mul_nid_postfix(group, point, one, nullptr, nullptr, nullptr) == 1);
    BN_free_nid_postfix(one);
    return point;
}

void RequireAffine(const EcGroup* group, const EcPoint* point, const char* x, const char* y, std::source_location location = std::source_location::current()) {
    Bignum* affineX = BN_new_nid_postfix();
    Bignum* affineY = BN_new_nid_postfix();
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, point, affineX, affineY, nullptr) == 1, location);
    Require(Hex(affineX) == x && Hex(affineY) == y, location);
    BN_free_nid_postfix(affineX);
    BN_free_nid_postfix(affineY);
    RequireErrors({}, location);
}

void TestGetAffine() {
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(Secp256r1);
    EcPoint* generator = Generator(group);
    RequireAffine(group, generator, GeneratorX, GeneratorY);
    Bignum* only = Number("05");
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, generator, only, nullptr, nullptr) == 1 && Hex(only) == GeneratorX);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, generator, nullptr, only, nullptr) == 1 && Hex(only) == GeneratorY);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, generator, nullptr, nullptr, nullptr) == 1);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, generator, only, only, nullptr) == 1 && Hex(only) == GeneratorY);
    RequireErrors({});
    EcPoint* infinity = EC_POINT_new_nid_postfix(group);
    BN_set_word_nid_postfix(only, 5);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, infinity, only, only, nullptr) == 0 && Hex(only) == "05");
    RequireErrors({PointAtInfinity});
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, infinity, nullptr, nullptr, nullptr) == 0);
    RequireErrors({PointAtInfinity});
    RequireInvalid([&] { EC_POINT_get_affine_coordinates_GFp_nid_postfix(nullptr, generator, only, only, nullptr); });
    RequireInvalid([&] { EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, nullptr, only, only, nullptr); });
    BN_free_nid_postfix(only);
    EC_POINT_free_nid_postfix(infinity);
    EC_POINT_free_nid_postfix(generator);
    EC_GROUP_free_nid_postfix(group);
}

void TestSetAffine() {
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(Secp256r1);
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    Bignum* x = Number(GeneratorX);
    Bignum* y = Number(GeneratorY);
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, x, y, nullptr) == 1);
    RequireAffine(group, point, GeneratorX, GeneratorY);
    unsigned char encoded[65] {};
    Require(EC_POINT_point2oct_nid_postfix(group, point, 4, encoded, sizeof(encoded), nullptr) == 65 && encoded[0] == 4);
    const std::vector<unsigned char> expectedX = Bytes(GeneratorX);
    Require(std::equal(expectedX.begin(), expectedX.end(), encoded + 1));
    Bignum* unreducedX = Number(GeneratorXPlusPrime);
    Bignum* negativeY = Number(PrimeMinusGeneratorY);
    negativeY->neg = 1;
    EcPoint* reduced = EC_POINT_new_nid_postfix(group);
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, reduced, unreducedX, negativeY, nullptr) == 1);
    RequireAffine(group, reduced, GeneratorX, GeneratorY);
    Require(Hex(unreducedX) == std::string("0") + GeneratorXPlusPrime &&Hex(negativeY) == std::string("-") + PrimeMinusGeneratorY);
    Bignum* offY = Number(GeneratorYPlusOne);
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, reduced, x, offY, nullptr) == 0);
    RequireErrors({PointIsNotOnCurve});
    RequireAffine(group, reduced, GeneratorX, GeneratorYPlusOne);
    Bignum* zero = BN_new_nid_postfix();
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, reduced, zero, zero, nullptr) == 0);
    RequireErrors({PointIsNotOnCurve});
    RequireAffine(group, reduced, "", "");
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, nullptr, y, nullptr) == 0);
    RequireErrors({PassedNullParameter});
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, x, nullptr, nullptr) == 0);
    RequireErrors({PassedNullParameter});
    RequireAffine(group, point, GeneratorX, GeneratorY);
    RequireInvalid([&] { EC_POINT_set_affine_coordinates_GFp_nid_postfix(nullptr, point, x, y, nullptr); });
    RequireInvalid([&] { EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, nullptr, x, y, nullptr); });
    EcGroup* larger = EC_GROUP_new_by_curve_name_nid_postfix(Secp384r1);
    EcPoint* largerGenerator = Generator(larger);
    Bignum* largerX = BN_new_nid_postfix();
    Bignum* largerY = BN_new_nid_postfix();
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(larger, largerGenerator, largerX, largerY, nullptr) == 1);
    EcPoint* largerCopy = EC_POINT_new_nid_postfix(larger);
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(larger, largerCopy, largerX, largerY, nullptr) == 1);
    Require(Hex(largerX).rfind("aa87ca22be8b0537", 0) == 0);
    RequireErrors({});
    for (Bignum* number : {x, y, unreducedX, negativeY, offY, zero, largerX, largerY}) BN_free_nid_postfix(number);
    for (EcPoint* each : {point, reduced, largerGenerator, largerCopy}) EC_POINT_free_nid_postfix(each);
    EC_GROUP_free_nid_postfix(larger);
    EC_GROUP_free_nid_postfix(group);
}

void TestDup() {
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(Secp256r1);
    EcPoint* generator = Generator(group);
    Require(EC_POINT_dup_nid_postfix(nullptr, group) == nullptr);
    Require(EC_POINT_dup_nid_postfix(nullptr, nullptr) == nullptr);
    RequireErrors({});
    Require(EC_POINT_dup_nid_postfix(generator, nullptr) == nullptr);
    RequireErrors({PassedNullParameter});
    EcPoint* copy = EC_POINT_dup_nid_postfix(generator, group);
    Require(copy != nullptr && copy != generator && copy->curve.nid == Secp256r1);
    RequireAffine(group, copy, GeneratorX, GeneratorY);
    Bignum* two = Number("02");
    Require(EC_POINT_mul_nid_postfix(group, generator, two, nullptr, nullptr, nullptr) == 1);
    RequireAffine(group, copy, GeneratorX, GeneratorY);
    EcPoint* infinity = EC_POINT_new_nid_postfix(group);
    EcPoint* infinityCopy = EC_POINT_dup_nid_postfix(infinity, group);
    Require(infinityCopy != nullptr && EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, infinityCopy, nullptr, nullptr, nullptr) == 0);
    RequireErrors({PointAtInfinity});
    BN_free_nid_postfix(two);
    for (EcPoint* each : {generator, copy, infinity, infinityCopy}) EC_POINT_free_nid_postfix(each);
    EC_GROUP_free_nid_postfix(group);
}

void TestGroupCompare() {
    EcGroup* first = EC_GROUP_new_by_curve_name_nid_postfix(Secp256r1);
    EcGroup* second = EC_GROUP_new_by_curve_name_nid_postfix(Secp256r1);
    EcGroup* other = EC_GROUP_new_by_curve_name_nid_postfix(Secp384r1);
    Require(EC_GROUP_cmp_nid_postfix(first, second, nullptr) == 0);
    Require(EC_GROUP_cmp_nid_postfix(first, first, nullptr) == 0);
    Require(EC_GROUP_cmp_nid_postfix(first, other, nullptr) == 1);
    Require(EC_GROUP_cmp_nid_postfix(other, second, nullptr) == 1);
    RequireErrors({});
    RequireInvalid([&] { EC_GROUP_cmp_nid_postfix(nullptr, first, nullptr); });
    RequireInvalid([&] { EC_GROUP_cmp_nid_postfix(first, nullptr, nullptr); });
    for (EcGroup* each : {first, second, other}) EC_GROUP_free_nid_postfix(each);
}

} // namespace

int main() {
    InitializeHeap();
    TestGetAffine();
    TestSetAffine();
    TestDup();
    TestGroupCompare();
    std::puts("LibreSSL3 EC point tests passed");
    return 0;
}
