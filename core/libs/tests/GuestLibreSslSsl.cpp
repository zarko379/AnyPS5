#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>

struct TestSslInternal {
    unsigned char beforeRwstate[156];
    int rwstate;
    unsigned char beforeShutdown[8];
    int shutdown;
    unsigned char beforeExData[100];
    LibreSslExData exData;
    unsigned char afterExData[152];
};

struct TestSsl {
    int version;
    const void* method;
    void* rbio;
    void* wbio;
    void* bbio;
    int server;
    void* s3;
    void* d1;
    void* param;
    void* cipherList;
    void* cert;
    unsigned int sidCtxLength;
    unsigned char sidCtx[32];
    void* session;
    int verifyMode;
    int error;
    int errorCode;
    int beforeCtx;
    void* ctx;
    std::int64_t verifyResult;
    int references;
    int clientVersion;
    unsigned int maxSendFragment;
    const char* tlsextHostname;
    int tlsextStatusType;
    void* initialCtx;
    void* encReadCtx;
    void* readHash;
    TestSslInternal* internal;
};

struct TestConnectionInternal {
    unsigned char beforeHandshakeFunc[56];
    void* handshakeFunc;
    unsigned char beforeRwstate[92];
    int rwstate;
    unsigned char beforeShutdown[8];
    int shutdown;
    unsigned char beforeExData[100];
    LibreSslExData exData;
    unsigned char beforeState[132];
    int state;
    unsigned char rest[16];
};

struct TestMethodInternal {
    int version;
    unsigned char beforeConnect[36];
    void* connect;
    int (APS5_VABI* read)(LibreSslSsl*, void*, int);
    int (APS5_VABI* peek)(LibreSslSsl*, void*, int);
    int (APS5_VABI* write)(LibreSslSsl*, const void*, int);
    int (APS5_VABI* shutdown)(LibreSslSsl*);
    unsigned char beforePending[40];
    int (APS5_VABI* pending)(const LibreSslSsl*);
    unsigned char rest[32];
};

struct TestMethod {
    unsigned char beforeInternal[40];
    const TestMethodInternal* internal;
};

struct TestSsl3Internal {
    unsigned char beforeWarnAlert[324];
    int warnAlert;
    unsigned char rest[712];
};

struct TestSsl3State {
    unsigned char beforeInternal[136];
    TestSsl3Internal* internal;
};

struct TestSession {
    unsigned char beforePeer[128];
    LibreSslX509* peer;
    std::int64_t verifyResult;
    std::int64_t timeout;
    std::int64_t time;
    int references;
    const void* cipher;
    std::uint64_t cipherId;
    void* ciphers;
    const char* tlsextHostname;
    unsigned char* tlsextTick;
    std::uint64_t tlsextTicklen;
    std::int64_t tlsextTickLifetimeHint;
    void* internal;
};

struct TestSessionInternal {
    void* exData;
    void* previous;
    void* next;
    int notResumable;
    void* sessionCertificates;
    unsigned char rest[32];
};

struct TestSessionCertificates {
    LibreSslStack* chain;
    unsigned char rest[64];
};

static_assert(offsetof(TestSession, internal) == 224 && offsetof(TestSessionInternal, sessionCertificates) == 32);

static_assert(sizeof(TestConnectionInternal) == 432 && offsetof(TestConnectionInternal, handshakeFunc) == 56 && offsetof(TestConnectionInternal, rwstate) == 156);
static_assert(offsetof(TestConnectionInternal, shutdown) == 168 && offsetof(TestConnectionInternal, exData) == 272 && offsetof(TestConnectionInternal, state) == 412);
static_assert(sizeof(TestMethodInternal) == 160 && offsetof(TestMethodInternal, read) == 48 && offsetof(TestMethodInternal, peek) == 56);
static_assert(offsetof(TestMethodInternal, write) == 64 && offsetof(TestMethodInternal, shutdown) == 72 && offsetof(TestMethodInternal, pending) == 120);
static_assert(sizeof(TestMethod) == 48 && sizeof(TestSsl3Internal) == 1040 && offsetof(TestSsl3Internal, warnAlert) == 324 && sizeof(TestSsl3State) == 144);
static_assert(sizeof(TestSession) == 232 && offsetof(TestSession, peer) == 128 && offsetof(TestSession, references) == 160);
static_assert(offsetof(TestSession, cipher) == 168 && offsetof(TestSession, tlsextHostname) == 192);
static_assert(offsetof(TestSsl, tlsextHostname) == 184 && offsetof(TestSsl, param) == 64);

static_assert(sizeof(TestSslInternal) == 432 && offsetof(TestSslInternal, rwstate) == 156 && offsetof(TestSslInternal, shutdown) == 168);
static_assert(offsetof(TestSslInternal, exData) == 272);
static_assert(sizeof(TestSsl) == 232 && offsetof(TestSsl, cert) == 80 && offsetof(TestSsl, session) == 128 && offsetof(TestSsl, error) == 140 && offsetof(TestSsl, errorCode) == 144 && offsetof(TestSsl, ctx) == 152);
static_assert(offsetof(TestSsl, verifyResult) == 160 && offsetof(TestSsl, internal) == 224);

