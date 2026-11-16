#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 ECDSA sign check failed at line %u\n", location.line());
    std::abort();
}

constexpr std::uint64_t EcCode(std::uint64_t reason) { return 0x10fff000ULL | reason; }

void ExpectError(std::uint64_t code) {
    Require(ERR_get_error_nid_postfix() == code);
    Require(ERR_get_error_nid_postfix() == 0);
}

template<class TAction> bool Throws(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

constexpr std::array<unsigned char, 32> P256Order{
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84, 0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51};

LibreSsl3EcdsaSig* Signature(const unsigned char* r, int rLength, const unsigned char* s, int sLength) {
    auto* signature = ECDSA_SIG_new_nid_postfix();
    Require(signature != nullptr);
    Bignum* first = BN_bin2bn_nid_postfix(r, rLength, nullptr);
    Bignum* second = BN_bin2bn_nid_postfix(s, sLength, nullptr);
    Require(first != nullptr && second != nullptr);
    Require(ECDSA_SIG_set0_nid_postfix(signature, first, second) == 1);
    return signature;
}

void SignAndVerify() {
    ERR_clear_error_nid_postfix();
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(key != nullptr);
    Require(EC_KEY_generate_key_nid_postfix(key) == 1);
    std::array<unsigned char, 64> digest{};
    for (std::size_t index = 0; index < digest.size(); ++index) digest[index] = static_cast<unsigned char>(index * 7 + 1);
    for (const int length : {32, 20, 64}) {
        LibreSsl3EcdsaSig* signature = ECDSA_do_sign_nid_postfix(digest.data(), length, key);
        Require(signature != nullptr && signature->r != nullptr && signature->s != nullptr);
        Require(ECDSA_do_verify_nid_postfix(digest.data(), length, signature, key) == 1);
        digest[0] ^= 1;
        Require(ECDSA_do_verify_nid_postfix(digest.data(), length, signature, key) == 0);
        digest[0] ^= 1;
        Require(ERR_get_error_nid_postfix() == 0);
        ECDSA_SIG_free_nid_postfix(signature);
    }

    const unsigned char zero[]{0};
    const unsigned char one[]{1};
    LibreSsl3EcdsaSig* bad = Signature(zero, 1, one, 1);
    Require(ECDSA_do_verify_nid_postfix(digest.data(), 32, bad, key) == 0);
    ExpectError(EcCode(166));
    ECDSA_SIG_free_nid_postfix(bad);
    bad = Signature(one, 1, P256Order.data(), static_cast<int>(P256Order.size()));
    Require(ECDSA_do_verify_nid_postfix(digest.data(), 32, bad, key) == 0);
    ExpectError(EcCode(166));
    Require(ECDSA_do_verify_nid_postfix(nullptr, 32, bad, key) == 0);
    ExpectError(EcCode(166));
    ECDSA_SIG_free_nid_postfix(bad);
    bad = Signature(one, 1, one, 1);
    Require(Throws([&] { ECDSA_do_verify_nid_postfix(nullptr, 32, bad, key); }));
    Require(ECDSA_do_verify_nid_postfix(nullptr, 0, bad, key) == 0);
    Require(ERR_get_error_nid_postfix() == 0);
    ECDSA_SIG_free_nid_postfix(bad);

    Require(ECDSA_do_verify_nid_postfix(digest.data(), 32, nullptr, key) == -1);
    ExpectError(EcCode(124));
    Require(Throws([&] { ECDSA_do_sign_nid_postfix(nullptr, 32, key); }));
    Require(Throws([&] { ECDSA_do_sign_nid_postfix(digest.data(), -1, key); }));
    Require(Throws([&] { ECDSA_do_sign_nid_postfix(digest.data(), 32, nullptr); }));
    Require(Throws([&] { ECDSA_do_verify_nid_postfix(digest.data(), 32, nullptr, nullptr); }));
    Require(ERR_get_error_nid_postfix() == 0);
    EC_KEY_free_nid_postfix(key);
}

void IncompleteKeys() {
    ERR_clear_error_nid_postfix();
    const unsigned char digest[32]{};
    const unsigned char one[]{1};
    EcKey* publicOnly = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(publicOnly != nullptr);
    Require(ECDSA_do_sign_nid_postfix(digest, 32, publicOnly) == nullptr);
    ExpectError(EcCode(67));
    LibreSsl3EcdsaSig* signature = Signature(one, 1, one, 1);
    Require(ECDSA_do_verify_nid_postfix(digest, 32, signature, publicOnly) == -1);
    ExpectError(EcCode(124));
    EcKey* groupless = EC_KEY_new_nid_postfix();
    Require(groupless != nullptr);
    Require(ECDSA_do_sign_nid_postfix(digest, 32, groupless) == nullptr);
    Require(ERR_get_error_nid_postfix() == 0);
    Require(ECDSA_do_verify_nid_postfix(digest, 32, signature, groupless) == -1);
    ExpectError(EcCode(124));
    ECDSA_SIG_free_nid_postfix(signature);
    EC_KEY_free_nid_postfix(groupless);
    EC_KEY_free_nid_postfix(publicOnly);
}

}

int main() {
    void* api[10]{};
    ApplicationHeapRegister_nid_no_patch(api);
    SignAndVerify();
    IncompleteKeys();
    std::puts("LibreSSL3 ECDSA sign tests passed");
    return 0;
}
