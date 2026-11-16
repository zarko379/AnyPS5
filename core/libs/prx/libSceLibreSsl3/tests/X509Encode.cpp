#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 i2d_X509 check failed at line %u\n", location.line());
    std::abort();
}

template<class TAction> bool Throws(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct Encoding {
    unsigned char* enc;
    std::int64_t length;
    std::int32_t modified;
};

std::vector<unsigned char> Encode(const LibreSsl3EncodingX509& certificate) {
    const int length = i2d_X509_nid_postfix(&certificate, nullptr);
    Require(length > 0);
    std::vector<unsigned char> buffer(static_cast<std::size_t>(length) + 1, 0xee);
    unsigned char* cursor = buffer.data();
    Require(i2d_X509_nid_postfix(&certificate, &cursor) == length);
    Require(cursor == buffer.data() + length && buffer.back() == 0xee);
    unsigned char* allocated = nullptr;
    Require(i2d_X509_nid_postfix(&certificate, &allocated) == length && allocated != nullptr);
    Require(std::memcmp(allocated, buffer.data(), static_cast<std::size_t>(length)) == 0);
    free_nid_postfix(allocated);
    buffer.pop_back();
    return buffer;
}

}

int main() {
    void* api[10]{};
    ApplicationHeapRegister_nid_no_patch(api);

    unsigned char tbs[]{0x30, 0x03, 0x02, 0x01, 0x05};
    LibreSsl3CertificateInfo info{};
    Encoding encoding{tbs, sizeof(tbs), 0};
    std::memcpy(info.encoding, &encoding, sizeof(encoding));

    const unsigned char oid[]{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
    Asn1Object_nid_no_patch object{"RSA-SHA256", "sha256WithRSAEncryption", 668, sizeof(oid), oid, 0};
    LibreSsl3Asn1Type nullParameter{5, nullptr};
    LibreSsl3X509Algor algorithm{&object, &nullParameter};

    unsigned char signatureBytes[]{0xab, 0xcd};
    Asn1String signature{};
    signature.length = 2;
    signature.type = 3;
    signature.data = signatureBytes;
    signature.flags = 0x08;

    LibreSsl3EncodingX509 certificate{};
    certificate.certInfo = &info;
    certificate.sigAlg = &algorithm;
    certificate.signature = &signature;

    Require(Encode(certificate) == std::vector<unsigned char>{
        0x30, 0x19, 0x30, 0x03, 0x02, 0x01, 0x05,
        0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00,
        0x03, 0x03, 0x00, 0xab, 0xcd});

    algorithm.parameter = nullptr;
    signatureBytes[1] = 0xcf;
    signature.flags = 0x08 | 4;
    Require(Encode(certificate) == std::vector<unsigned char>{
        0x30, 0x17, 0x30, 0x03, 0x02, 0x01, 0x05,
        0x30, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
        0x03, 0x03, 0x04, 0xab, 0xc0});
    Require(signatureBytes[1] == 0xcf);

    signatureBytes[1] = 0xcc;
    signature.flags = 0;
    Require(Encode(certificate) == std::vector<unsigned char>{
        0x30, 0x17, 0x30, 0x03, 0x02, 0x01, 0x05,
        0x30, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
        0x03, 0x03, 0x02, 0xab, 0xcc});

    signature.length = 0;
    Require(Encode(certificate) == std::vector<unsigned char>{
        0x30, 0x13, 0x30, 0x03, 0x02, 0x01, 0x05,
        0x30, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
        0x03, 0x01, 0x00});
    signature.length = 2;

    std::vector<unsigned char> longTbs(300, 0x5a);
    Encoding longEncoding{longTbs.data(), static_cast<std::int64_t>(longTbs.size()), 0};
    std::memcpy(info.encoding, &longEncoding, sizeof(longEncoding));
    const auto longDer = Encode(certificate);
    Require(longDer.size() == 4 + 300 + 13 + 5);
    Require(longDer[0] == 0x30 && longDer[1] == 0x82 && longDer[2] == 0x01 && longDer[3] == 0x3e);
    std::memcpy(info.encoding, &encoding, sizeof(encoding));

    Require(i2d_X509_nid_postfix(nullptr, nullptr) == 0);
    signatureBytes[1] = 0x00;
    Require(Throws([&] { i2d_X509_nid_postfix(&certificate, nullptr); }));
    signatureBytes[1] = 0xcc;
    LibreSsl3Asn1Type integerParameter{2, nullptr};
    algorithm.parameter = &integerParameter;
    Require(Throws([&] { i2d_X509_nid_postfix(&certificate, nullptr); }));
    algorithm.parameter = nullptr;
    Encoding modified{tbs, sizeof(tbs), 1};
    std::memcpy(info.encoding, &modified, sizeof(modified));
    Require(Throws([&] { i2d_X509_nid_postfix(&certificate, nullptr); }));
    std::memcpy(info.encoding, &encoding, sizeof(encoding));
    certificate.signature = nullptr;
    Require(Throws([&] { i2d_X509_nid_postfix(&certificate, nullptr); }));
    std::puts("LibreSSL3 i2d_X509 passed");
    return 0;
}
