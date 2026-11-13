#include "prx/libSceLibreSsl/include/Api.hpp"
#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslCertKey) == 24);
static_assert(offsetof(LibreSslCertKey, privatekey) == 8);
static_assert(sizeof(LibreSslCert) == 256);
static_assert(sizeof(LibreSslSession) == 232);
static_assert(offsetof(LibreSslSession, peer) == 128);
static_assert(offsetof(LibreSslSession, verifyResult) == 136);
static_assert(offsetof(LibreSslSession, references) == 160);
static_assert(offsetof(LibreSslSession, cipher) == 168);
static_assert(offsetof(LibreSslSession, tlsextHostname) == 192);
static_assert(offsetof(LibreSslSession, internal) == 224);
static_assert(offsetof(LibreSslX509, references) == 28);
static_assert(offsetof(LibreSslSsl, param) == 64);
static_assert(offsetof(LibreSslSsl, tlsextHostname) == 184);

namespace {

struct SessionInternal {
    void* exData;
    void* previous;
    void* next;
    int notResumable;
    void* sessionCertificates;
};

struct SessionCertificates {
    LibreSslStack* chain;
};

static_assert(offsetof(SessionInternal, sessionCertificates) == 32);
static_assert(offsetof(SessionCertificates, chain) == 0);

const LibreSslSsl& Connection(const LibreSslSsl* ssl, const char* function) {
    if (!ssl) throw std::invalid_argument(std::string(function) + ": null connection");
    return *ssl;
}

LibreSslSession* Session(const LibreSslSsl* ssl, const char* function) {
    return static_cast<LibreSslSession*>(Connection(ssl, function).session);
}

const LibreSslCertKey* CurrentKey(const LibreSslSsl* ssl, const char* function) {
    const LibreSslCert* cert = static_cast<const LibreSslCert*>(Connection(ssl, function).cert);
    if (!cert) return nullptr;
    if (!cert->key) throw std::invalid_argument(std::string(function) + ": null current key");
    return cert->key;
}

}

extern "C" {

LibreSslSession* APS5_VABI SSL_get_session_nid_postfix(const LibreSslSsl* ssl) {
    return Session(ssl, __func__);
}

const void* APS5_VABI SSL_get_current_cipher_nid_postfix(const LibreSslSsl* ssl) {
    const LibreSslSession* session = Session(ssl, __func__);
    return session ? session->cipher : nullptr;
}

void* APS5_VABI SSL_get_certificate_nid_postfix(const LibreSslSsl* ssl) {
    const LibreSslCertKey* key = CurrentKey(ssl, __func__);
    return key ? key->x509 : nullptr;
}

void* APS5_VABI SSL_get_privatekey_nid_postfix(const LibreSslSsl* ssl) {
    const LibreSslCertKey* key = CurrentKey(ssl, __func__);
    return key ? key->privatekey : nullptr;
}

LibreSslSession* APS5_VABI SSL_get1_session_nid_postfix(LibreSslSsl* ssl) {
    LibreSslSession* session = Session(ssl, __func__);
    if (session) std::atomic_ref(session->references).fetch_add(1);
    return session;
}

LibreSslX509* APS5_VABI SSL_get_peer_certificate_nid_postfix(const LibreSslSsl* ssl) {
    if (!ssl || !ssl->session) return nullptr;
    LibreSslX509* peer = static_cast<const LibreSslSession*>(ssl->session)->peer;
    if (peer) std::atomic_ref(peer->references).fetch_add(1);
    return peer;
}

const char* APS5_VABI SSL_get_servername_nid_postfix(const LibreSslSsl* ssl, int type) {
    constexpr int HostName = 0;
    if (type != HostName) return nullptr;
    const LibreSslSsl& connection = Connection(ssl, __func__);
    const auto* session = static_cast<const LibreSslSession*>(connection.session);
    return session && !connection.tlsextHostname ? session->tlsextHostname : connection.tlsextHostname;
}

LibreSslStack* APS5_VABI SSL_get_peer_cert_chain_nid_postfix(const LibreSslSsl* ssl) {
    if (!ssl || !ssl->session) return nullptr;
    const auto* internal = static_cast<const SessionInternal*>(static_cast<const LibreSslSession*>(ssl->session)->internal);
    if (!internal) throw std::invalid_argument(std::string(__func__) + ": null session internal state");
    const auto* certificates = static_cast<const SessionCertificates*>(internal->sessionCertificates);
    return certificates ? certificates->chain : nullptr;
}

void* APS5_VABI SSL_get0_param_nid_postfix(LibreSslSsl* ssl) {
    return Connection(ssl, __func__).param;
}

}
