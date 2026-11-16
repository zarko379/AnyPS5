#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc RTCCertificate check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

struct FakeIdentity {
    const void* const* vtable;
};

void* deleted = nullptr;
int deletions = 0;

void APS5_VABI DeleteIdentity(void* identity) {
    deleted = identity;
    ++deletions;
}

void APS5_VABI CompleteDestructor(void*) {
    Require(false);
}

void TestDeletesTheIdentityThroughItsDeletingDestructor() {
    const std::array<const void*, 3> vtable {reinterpret_cast<const void*>(&CompleteDestructor), reinterpret_cast<const void*>(&DeleteIdentity), nullptr};
    FakeIdentity identity {vtable.data()};
    WebRtcRtcCertificate certificate {1, &identity};
    _ZN3rtc14RTCCertificateD1Ev_nid_postfix(&certificate);
    Require(deletions == 1 && deleted == &identity);
    Require(certificate.refCount == 1);
}

void TestWithoutIdentityDoesNothing() {
    WebRtcRtcCertificate certificate {0, nullptr};
    const int before = deletions;
    _ZN3rtc14RTCCertificateD1Ev_nid_postfix(&certificate);
    Require(deletions == before);
}

void TestInvalidArgumentsThrow() {
    RequireInvalidArgument([] { _ZN3rtc14RTCCertificateD1Ev_nid_postfix(nullptr); });
    FakeIdentity withoutVtable {nullptr};
    WebRtcRtcCertificate certificate {1, &withoutVtable};
    RequireInvalidArgument([&] { _ZN3rtc14RTCCertificateD1Ev_nid_postfix(&certificate); });
    const std::array<const void*, 2> vtable {reinterpret_cast<const void*>(&CompleteDestructor), nullptr};
    FakeIdentity withoutDestructor {vtable.data()};
    certificate.identity = &withoutDestructor;
    RequireInvalidArgument([&] { _ZN3rtc14RTCCertificateD1Ev_nid_postfix(&certificate); });
}

struct FakeSslCertificate {
    const void* const* vtable;
    std::int64_t expiration;
};

struct FakeCertificateIdentity {
    const void* const* vtable;
    const FakeSslCertificate* certificate;
};

const void* APS5_VABI IdentityCertificate(const void* identity) {
    return static_cast<const FakeCertificateIdentity*>(identity)->certificate;
}

std::int64_t APS5_VABI CertificateExpiration(const void* certificate) {
    return static_cast<const FakeSslCertificate*>(certificate)->expiration;
}

void TestHasExpired() {
    const std::array<const void*, 8> certificateVtable {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, reinterpret_cast<const void*>(&CertificateExpiration)};
    const std::array<const void*, 3> identityVtable {nullptr, nullptr, reinterpret_cast<const void*>(&IdentityCertificate)};
    FakeSslCertificate sslCertificate {certificateVtable.data(), 100};
    FakeCertificateIdentity identity {identityVtable.data(), &sslCertificate};
    WebRtcRtcCertificate certificate {1, &identity};
    Require(!_ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 99999));
    Require(_ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 100000));
    Require(_ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 100001));
    sslCertificate.expiration = -1;
    Require(_ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 0));
    sslCertificate.expiration = 0;
    Require(_ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 0));
    sslCertificate.expiration = -2;
    Require(!_ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 0xFFFFFFFFFFFFF82Full));
    Require(_ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 0xFFFFFFFFFFFFF830ull));
}

void TestHasExpiredInvalidArgumentsThrow() {
    const std::array<const void*, 8> certificateVtable {};
    const std::array<const void*, 3> identityVtable {nullptr, nullptr, reinterpret_cast<const void*>(&IdentityCertificate)};
    const std::array<const void*, 3> emptyVtable {};
    FakeSslCertificate sslCertificate {certificateVtable.data(), 1};
    FakeCertificateIdentity identity {identityVtable.data(), &sslCertificate};
    WebRtcRtcCertificate certificate {1, &identity};
    RequireInvalidArgument([&] { _ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 0); });
    identity.certificate = nullptr;
    RequireInvalidArgument([&] { _ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 0); });
    identity.vtable = emptyVtable.data();
    RequireInvalidArgument([&] { _ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 0); });
    certificate.identity = nullptr;
    RequireInvalidArgument([&] { _ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(&certificate, 0); });
    RequireInvalidArgument([] { _ZNK3rtc14RTCCertificate10HasExpiredEm_nid_postfix(nullptr, 0); });
}

}

int main() {
    TestDeletesTheIdentityThroughItsDeletingDestructor();
    TestWithoutIdentityDoesNothing();
    TestInvalidArgumentsThrow();
    TestHasExpired();
    TestHasExpiredInvalidArgumentsThrow();
    return 0;
}
