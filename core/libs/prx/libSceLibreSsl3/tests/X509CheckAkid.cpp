#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_check_akid check failed at line %u\n", location.line());
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

Asn1String String(unsigned char* data, int length, int type) {
    Asn1String value{};
    value.length = length;
    value.type = type;
    value.data = data;
    return value;
}

}

int main() {
    unsigned char keyBytes[]{1, 2, 3, 4};
    unsigned char otherKeyBytes[]{1, 2, 3, 5};
    unsigned char serialBytes[]{0x10, 0x20};
    Asn1String issuerKey = String(keyBytes, 4, 4);
    Asn1String matchingKey = String(keyBytes, 4, 4);
    Asn1String otherKey = String(otherKeyBytes, 4, 4);
    Asn1String serial = String(serialBytes, 2, 2);
    Asn1String sameSerial = String(serialBytes, 2, 2);
    Asn1String negativeSerial = String(serialBytes, 2, 0x102);
    Asn1String shortSerial = String(serialBytes, 1, 2);

    LibreSsl3CertificateInfo info{};
    info.serialNumber = &serial;
    LibreSsl3PurposeX509 issuer{};
    issuer.certInfo = &info;
    issuer.skid = &issuerKey;

    Require(X509_check_akid_nid_postfix(&issuer, nullptr) == 0);
    Require(X509_check_akid_nid_postfix(nullptr, nullptr) == 0);
    LibreSsl3AuthorityKeyId akid{};
    Require(X509_check_akid_nid_postfix(&issuer, &akid) == 0);
    Require(X509_check_akid_nid_postfix(nullptr, &akid) == 0);

    akid.keyid = &matchingKey;
    Require(X509_check_akid_nid_postfix(&issuer, &akid) == 0);
    akid.keyid = &otherKey;
    Require(X509_check_akid_nid_postfix(&issuer, &akid) == 30);
    issuer.skid = nullptr;
    Require(X509_check_akid_nid_postfix(&issuer, &akid) == 0);
    issuer.skid = &issuerKey;
    akid.serial = &shortSerial;
    Require(X509_check_akid_nid_postfix(&issuer, &akid) == 30);
    akid.keyid = &matchingKey;
    Require(X509_check_akid_nid_postfix(&issuer, &akid) == 31);
    akid.serial = &negativeSerial;
    Require(X509_check_akid_nid_postfix(&issuer, &akid) == 31);
    akid.serial = &sameSerial;
    Require(X509_check_akid_nid_postfix(&issuer, &akid) == 0);

    LibreSslGeneralName uri{6, nullptr};
    LibreSslGeneralName directory{4, nullptr};
    char* names[]{reinterpret_cast<char*>(&uri), reinterpret_cast<char*>(&directory)};
    LibreSsl3Stack stack{1, names, 0, 2, nullptr};
    akid.issuer = &stack;
    Require(X509_check_akid_nid_postfix(&issuer, &akid) == 0);
    stack.num = 2;
    Require(Throws([&] { X509_check_akid_nid_postfix(&issuer, &akid); }));
    akid.serial = &shortSerial;
    Require(X509_check_akid_nid_postfix(&issuer, &akid) == 31);
    akid.serial = nullptr;
    akid.keyid = nullptr;
    stack.num = 1;
    Require(X509_check_akid_nid_postfix(nullptr, &akid) == 0);

    akid.keyid = &matchingKey;
    Require(Throws([&] { X509_check_akid_nid_postfix(nullptr, &akid); }));
    akid.keyid = nullptr;
    akid.serial = &sameSerial;
    Require(Throws([&] { X509_check_akid_nid_postfix(nullptr, &akid); }));
    info.serialNumber = nullptr;
    Require(Throws([&] { X509_check_akid_nid_postfix(&issuer, &akid); }));
    issuer.certInfo = nullptr;
    Require(Throws([&] { X509_check_akid_nid_postfix(&issuer, &akid); }));
    std::puts("LibreSSL3 X509_check_akid passed");
    return 0;
}
