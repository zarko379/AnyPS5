#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 pending export check failed at line %u\n", location.line());
    std::abort();
}

void CheckBioVfree() {
    BIO_vfree_nid_postfix(nullptr);
    auto* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(bio != nullptr);
    Require(BIO_up_ref_nid_postfix(bio) == 1);
    Require(bio->references == 2);
    BIO_vfree_nid_postfix(bio);
    Require(bio->references == 1);
    Require(BIO_write_nid_postfix(bio, "abc", 3) == 3);
    BIO_vfree_nid_postfix(bio);
}

void CheckCipherFlags() {
    auto* context = EVP_CIPHER_CTX_new_nid_postfix();
    Require(context != nullptr && context->flags == 0);
    EVP_CIPHER_CTX_set_flags_nid_postfix(context, 0x1);
    Require(context->flags == 0x1);
    EVP_CIPHER_CTX_set_flags_nid_postfix(context, 0x100);
    Require(context->flags == 0x101);
    Require(EVP_CIPHER_CTX_set_padding_nid_postfix(context, 1) == 1);
    Require(context->flags == 0x1);
    EVP_CIPHER_CTX_set_flags_nid_postfix(context, 0);
    Require(context->flags == 0x1);
    EVP_CIPHER_CTX_set_flags_nid_postfix(context, -2);
    Require(context->flags == 0xffffffffffffffffULL);
    context->flags = 0;
    EVP_CIPHER_CTX_set_flags_nid_postfix(context, 0x7fffffff);
    Require(context->flags == 0x7fffffffULL);
    EVP_CIPHER_CTX_free_nid_postfix(context);
    bool threw = false;
    try {
        EVP_CIPHER_CTX_set_flags_nid_postfix(nullptr, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void CheckLoaders() {
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(11, 0xfff, 101);
    const std::uint64_t code = 0x0bfff065;
    const auto* sha256 = EVP_get_digestbyname_nid_postfix("SHA256");
    ERR_load_X509_strings_nid_postfix();
    ERR_load_PEM_strings_nid_postfix();
    OpenSSL_add_all_digests_nid_postfix();
    ERR_load_X509_strings_nid_postfix();
    ERR_load_PEM_strings_nid_postfix();
    OpenSSL_add_all_digests_nid_postfix();
    Require(ERR_reason_error_string_nid_postfix(code) == nullptr);
    Require(ERR_lib_error_string_nid_postfix(code) == nullptr);
    Require(ERR_reason_error_string_nid_postfix(0x09fff064) == nullptr);
    Require(EVP_get_digestbyname_nid_postfix("SHA256") == sha256 && sha256 != nullptr);
    Require(ERR_get_error_nid_postfix() == code && ERR_get_error_nid_postfix() == 0);
}

}

int main() {
    void* api[10]{};
    ApplicationHeapRegister_nid_no_patch(api);
    CheckBioVfree();
    CheckCipherFlags();
    CheckLoaders();
    std::puts("LibreSSL3 pending exports passed");
    return 0;
}