static LibreSslSsl* Ssl(TestSsl* ssl) {
    return reinterpret_cast<LibreSslSsl*>(ssl);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL SSL check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestCheckPrivateKey() {
    constexpr std::uint64_t firstCode = (8ULL << 24) | (0xfffULL << 12) | 101;
    constexpr std::uint64_t nullSslCode = (20ULL << 24) | (0xfffULL << 12) | 67;
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(8, 0xfff, 101);
    Require(SSL_check_private_key_nid_postfix(nullptr) == 0);
    Require(ERR_peek_error_nid_postfix() == firstCode);
    Require(ERR_get_error_nid_postfix() == firstCode);
    Require(ERR_get_error_nid_postfix() == nullSslCode);
    Require(ERR_get_error_nid_postfix() == 0);

    RequireThrows([] { SSL_check_private_key_nid_postfix(reinterpret_cast<const LibreSslSsl*>(std::uintptr_t{1})); });
    Require(ERR_peek_error_nid_postfix() == 0);

    TestSsl ssl;
    std::memset(&ssl, 0xa5, sizeof(ssl));
    ssl.cert = reinterpret_cast<void*>(std::uintptr_t{1});
    ssl.error = 0x12345678;
    ssl.errorCode = 0x23456789;
    unsigned char before[sizeof(ssl)];
    std::memcpy(before, &ssl, sizeof(ssl));
    ERR_clear_error_nid_postfix();
    RequireThrows([&] { SSL_check_private_key_nid_postfix(Ssl(&ssl)); });
    Require(ERR_peek_error_nid_postfix() == 0);
    Require(std::memcmp(before, &ssl, sizeof(ssl)) == 0);
}

static void TestGetSslCtx() {
    int context = 0;
    int session = 0;
    TestSsl ssl {};
    RequireThrows([] { SSL_get_SSL_CTX_nid_postfix(nullptr); });
    Require(SSL_get_SSL_CTX_nid_postfix(Ssl(&ssl)) == nullptr);
    ssl.ctx = &context;
    ssl.session = &session;
    Require(SSL_get_SSL_CTX_nid_postfix(Ssl(&ssl)) == &context);
}

static void TestGet1Session() {
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(1, 2, 3);
    LibreSslPutError_nid_no_patch(4, 5, 6);
    constexpr std::uint64_t firstError = (std::uint64_t{1} << 24) | (std::uint64_t{2} << 12) | 3;
    constexpr std::uint64_t secondError = (std::uint64_t{4} << 24) | (std::uint64_t{5} << 12) | 6;
    TestSsl ssl {};
    std::array<unsigned char, sizeof(TestSsl)> before {};
    std::memcpy(before.data(), &ssl, sizeof(ssl));
    RequireThrows([] { SSL_get1_session_nid_postfix(nullptr); });
    Require(std::memcmp(&ssl, before.data(), sizeof(ssl)) == 0);
    Require(ERR_peek_error_nid_postfix() == firstError);
    Require(LibreSslPeekLastError_nid_no_patch() == secondError);
    Require(SSL_get1_session_nid_postfix(reinterpret_cast<LibreSslSsl*>(&ssl)) == nullptr);
    Require(std::memcmp(&ssl, before.data(), sizeof(ssl)) == 0);
    Require(ERR_peek_error_nid_postfix() == firstError);
    Require(LibreSslPeekLastError_nid_no_patch() == secondError);
    Require(ERR_get_error_nid_postfix() == firstError);
    Require(ERR_get_error_nid_postfix() == secondError);
    Require(ERR_get_error_nid_postfix() == 0);
}

static void TestGetRbio() {
    int read = 0;
    int write = 0;
    TestSsl ssl {};
    RequireThrows([] { SSL_get_rbio_nid_postfix(nullptr); });
    Require(SSL_get_rbio_nid_postfix(Ssl(&ssl)) == nullptr);
    ssl.rbio = &read;
    ssl.wbio = &write;
    Require(SSL_get_rbio_nid_postfix(Ssl(&ssl)) == &read);
}

static void TestGetWbio() {
    int read = 0;
    int write = 0;
    int buffer = 0;
    TestSsl ssl {};
    RequireThrows([] { SSL_get_wbio_nid_postfix(nullptr); });
    Require(SSL_get_wbio_nid_postfix(Ssl(&ssl)) == nullptr);
    ssl.rbio = &read;
    ssl.wbio = &write;
    ssl.bbio = &buffer;
    Require(SSL_get_wbio_nid_postfix(Ssl(&ssl)) == &write);
}

static void TestVersion() {
    TestSsl ssl {};
    RequireThrows([] { SSL_version_nid_postfix(nullptr); });
    Require(SSL_version_nid_postfix(Ssl(&ssl)) == 0);
    ssl.version = 0x0303;
    Require(SSL_version_nid_postfix(Ssl(&ssl)) == 0x0303);
    ssl.version = 0xFEFF;
    Require(SSL_version_nid_postfix(Ssl(&ssl)) == 0xFEFF);
}

static void TestVerifyResult() {
    TestSsl ssl {};
    RequireThrows([] { SSL_get_verify_result_nid_postfix(nullptr); });
    Require(SSL_get_verify_result_nid_postfix(Ssl(&ssl)) == 0);
    ssl.verifyResult = 20;
    Require(SSL_get_verify_result_nid_postfix(Ssl(&ssl)) == 20);
    ssl.verifyResult = INT64_C(0x100000002);
    Require(SSL_get_verify_result_nid_postfix(Ssl(&ssl)) == INT64_C(0x100000002));
    ssl.verifyResult = -1;
    Require(SSL_get_verify_result_nid_postfix(Ssl(&ssl)) == -1);
}

static void TestGetPeerCertificateNullPaths() {
    LibreSslPutError_nid_no_patch(1, 2, 3);
    LibreSslPutError_nid_no_patch(4, 5, 6);
    TestSsl ssl {};
    unsigned char before[sizeof(ssl)];
    std::memcpy(before, &ssl, sizeof(ssl));
    Require(SSL_get_peer_certificate_nid_postfix(nullptr) == nullptr);
    Require(SSL_get_peer_certificate_nid_postfix(Ssl(&ssl)) == nullptr);
    Require(std::memcmp(&ssl, before, sizeof(ssl)) == 0);
    Require(ERR_peek_error_nid_postfix() == ((std::uint64_t{1} << 24) | (std::uint64_t{2} << 12) | 3));
    Require(ERR_get_error_nid_postfix() == ((std::uint64_t{1} << 24) | (std::uint64_t{2} << 12) | 3));
    Require(ERR_get_error_nid_postfix() == ((std::uint64_t{4} << 24) | (std::uint64_t{5} << 12) | 6));
    Require(ERR_get_error_nid_postfix() == 0);
}

static void TestGetShutdown() {
    TestSslInternal internal {};
    TestSsl ssl {};
    RequireThrows([] { SSL_get_shutdown_nid_postfix(nullptr); });
    RequireThrows([&] { SSL_get_shutdown_nid_postfix(Ssl(&ssl)); });
    ssl.internal = &internal;
    Require(SSL_get_shutdown_nid_postfix(Ssl(&ssl)) == 0);
    internal.rwstate = 3;
    internal.shutdown = 1;
    Require(SSL_get_shutdown_nid_postfix(Ssl(&ssl)) == 1);
    internal.shutdown = 3;
    Require(SSL_get_shutdown_nid_postfix(Ssl(&ssl)) == 3);
}

static void TestWant() {
    TestSslInternal internal {};
    TestSsl ssl {};
    RequireThrows([] { SSL_want_nid_postfix(nullptr); });
    RequireThrows([&] { SSL_want_nid_postfix(Ssl(&ssl)); });
    ssl.internal = &internal;
    Require(SSL_want_nid_postfix(Ssl(&ssl)) == 0);
    internal.rwstate = 1;
    Require(SSL_want_nid_postfix(Ssl(&ssl)) == 1);
    internal.shutdown = 2;
    internal.rwstate = 3;
    Require(SSL_want_nid_postfix(Ssl(&ssl)) == 3);
}

static void FreeExData(TestSslInternal& internal) {
    auto* const stack = static_cast<LibreSslStack*>(internal.exData.stack);
    if (!stack) return;
    CRYPTO_free_nid_postfix(stack->data);
    CRYPTO_free_nid_postfix(stack);
    internal.exData.stack = nullptr;
}

static void TestSetExData() {
    TestSslInternal internal {};
    std::memset(internal.beforeExData, 0xa5, sizeof(internal.beforeExData));
    std::memset(internal.afterExData, 0x5a, sizeof(internal.afterExData));
    TestSsl ssl {};
    int first = 1;
    int replacement = 2;
    RequireThrows([] { SSL_set_ex_data_nid_postfix(nullptr, 0, nullptr); });
    RequireThrows([&] { SSL_set_ex_data_nid_postfix(Ssl(&ssl), 0, nullptr); });
    ssl.internal = &internal;
    Require(SSL_set_ex_data_nid_postfix(Ssl(&ssl), -1, &first) == 1);
    auto* stack = static_cast<LibreSslStack*>(internal.exData.stack);
    Require(stack && stack->num == 0 && stack->numAlloc == 4);
    Require(SSL_set_ex_data_nid_postfix(Ssl(&ssl), 2, &first) == 1);
    Require(stack->num == 3 && stack->data[0] == nullptr && stack->data[1] == nullptr && stack->data[2] == reinterpret_cast<char*>(&first));
    Require(SSL_set_ex_data_nid_postfix(Ssl(&ssl), 2, &replacement) == 1);
    Require(stack->data[2] == reinterpret_cast<char*>(&replacement));
    Require(SSL_set_ex_data_nid_postfix(Ssl(&ssl), 5, nullptr) == 1);
    Require(stack->num == 6 && stack->data[3] == nullptr && stack->data[4] == nullptr && stack->data[5] == nullptr);
    Require(internal.rwstate == 0 && internal.shutdown == 0);
    for (unsigned char byte : internal.beforeExData) Require(byte == 0xa5);
    for (unsigned char byte : internal.afterExData) Require(byte == 0x5a);
    FreeExData(internal);
}

static void TestSetExDataPrefilledStack() {
    TestSslInternal internal {};
    TestSsl ssl {};
    ssl.internal = &internal;
    auto* stack = static_cast<LibreSslStack*>(CRYPTO_malloc_nid_postfix(sizeof(LibreSslStack), nullptr, 0));
    Require(stack != nullptr);
    stack->data = static_cast<char**>(CRYPTO_malloc_nid_postfix(4 * sizeof(char*), nullptr, 0));
    Require(stack->data != nullptr);
    int original = 1;
    int updated = 2;
    stack->num = 1;
    stack->data[0] = reinterpret_cast<char*>(&original);
    stack->data[1] = nullptr;
    stack->data[2] = nullptr;
    stack->data[3] = nullptr;
    stack->sorted = 1;
    stack->numAlloc = 4;
    stack->comp = nullptr;
    internal.exData.stack = stack;
    Require(SSL_set_ex_data_nid_postfix(Ssl(&ssl), 0, &updated) == 1);
    Require(stack->num == 1 && stack->data[0] == reinterpret_cast<char*>(&updated) && stack->sorted == 1);
    Require(SSL_set_ex_data_nid_postfix(Ssl(&ssl), 2, &original) == 1);
    Require(stack->num == 3 && stack->data[1] == nullptr && stack->data[2] == reinterpret_cast<char*>(&original) && stack->sorted == 0);
    FreeExData(internal);
}

static void TestGetPeerCertChain() {
    TestSsl ssl {};
    LibreSslPutError_nid_no_patch(3, 100, 65);
    LibreSslPutError_nid_no_patch(4, 101, 66);
    constexpr std::uint64_t firstError = (std::uint64_t{3} << 24) | (std::uint64_t{100} << 12) | 65;
    constexpr std::uint64_t secondError = (std::uint64_t{4} << 24) | (std::uint64_t{101} << 12) | 66;
    Require(ERR_peek_error_nid_postfix() == firstError);
    Require(SSL_get_peer_cert_chain_nid_postfix(nullptr) == nullptr);
    Require(ERR_peek_error_nid_postfix() == firstError);
    unsigned char before[sizeof(ssl)];
    std::memcpy(before, &ssl, sizeof(ssl));
    Require(SSL_get_peer_cert_chain_nid_postfix(Ssl(&ssl)) == nullptr);
    Require(std::memcmp(&ssl, before, sizeof(ssl)) == 0);
    Require(ERR_peek_error_nid_postfix() == firstError);
    TestSession session {};
    ssl.session = &session;
    std::memcpy(before, &ssl, sizeof(ssl));
    RequireThrows([&] { SSL_get_peer_cert_chain_nid_postfix(Ssl(&ssl)); });
    Require(std::memcmp(&ssl, before, sizeof(ssl)) == 0);
    Require(ERR_peek_error_nid_postfix() == firstError);
    TestSessionInternal sessionInternal {};
    session.internal = &sessionInternal;
    Require(SSL_get_peer_cert_chain_nid_postfix(Ssl(&ssl)) == nullptr);
    TestSessionCertificates certificates {};
    sessionInternal.sessionCertificates = &certificates;
    Require(SSL_get_peer_cert_chain_nid_postfix(Ssl(&ssl)) == nullptr);
    LibreSslStack chain {};
    certificates.chain = &chain;
    Require(SSL_get_peer_cert_chain_nid_postfix(Ssl(&ssl)) == &chain);
    ssl.session = nullptr;
    Require(SSL_get_peer_cert_chain_nid_postfix(Ssl(&ssl)) == nullptr);
    Require(ERR_peek_error_nid_postfix() == firstError);
    Require(ERR_get_error_nid_postfix() == firstError);
    Require(ERR_get_error_nid_postfix() == secondError);
    Require(ERR_get_error_nid_postfix() == 0);
}

static int heapAllocations = 0;
static bool failNextAllocation = false;
static bool failNextReallocation = false;
static std::size_t lastAllocationSize = 0;

static void* APS5_VABI HeapAllocate(std::size_t size) {
    if (failNextAllocation) {
        failNextAllocation = false;
        return nullptr;
    }
    lastAllocationSize = size;
    void* result = std::malloc(size ? size : 1);
    if (result) ++heapAllocations;
    return result;
}

static void APS5_VABI HeapFree(void* pointer) {
    if (pointer) --heapAllocations;
    std::free(pointer);
}

static void* APS5_VABI HeapCalloc(std::size_t count, std::size_t size) {
    void* result = std::calloc(count ? count : 1, size ? size : 1);
    if (result) ++heapAllocations;
    return result;
}

static void* APS5_VABI HeapReallocate(void* pointer, std::size_t size) {
    if (failNextReallocation) {
        failNextReallocation = false;
        return nullptr;
    }
    void* result = std::realloc(pointer, size ? size : 1);
    if (result && !pointer) ++heapAllocations;
    return result;
}

static void* APS5_VABI HeapAlign(std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }
static void* APS5_VABI HeapRealign(void*, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned reallocation"); }
static int APS5_VABI HeapPosixAlign(void**, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }

static void TestSetAlpnProtos() {
    static const unsigned char first[] = {2, 'h', '2'};
    static const unsigned char second[] = {8, 'h', 't', 't', 'p', '/', '1', '.', '1'};
    ERR_clear_error_nid_postfix();
    LibreSslCtxInternal internal {};
    LibreSslCtx context {};
    RequireThrows([] { SSL_CTX_set_alpn_protos_nid_postfix(nullptr, first, sizeof(first)); });
    RequireThrows([&] { SSL_CTX_set_alpn_protos_nid_postfix(&context, first, sizeof(first)); });
    context.internal = &internal;
    const int baseline = heapAllocations;
    Require(SSL_CTX_set_alpn_protos_nid_postfix(&context, first, sizeof(first)) == 0);
    Require(internal.alpnClientProtoList && internal.alpnClientProtoList != first && internal.alpnClientProtoListLen == sizeof(first));
    Require(std::memcmp(internal.alpnClientProtoList, first, sizeof(first)) == 0 && heapAllocations == baseline + 1);
    Require(SSL_CTX_set_alpn_protos_nid_postfix(&context, second, sizeof(second)) == 0);
    Require(internal.alpnClientProtoListLen == sizeof(second) && std::memcmp(internal.alpnClientProtoList, second, sizeof(second)) == 0);
    Require(heapAllocations == baseline + 1);
    failNextAllocation = true;
    Require(SSL_CTX_set_alpn_protos_nid_postfix(&context, first, sizeof(first)) == 1);
    Require(!internal.alpnClientProtoList && internal.alpnClientProtoListLen == sizeof(second) && heapAllocations == baseline);
    Require(ERR_peek_error_nid_postfix() == 0);
    Require(SSL_CTX_set_alpn_protos_nid_postfix(&context, nullptr, 0) == 0);
    Require(internal.alpnClientProtoList && internal.alpnClientProtoListLen == 0 && heapAllocations == baseline + 1);
    Require(SSL_CTX_set_alpn_protos_nid_postfix(&context, first, 1) == 0);
    Require(internal.alpnClientProtoListLen == 1 && internal.alpnClientProtoList[0] == 2 && heapAllocations == baseline + 1);
    lastAllocationSize = 0;
    RequireThrows([&] { SSL_CTX_set_alpn_protos_nid_postfix(&context, nullptr, 2); });
    Require(internal.alpnClientProtoList && internal.alpnClientProtoListLen == 1 && heapAllocations == baseline + 1 && lastAllocationSize == 2);
    CRYPTO_free_nid_postfix(internal.alpnClientProtoList);
    Require(heapAllocations == baseline && ERR_peek_error_nid_postfix() == 0);
}

static bool HasGroups(const LibreSslCtxInternal& internal, std::initializer_list<std::uint16_t> expected) {
    if (internal.tlsextSupportedgroupsLength != expected.size() || !internal.tlsextSupportedgroups) return false;
    std::size_t index = 0;
    for (const std::uint16_t id : expected)
        if (internal.tlsextSupportedgroups[index++] != id) return false;
    return true;
}

static void TestSetGroupsList() {
    ERR_clear_error_nid_postfix();
    LibreSslCtxInternal internal {};
    LibreSslCtx context {};
    RequireThrows([] { SSL_CTX_set1_groups_list_nid_postfix(nullptr, "P-256"); });
    RequireThrows([&] { SSL_CTX_set1_groups_list_nid_postfix(&context, nullptr); });
    const int baseline = heapAllocations;
    Require(SSL_CTX_set1_groups_list_nid_postfix(&context, "P-256:nosuchgroup") == 0 && heapAllocations == baseline);
    RequireThrows([&] { SSL_CTX_set1_groups_list_nid_postfix(&context, "P-256"); });
    Require(heapAllocations == baseline);
    context.internal = &internal;
    Require(SSL_CTX_set1_groups_list_nid_postfix(&context, "P-256") == 1 && HasGroups(internal, {23}) && heapAllocations == baseline + 1);
    Require(SSL_CTX_set1_groups_list_nid_postfix(&context, "X25519:prime256v1:secp384r1:P-521:K-163:B-571:brainpoolP512r1:secp160k1") == 1);
    Require(HasGroups(internal, {29, 23, 24, 25, 1, 14, 28, 15}) && heapAllocations == baseline + 1);
    Require(SSL_CTX_set1_groups_list_nid_postfix(&context, "secp224k1:P-224:P-192:prime192v1:secp521r1") == 1);
    Require(HasGroups(internal, {20, 21, 19, 19, 25}));
    for (const char* rejected : {"", ":", "P-256:", ":P-256", "P-256::P-384", "p-256", "nosuchgroup", "CN", "commonName", "NULL", "UNDEF", "undefined", "secp112r1"}) {
        Require(SSL_CTX_set1_groups_list_nid_postfix(&context, rejected) == 0);
        Require(HasGroups(internal, {20, 21, 19, 19, 25}) && heapAllocations == baseline + 1);
    }
    failNextReallocation = true;
    Require(SSL_CTX_set1_groups_list_nid_postfix(&context, "commonName") == 0 && !failNextReallocation);
    failNextReallocation = true;
    Require(SSL_CTX_set1_groups_list_nid_postfix(&context, "nosuchgroup") == 0 && failNextReallocation);
    Require(SSL_CTX_set1_groups_list_nid_postfix(&context, "P-256") == 0 && !failNextReallocation);
    Require(HasGroups(internal, {20, 21, 19, 19, 25}) && heapAllocations == baseline + 1);
    failNextAllocation = true;
    Require(SSL_CTX_set1_groups_list_nid_postfix(&context, "P-256") == 0 && !failNextAllocation);
    Require(HasGroups(internal, {20, 21, 19, 19, 25}) && heapAllocations == baseline + 1);
    Require(OBJ_create_nid_postfix("1.2.3.4.5.6.7.8.9", "groupShortName", "group long name") > 0);
    failNextReallocation = true;
    Require(SSL_CTX_set1_groups_list_nid_postfix(&context, "group long name") == 0 && !failNextReallocation);
    failNextReallocation = true;
    Require(SSL_CTX_set1_groups_list_nid_postfix(&context, "groupShortName") == 0 && !failNextReallocation);
    failNextReallocation = true;
    Require(SSL_CTX_set1_groups_list_nid_postfix(&context, "group") == 0 && failNextReallocation);
    failNextReallocation = false;
    OBJ_cleanup_nid_postfix();
    ERR_clear_error_nid_postfix();
    CRYPTO_free_nid_postfix(internal.tlsextSupportedgroups);
    Require(heapAllocations == baseline);
}

static std::uint64_t ErrorCode(std::uint64_t library, std::uint64_t function, std::uint64_t reason) {
    return (library << 24) | (function << 12) | reason;
}

static void TestGetExData() {
    TestSslInternal internal {};
    TestSsl ssl {};
    int first = 1;
    RequireThrows([] { SSL_get_ex_data_nid_postfix(nullptr, 0); });
    RequireThrows([&] { SSL_get_ex_data_nid_postfix(Ssl(&ssl), 0); });
    ssl.internal = &internal;
    Require(SSL_get_ex_data_nid_postfix(Ssl(&ssl), 0) == nullptr);
    Require(SSL_get_ex_data_nid_postfix(Ssl(&ssl), -1) == nullptr);
    Require(SSL_set_ex_data_nid_postfix(Ssl(&ssl), 2, &first) == 1);
    Require(SSL_get_ex_data_nid_postfix(Ssl(&ssl), 2) == &first);
    Require(SSL_get_ex_data_nid_postfix(Ssl(&ssl), 1) == nullptr);
    Require(SSL_get_ex_data_nid_postfix(Ssl(&ssl), 3) == nullptr);
    Require(SSL_get_ex_data_nid_postfix(Ssl(&ssl), -1) == nullptr);
    Require(SSL_get_ex_data_nid_postfix(Ssl(&ssl), 1 << 30) == nullptr);
    auto* stack = static_cast<LibreSslStack*>(internal.exData.stack);
    char** const data = stack->data;
    stack->data = nullptr;
    RequireThrows([&] { SSL_get_ex_data_nid_postfix(Ssl(&ssl), 0); });
    Require(SSL_get_ex_data_nid_postfix(Ssl(&ssl), 3) == nullptr);
    stack->data = data;
    FreeExData(internal);

    LibreSslCtxInternal contextInternal {};
    LibreSslCtx context {};
    RequireThrows([] { SSL_CTX_get_ex_data_nid_postfix(nullptr, 0); });
    RequireThrows([&] { SSL_CTX_get_ex_data_nid_postfix(&context, 0); });
    context.internal = &contextInternal;
    Require(SSL_CTX_get_ex_data_nid_postfix(&context, 0) == nullptr);
    Require(SSL_CTX_set_ex_data_nid_postfix(&context, 1, &first) == 1);
    Require(SSL_CTX_get_ex_data_nid_postfix(&context, 1) == &first);
    Require(SSL_CTX_get_ex_data_nid_postfix(&context, 0) == nullptr);
    Require(SSL_CTX_get_ex_data_nid_postfix(&context, 2) == nullptr);
    Require(SSL_CTX_get_ex_data_nid_postfix(&context, -1) == nullptr);
    auto* contextStack = static_cast<LibreSslStack*>(contextInternal.exData);
    CRYPTO_free_nid_postfix(contextStack->data);
    CRYPTO_free_nid_postfix(contextStack);
}

static void TestSessionAccessors() {
    TestSsl ssl {};
    TestSession session {};
    LibreSslX509 peer {};
    session.references = 1;
    peer.references = 1;
    Require(SSL_get1_session_nid_postfix(Ssl(&ssl)) == nullptr);
    ssl.session = &session;
    Require(static_cast<void*>(SSL_get1_session_nid_postfix(Ssl(&ssl))) == &session && session.references == 2);
    Require(static_cast<void*>(SSL_get1_session_nid_postfix(Ssl(&ssl))) == &session && session.references == 3);
    Require(SSL_get_peer_certificate_nid_postfix(Ssl(&ssl)) == nullptr && session.references == 3);
    session.peer = &peer;
    Require(SSL_get_peer_certificate_nid_postfix(Ssl(&ssl)) == &peer && peer.references == 2);
    Require(SSL_get_peer_certificate_nid_postfix(Ssl(&ssl)) == &peer && peer.references == 3 && session.references == 3);

    static const char connectionName[] = "connection.example";
    static const char sessionName[] = "session.example";
    TestSsl named {};
    Require(SSL_get_servername_nid_postfix(nullptr, 1) == nullptr);
    Require(SSL_get_servername_nid_postfix(nullptr, -1) == nullptr);
    RequireThrows([] { SSL_get_servername_nid_postfix(nullptr, 0); });
    Require(SSL_get_servername_nid_postfix(Ssl(&named), 0) == nullptr);
    named.tlsextHostname = connectionName;
    Require(SSL_get_servername_nid_postfix(Ssl(&named), 0) == connectionName);
    Require(SSL_get_servername_nid_postfix(Ssl(&named), 1) == nullptr);
    TestSession namedSession {};
    namedSession.tlsextHostname = sessionName;
    named.session = &namedSession;
    Require(SSL_get_servername_nid_postfix(Ssl(&named), 0) == connectionName);
    named.tlsextHostname = nullptr;
    Require(SSL_get_servername_nid_postfix(Ssl(&named), 0) == sessionName);
    namedSession.tlsextHostname = nullptr;
    Require(SSL_get_servername_nid_postfix(Ssl(&named), 0) == nullptr);

    int parameters = 0;
    RequireThrows([] { SSL_get0_param_nid_postfix(nullptr); });
    Require(SSL_get0_param_nid_postfix(Ssl(&named)) == nullptr);
    named.param = &parameters;
    Require(SSL_get0_param_nid_postfix(Ssl(&named)) == &parameters);
}

struct DispatchLog {
    LibreSslSsl* ssl;
    const void* buffer;
    int length;
    int calls;
};

static DispatchLog dispatchLog;

static int APS5_VABI DispatchRead(LibreSslSsl* ssl, void* buffer, int length) {
    dispatchLog = {ssl, buffer, length, dispatchLog.calls + 1};
    return 7;
}

static int APS5_VABI DispatchPeek(LibreSslSsl* ssl, void* buffer, int length) {
    dispatchLog = {ssl, buffer, length, dispatchLog.calls + 1};
    return 8;
}

static int APS5_VABI DispatchWrite(LibreSslSsl* ssl, const void* buffer, int length) {
    dispatchLog = {ssl, buffer, length, dispatchLog.calls + 1};
    return 9;
}

static int APS5_VABI DispatchShutdown(LibreSslSsl* ssl) {
    dispatchLog = {ssl, nullptr, 0, dispatchLog.calls + 1};
    return 10;
}

static int APS5_VABI DispatchPending(const LibreSslSsl* ssl) {
    dispatchLog = {const_cast<LibreSslSsl*>(ssl), nullptr, 0, dispatchLog.calls + 1};
    return 11;
}

static void TestDispatch() {
    ERR_clear_error_nid_postfix();
    TestMethodInternal methodInternal {};
    TestMethod method {};
    method.internal = &methodInternal;
    TestConnectionInternal internal {};
    TestSsl ssl {};
    ssl.method = &method;
    ssl.internal = reinterpret_cast<TestSslInternal*>(&internal);
    LibreSslSsl* const connection = Ssl(&ssl);
    char buffer[4] {};
    dispatchLog = {};

    RequireThrows([&] { SSL_read_nid_postfix(nullptr, buffer, 4); });
    RequireThrows([&] { SSL_write_nid_postfix(nullptr, buffer, 4); });
    RequireThrows([&] { SSL_peek_nid_postfix(nullptr, buffer, 4); });
    RequireThrows([&] { SSL_shutdown_nid_postfix(nullptr); });
    RequireThrows([&] { SSL_pending_nid_postfix(nullptr); });

    internal.state = 0x5000;
    internal.shutdown = 3;
    Require(SSL_read_nid_postfix(connection, buffer, 4) == -1);
    Require(ERR_get_error_nid_postfix() == ErrorCode(20, 78, 276));
    Require(SSL_peek_nid_postfix(connection, buffer, 4) == -1);
    Require(ERR_get_error_nid_postfix() == ErrorCode(20, 78, 276));
    internal.state = 0x1234;
    Require(SSL_write_nid_postfix(connection, buffer, 4) == -1);
    Require(ERR_get_error_nid_postfix() == ErrorCode(20, 0xfff, 276));
    internal.state = 0x2141;
    Require(SSL_shutdown_nid_postfix(connection) == -1);
    Require(ERR_get_error_nid_postfix() == ErrorCode(20, 46, 276));
    internal.state = 0x3;
    Require(SSL_shutdown_nid_postfix(connection) == -1);
    Require(ERR_get_error_nid_postfix() == ErrorCode(20, 76, 276));
    Require(ERR_get_error_nid_postfix() == 0 && dispatchLog.calls == 0 && internal.rwstate == 0);

    internal.handshakeFunc = &internal;
    internal.rwstate = 3;
    internal.shutdown = 2;
    Require(SSL_peek_nid_postfix(connection, buffer, 4) == 0 && internal.rwstate == 3);
    Require(SSL_read_nid_postfix(connection, buffer, 4) == 0 && internal.rwstate == 1);
    Require(dispatchLog.calls == 0);
    RequireThrows([&] { SSL_write_nid_postfix(connection, buffer, 4); });
    methodInternal.write = DispatchWrite;
    internal.rwstate = 2;
    Require(SSL_write_nid_postfix(connection, buffer, 3) == 9 && internal.rwstate == 2);
    Require(dispatchLog.ssl == connection && dispatchLog.buffer == buffer && dispatchLog.length == 3 && dispatchLog.calls == 1);
    internal.shutdown = 1;
    internal.state = 0x1110;
    Require(SSL_write_nid_postfix(connection, buffer, 3) == -1 && internal.rwstate == 1 && dispatchLog.calls == 1);
    Require(ERR_get_error_nid_postfix() == ErrorCode(20, 2, 207));
    Require(ERR_get_error_nid_postfix() == 0);

    RequireThrows([&] { SSL_read_nid_postfix(connection, buffer, 4); });
    RequireThrows([&] { SSL_peek_nid_postfix(connection, buffer, 4); });
    methodInternal.read = DispatchRead;
    methodInternal.peek = DispatchPeek;
    Require(SSL_read_nid_postfix(connection, buffer, 4) == 7 && dispatchLog.length == 4 && dispatchLog.calls == 2);
    Require(SSL_peek_nid_postfix(connection, buffer + 1, 2) == 8 && dispatchLog.buffer == buffer + 1 && dispatchLog.calls == 3);

    internal.state = 0x1000;
    Require(SSL_shutdown_nid_postfix(connection) == 1 && dispatchLog.calls == 3);
    internal.state = 0x2000;
    Require(SSL_shutdown_nid_postfix(connection) == 1 && dispatchLog.calls == 3);
    internal.state = 0x3;
    RequireThrows([&] { SSL_shutdown_nid_postfix(connection); });
    methodInternal.shutdown = DispatchShutdown;
    Require(SSL_shutdown_nid_postfix(connection) == 10 && dispatchLog.ssl == connection && dispatchLog.calls == 4);

    RequireThrows([&] { SSL_pending_nid_postfix(connection); });
    methodInternal.pending = DispatchPending;
    Require(SSL_pending_nid_postfix(connection) == 11 && dispatchLog.ssl == connection && dispatchLog.calls == 5);
    method.internal = nullptr;
    RequireThrows([&] { SSL_pending_nid_postfix(connection); });
    ssl.method = nullptr;
    RequireThrows([&] { SSL_read_nid_postfix(connection, buffer, 4); });
    Require(ERR_get_error_nid_postfix() == 0);
}

static void TestGetError() {
    ERR_clear_error_nid_postfix();
    TestConnectionInternal internal {};
    TestSsl ssl {};
    LibreSsl::Bio readBio {};
    LibreSsl::Bio writeBio {};
    TestSsl3Internal stateInternal {};
    TestSsl3State state {};
    state.internal = &stateInternal;
    LibreSslSsl* const connection = Ssl(&ssl);
    Require(SSL_get_error_nid_postfix(nullptr, 1) == 0);
    RequireThrows([] { SSL_get_error_nid_postfix(nullptr, 0); });
    RequireThrows([&] { SSL_get_error_nid_postfix(connection, -1); });
    ssl.internal = reinterpret_cast<TestSslInternal*>(&internal);

    LibreSslPutError_nid_no_patch(2, 1, 3);
    Require(SSL_get_error_nid_postfix(nullptr, 0) == 5);
    Require(SSL_get_error_nid_postfix(nullptr, -1) == 5);
    Require(SSL_get_error_nid_postfix(connection, 5) == 0);
    LibreSslPutError_nid_no_patch(20, 1, 3);
    Require(SSL_get_error_nid_postfix(connection, -1) == 5);
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(20, 1, 3);
    LibreSslPutError_nid_no_patch(2, 1, 3);
    Require(SSL_get_error_nid_postfix(connection, 0) == 1);
    ERR_clear_error_nid_postfix();

    Require(SSL_get_error_nid_postfix(connection, -1) == 5);
    Require(SSL_get_error_nid_postfix(connection, 0) == 5);
    internal.rwstate = 3;
    RequireThrows([&] { SSL_get_error_nid_postfix(connection, -1); });
    Require(SSL_get_error_nid_postfix(connection, 0) == 5);
    ssl.rbio = &readBio;
    Require(SSL_get_error_nid_postfix(connection, -1) == 5);
    readBio.flags = 0x09;
    Require(SSL_get_error_nid_postfix(connection, -1) == 2);
    readBio.flags = 0x03;
    Require(SSL_get_error_nid_postfix(connection, -1) == 2);
    readBio.flags = 0x02;
    Require(SSL_get_error_nid_postfix(connection, -1) == 3);
    readBio.flags = 0x06;
    Require(SSL_get_error_nid_postfix(connection, -1) == 3);
    readBio.flags = 0x04;
    readBio.retryReason = 0x02;
    Require(SSL_get_error_nid_postfix(connection, -1) == 7);
    readBio.retryReason = 0x03;
    Require(SSL_get_error_nid_postfix(connection, -1) == 8);
    readBio.retryReason = 0x01;
    Require(SSL_get_error_nid_postfix(connection, -1) == 5);

    internal.rwstate = 2;
    RequireThrows([&] { SSL_get_error_nid_postfix(connection, -1); });
    ssl.wbio = &writeBio;
    Require(SSL_get_error_nid_postfix(connection, -1) == 5);
    writeBio.flags = 0x03;
    Require(SSL_get_error_nid_postfix(connection, -1) == 3);
    writeBio.flags = 0x01;
    Require(SSL_get_error_nid_postfix(connection, -1) == 2);
    writeBio.flags = 0x05;
    Require(SSL_get_error_nid_postfix(connection, -1) == 2);
    writeBio.flags = 0x04;
    writeBio.retryReason = 0x02;
    Require(SSL_get_error_nid_postfix(connection, -1) == 7);
    writeBio.retryReason = 0x03;
    Require(SSL_get_error_nid_postfix(connection, -1) == 8);
    writeBio.retryReason = 0x00;
    Require(SSL_get_error_nid_postfix(connection, -1) == 5);

    internal.rwstate = 4;
    Require(SSL_get_error_nid_postfix(connection, -1) == 4);
    Require(SSL_get_error_nid_postfix(connection, 0) == 5);
    internal.rwstate = 1;
    Require(SSL_get_error_nid_postfix(connection, -1) == 5);

    internal.shutdown = 1;
    Require(SSL_get_error_nid_postfix(connection, 0) == 5);
    internal.shutdown = 2;
    RequireThrows([&] { SSL_get_error_nid_postfix(connection, 0); });
    ssl.s3 = &state;
    Require(SSL_get_error_nid_postfix(connection, 0) == 6);
    Require(SSL_get_error_nid_postfix(connection, -1) == 5);
    stateInternal.warnAlert = 100;
    Require(SSL_get_error_nid_postfix(connection, 0) == 5);
    stateInternal.warnAlert = 0;
    state.internal = nullptr;
    RequireThrows([&] { SSL_get_error_nid_postfix(connection, 0); });
    Require(ERR_get_error_nid_postfix() == 0);
}

int main() {
    void* heap[10] {reinterpret_cast<void*>(HeapAllocate), reinterpret_cast<void*>(HeapFree), reinterpret_cast<void*>(HeapCalloc),
        reinterpret_cast<void*>(HeapReallocate), reinterpret_cast<void*>(HeapAlign), reinterpret_cast<void*>(HeapRealign),
        reinterpret_cast<void*>(HeapPosixAlign)};
    ApplicationHeapRegister_nid_no_patch(heap);
    TestGet1Session();
    TestCheckPrivateKey();
    TestGetSslCtx();
    TestGetRbio();
    TestGetWbio();
    TestVersion();
    TestVerifyResult();
    TestGetPeerCertificateNullPaths();
    TestGetShutdown();
    TestWant();
    TestSetExData();
    TestSetExDataPrefilledStack();
    TestGetPeerCertChain();
    TestGetExData();
    TestSessionAccessors();
    TestDispatch();
    TestGetError();
    TestSetAlpnProtos();
    TestSetGroupsList();
    return 0;
}
