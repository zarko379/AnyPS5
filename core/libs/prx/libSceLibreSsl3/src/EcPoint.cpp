#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int EcLibrary = 16;
constexpr int AnyFunction = 0xfff;
constexpr int PassedNullParameter = 67;
constexpr int PointAtInfinity = 106;
constexpr int PointIsNotOnCurve = 107;

struct Mpi {
    Mpi() { mbedtls_mpi_init(&value); }
    ~Mpi() { mbedtls_mpi_free(&value); }
    Mpi(const Mpi&) = delete;
    Mpi& operator=(const Mpi&) = delete;
    mbedtls_mpi value;
};

void EcError(int reason) {
    LibreSslPutError_nid_no_patch(EcLibrary, AnyFunction, reason);
}

void Check(int result, const char* function) {
    if (result != 0) throw std::runtime_error(std::string(function) + ": mbedTLS failure " + std::to_string(result));
}

void RequireSameCurve(const EcGroup& group, const EcPoint& point, const char* what) {
    if (group.curve.nid != point.curve.nid) NotImplemented_nid_no_patch(what);
}

void ReadBignum(const Bignum* number, mbedtls_mpi& out, const char* function) {
    const int bits = BN_num_bits_nid_postfix(number);
    std::vector<unsigned char> bytes((static_cast<std::size_t>(bits) + 7) / 8);
    BN_bn2bin_nid_postfix(number, bytes.data());
    Check(mbedtls_mpi_read_binary(&out, bytes.data(), bytes.size()), function);
    if (number->neg && mbedtls_mpi_cmp_int(&out, 0) != 0) {
        Mpi zero;
        Check(mbedtls_mpi_sub_mpi(&out, &zero.value, &out), function);
    }
}

bool SameParameters(const mbedtls_ecp_group& first, const mbedtls_ecp_group& second) {
    return mbedtls_mpi_cmp_mpi(&first.P, &second.P) == 0 && mbedtls_mpi_cmp_mpi(&first.A, &second.A) == 0 &&
        mbedtls_mpi_cmp_mpi(&first.B, &second.B) == 0 && mbedtls_ecp_point_cmp(&first.G, &second.G) == 0 &&
        mbedtls_mpi_cmp_mpi(&first.N, &second.N) == 0 && first.MBEDTLS_PRIVATE(h) == second.MBEDTLS_PRIVATE(h);
}

} // namespace

extern "C" {

int APS5_VABI EC_GROUP_cmp_nid_postfix(const EcGroup* first, const EcGroup* second, BignumContext*) {
    if (first == nullptr || second == nullptr) APS5_INVALID_ARG_EX;
    if (first->curve.nid != 0 && second->curve.nid != 0 && first->curve.nid != second->curve.nid) return 1;
    return SameParameters(first->group, second->group) ? 0 : 1;
}

EcPoint* APS5_VABI EC_POINT_dup_nid_postfix(const EcPoint* source, const EcGroup* group) {
    if (source == nullptr) return nullptr;
    if (group == nullptr) {
        EcError(PassedNullParameter);
        return nullptr;
    }
    RequireSameCurve(*group, *source, "EC_POINT_dup: a point of another curve");
    std::unique_ptr<EcPoint, void (APS5_VABI*)(EcPoint*)> copy(EC_POINT_new_nid_postfix(group), &EC_POINT_free_nid_postfix);
    if (!copy) return nullptr;
    Check(mbedtls_ecp_copy(&copy->point, &source->point), __func__);
    return copy.release();
}

int APS5_VABI EC_POINT_get_affine_coordinates_GFp_nid_postfix(const EcGroup* group, const EcPoint* point, Bignum* x, Bignum* y, BignumContext*) {
    if (group == nullptr || point == nullptr) APS5_INVALID_ARG_EX;
    RequireSameCurve(*group, *point, "EC_POINT_get_affine_coordinates_GFp: a point of another curve");
    const mbedtls_ecp_point& value = point->point;
    if (mbedtls_ecp_is_zero(const_cast<mbedtls_ecp_point*>(&value)) == 1) {
        EcError(PointAtInfinity);
        return 0;
    }
    const std::size_t fieldSize = mbedtls_mpi_size(&group->group.P);
    std::vector<unsigned char> encoded(1 + 2 * fieldSize);
    std::size_t written = 0;
    Check(mbedtls_ecp_point_write_binary(&group->group, &value, MBEDTLS_ECP_PF_UNCOMPRESSED, &written, encoded.data(), encoded.size()), __func__);
    if (written != encoded.size()) throw std::runtime_error(std::string(__func__) + ": unexpected point encoding");
    if (x != nullptr && !BN_bin2bn_nid_postfix(encoded.data() + 1, static_cast<int>(fieldSize), x)) return 0;
    if (y != nullptr && !BN_bin2bn_nid_postfix(encoded.data() + 1 + fieldSize, static_cast<int>(fieldSize), y)) return 0;
    return 1;
}

int APS5_VABI EC_POINT_set_affine_coordinates_GFp_nid_postfix(const EcGroup* group, EcPoint* point, const Bignum* x, const Bignum* y, BignumContext*) {
    if (group == nullptr || point == nullptr) APS5_INVALID_ARG_EX;
    RequireSameCurve(*group, *point, "EC_POINT_set_affine_coordinates_GFp: a point of another curve");
    if (x == nullptr || y == nullptr) {
        EcError(PassedNullParameter);
        return 0;
    }
    const mbedtls_mpi& prime = group->group.P;
    Mpi affineX;
    Mpi affineY;
    ReadBignum(x, affineX.value, __func__);
    ReadBignum(y, affineY.value, __func__);
    Check(mbedtls_mpi_mod_mpi(&affineX.value, &affineX.value, &prime), __func__);
    Check(mbedtls_mpi_mod_mpi(&affineY.value, &affineY.value, &prime), __func__);
    const std::size_t fieldSize = mbedtls_mpi_size(&prime);
    std::vector<unsigned char> encoded(1 + 2 * fieldSize);
    encoded[0] = 0x04;
    Check(mbedtls_mpi_write_binary(&affineX.value, encoded.data() + 1, fieldSize), __func__);
    Check(mbedtls_mpi_write_binary(&affineY.value, encoded.data() + 1 + fieldSize, fieldSize), __func__);
    Check(mbedtls_ecp_point_read_binary(&group->group, &point->point, encoded.data(), encoded.size()), __func__);
    if (mbedtls_ecp_check_pubkey(&group->group, &point->point) != 0) {
        EcError(PointIsNotOnCurve);
        return 0;
    }
    return 1;
}

}
