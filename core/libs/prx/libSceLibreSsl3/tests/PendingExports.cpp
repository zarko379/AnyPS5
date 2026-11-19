#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

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

}

int main() {
    void* api[10]{};
    ApplicationHeapRegister_nid_no_patch(api);
    CheckBioVfree();
    CheckCipherFlags();
    std::puts("LibreSSL3 pending exports passed");
    return 0;
}
