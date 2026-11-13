#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"

#include <array>
#include <limits>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(offsetof(LibreSslSslInternal, exData) == 272);
static_assert(sizeof(LibreSslSslInternal) == 432);
static_assert(offsetof(LibreSslSslInternal, rwstate) == 156);
static_assert(offsetof(LibreSslSslInternal, shutdown) == 168);
static_assert(sizeof(LibreSslSsl) == 232);
static_assert(offsetof(LibreSslSsl, rbio) == 16);
static_assert(offsetof(LibreSslSsl, wbio) == 24);
static_assert(offsetof(LibreSslSsl, session) == 128);
static_assert(offsetof(LibreSslSsl, ctx) == 152);
static_assert(offsetof(LibreSslSsl, verifyResult) == 160);
static_assert(offsetof(LibreSslSsl, internal) == 224);
static_assert(offsetof(LibreSslSslInternal, options) == 8);
static_assert(offsetof(LibreSslSslInternal, mode) == 16);
static_assert(offsetof(LibreSslSslInternal, handshakeFunc) == 56);
static_assert(offsetof(LibreSslSslInternal, msgCallback) == 64);
static_assert(offsetof(LibreSslSslInternal, msgCallbackArg) == 72);
static_assert(offsetof(LibreSslSslInternal, verifyCallback) == 88);
static_assert(offsetof(LibreSslSslInternal, readAhead) == 212);
static_assert(offsetof(LibreSslSslInternal, aeadReadCtx) == 240);
static_assert(offsetof(LibreSslSslInternal, aeadWriteCtx) == 248);
static_assert(offsetof(LibreSslSslInternal, encWriteCtx) == 256);
static_assert(offsetof(LibreSslSslInternal, writeHash) == 264);
static_assert(offsetof(LibreSslSslInternal, maxCertList) == 296);
static_assert(offsetof(LibreSslSslInternal, state) == 412);
static_assert(offsetof(LibreSslSsl, method) == 8);
static_assert(offsetof(LibreSslSsl, server) == 40);
static_assert(offsetof(LibreSslSsl, s3) == 48);
static_assert(offsetof(LibreSslSsl, d1) == 56);
static_assert(offsetof(LibreSslSsl, verifyMode) == 136);
static_assert(offsetof(LibreSslSsl, maxSendFragment) == 176);
static_assert(offsetof(LibreSslSsl, encReadCtx) == 208);
static_assert(offsetof(LibreSslSsl, readHash) == 216);

namespace {

const LibreSslSsl& Connection(const LibreSslSsl* ssl, const char* function) {
    if (!ssl) throw std::invalid_argument(std::string(function) + ": null connection");
    return *ssl;
}

const LibreSslSslInternal& Internal(const LibreSslSsl* ssl, const char* function) {
    const LibreSslSsl& connection = Connection(ssl, function);
    if (!connection.internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return *connection.internal;
}

struct Ssl3StateInternal {
    unsigned char beforeWarnAlert[324];
    int warnAlert;
    unsigned char beforeSendConnectionBinding[684];
    int sendConnectionBinding;
    unsigned char beforeAlpnSelected[8];
    const unsigned char* alpnSelected;
    unsigned int alpnSelectedLength;
    unsigned char rest[4];
};

struct Ssl3State {
    unsigned char beforeInternal[136];
    const Ssl3StateInternal* internal;
};

struct Dtls1StateInternal {
    unsigned char beforeMtu[636];
    std::uint32_t mtu;
};

struct Dtls1State {
    unsigned char beforeInternal[32];
    Dtls1StateInternal* internal;
};

using SslReadFunction = int (APS5_VABI*)(LibreSslSsl*, void*, int);
using SslWriteFunction = int (APS5_VABI*)(LibreSslSsl*, const void*, int);
using SslShutdownFunction = int (APS5_VABI*)(LibreSslSsl*);
using SslPendingFunction = int (APS5_VABI*)(const LibreSslSsl*);

struct SslMethodInternal {
    int version;
    unsigned char beforeConnect[36];
    void* connect;
    SslReadFunction read;
    SslReadFunction peek;
    SslWriteFunction write;
    SslShutdownFunction shutdown;
    unsigned char beforePending[40];
    SslPendingFunction pending;
    unsigned char rest[32];
};

struct SslMethod {
    unsigned char beforeInternal[40];
    const SslMethodInternal* internal;
};

static_assert(sizeof(Ssl3StateInternal) == 1040);
static_assert(offsetof(Ssl3StateInternal, warnAlert) == 324);
static_assert(offsetof(Ssl3StateInternal, sendConnectionBinding) == 1012);
static_assert(offsetof(Ssl3StateInternal, alpnSelected) == 1024);
static_assert(offsetof(Ssl3StateInternal, alpnSelectedLength) == 1032);
static_assert(sizeof(Ssl3State) == 144);
static_assert(offsetof(Dtls1StateInternal, mtu) == 636);
static_assert(sizeof(Dtls1State) == 40);
static_assert(sizeof(SslMethodInternal) == 160);
static_assert(offsetof(SslMethodInternal, connect) == 40);
static_assert(offsetof(SslMethodInternal, read) == 48);
static_assert(offsetof(SslMethodInternal, peek) == 56);
static_assert(offsetof(SslMethodInternal, write) == 64);
static_assert(offsetof(SslMethodInternal, shutdown) == 72);
static_assert(offsetof(SslMethodInternal, pending) == 120);
static_assert(sizeof(SslMethod) == 48);

constexpr int ControlSetMessageCallbackArgument = 16;
constexpr int ControlSetMtu = 17;
constexpr int ControlOptions = 32;
constexpr int ControlMode = 33;
constexpr int ControlGetReadAhead = 40;
constexpr int ControlSetReadAhead = 41;
constexpr int ControlGetMaxCertList = 50;
constexpr int ControlSetMaxCertList = 51;
constexpr int ControlSetMaxSendFragment = 52;
constexpr int ControlGetRenegotiationInfoSupport = 76;
constexpr int ControlClearOptions = 77;
constexpr int ControlClearMode = 78;
constexpr int Dtls1Version = 0xFEFF;
constexpr int StateConnectBefore = 0x5000;
constexpr std::int64_t DtlsMinimumMtu = 228;
constexpr std::int64_t MinimumSendFragment = 512;
constexpr std::int64_t MaximumPlainLength = 16384;

LibreSslSsl& Connection(LibreSslSsl* ssl, const char* function) {
    if (!ssl) throw std::invalid_argument(std::string(function) + ": null connection");
    return *ssl;
}

LibreSslSslInternal& Internal(LibreSslSsl* ssl, const char* function) {
    LibreSslSsl& connection = Connection(ssl, function);
    if (!connection.internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return *connection.internal;
}

const SslMethodInternal& Method(const LibreSslSsl& connection, const char* function) {
    const auto* method = static_cast<const SslMethod*>(connection.method);
    if (!method || !method->internal) throw std::invalid_argument(std::string(function) + ": null method");
    return *method->internal;
}

const Ssl3StateInternal* Ssl3Internal(const LibreSslSsl& connection, const char* function) {
    const auto* state = static_cast<const Ssl3State*>(connection.s3);
    if (!state) return nullptr;
    if (!state->internal) throw std::invalid_argument(std::string(function) + ": null SSL3 state");
    return state->internal;
}

constexpr int MinimumNodes = 4;
constexpr int CryptoLibrary = 15;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;

LibreSslStack* NewExDataStack() {
    auto* const stack = static_cast<LibreSslStack*>(malloc_nid_postfix(sizeof(LibreSslStack)));
    if (!stack) return nullptr;
    stack->data = static_cast<char**>(malloc_nid_postfix(MinimumNodes * sizeof(char*)));
    if (!stack->data) {
        free_nid_postfix(stack);
        return nullptr;
    }
    for (int index = 0; index < MinimumNodes; ++index) stack->data[index] = nullptr;
    stack->num = 0;
    stack->sorted = 0;
    stack->numAlloc = MinimumNodes;
    stack->comp = nullptr;
    return stack;
}

void ValidateExDataStack(const LibreSslStack* stack, const char* function) {
    if (!stack->data || stack->num < 0 || stack->numAlloc < MinimumNodes || stack->num > stack->numAlloc) {
        NotImplemented_nid_no_patch(function);
    }
}

constexpr int SslLibrary = 20;
constexpr int SystemLibrary = 2;
constexpr int AnyFunctionCode = 0xfff;
constexpr int ReasonProtocolIsShutdown = 207;
constexpr int ReasonUninitialized = 276;
constexpr int SentShutdown = 1;
constexpr int ReceivedShutdown = 2;
constexpr int StateInit = 0x3000;
constexpr int Nothing = 1;
constexpr int Writing = 2;
constexpr int Reading = 3;
constexpr int X509Lookup = 4;
constexpr int AlertCloseNotify = 0;
constexpr int BioShouldRead = 0x01;
constexpr int BioShouldWrite = 0x02;
constexpr int BioIoSpecial = 0x04;
constexpr int BioRetryConnect = 0x02;
constexpr int BioRetryAccept = 0x03;
constexpr int ErrorNone = 0;
constexpr int ErrorSsl = 1;
constexpr int ErrorWantRead = 2;
constexpr int ErrorWantWrite = 3;
constexpr int ErrorWantX509Lookup = 4;
constexpr int ErrorSyscall = 5;
constexpr int ErrorZeroReturn = 6;
constexpr int ErrorWantConnect = 7;
constexpr int ErrorWantAccept = 8;
constexpr int ReasonBufferLibrary = 7;
constexpr int BioTypeDescriptor = 0x0100;
constexpr int BioControlSetDescriptor = 104;
constexpr int BioControlGetDescriptor = 105;
constexpr int BioNoClose = 0;

constexpr std::array<std::pair<int, int>, 85> StateFunctionCodes{{
    {0x3, 76}, {0x1000, 75}, {0x1003, 79}, {0x1100, 1}, {0x1110, 2}, {0x1111, 3}, {0x1120, 4}, {0x1121, 5},
    {0x1126, 83}, {0x1127, 84}, {0x1130, 6}, {0x1131, 7}, {0x1140, 8}, {0x1141, 9}, {0x1150, 10}, {0x1151, 11},
    {0x1160, 12}, {0x1161, 13}, {0x1170, 14}, {0x1171, 15}, {0x1172, 16}, {0x1173, 17}, {0x1180, 18}, {0x1181, 19},
    {0x1190, 20}, {0x1191, 21}, {0x11a0, 22}, {0x11a1, 23}, {0x11b0, 26}, {0x11b1, 27}, {0x11c0, 28}, {0x11c1, 29},
    {0x11d0, 30}, {0x11d1, 31}, {0x11e0, 32}, {0x11e1, 33}, {0x11f0, 34}, {0x11f1, 35}, {0x1200, 24}, {0x1201, 25},
    {0x2000, 74}, {0x2003, 81}, {0x2100, 36}, {0x2110, 37}, {0x2111, 38}, {0x2112, 39}, {0x2113, 85}, {0x2114, 86},
    {0x2120, 40}, {0x2121, 41}, {0x2122, 42}, {0x2130, 43}, {0x2131, 44}, {0x2140, 45}, {0x2141, 46}, {0x2150, 47},
    {0x2151, 48}, {0x2160, 49}, {0x2161, 50}, {0x2170, 51}, {0x2171, 52}, {0x2180, 53}, {0x2181, 54}, {0x2190, 55},
    {0x2191, 56}, {0x21a0, 57}, {0x21a1, 58}, {0x21b0, 59}, {0x21b1, 60}, {0x21c0, 63}, {0x21c1, 64}, {0x21d0, 65},
    {0x21d1, 66}, {0x21e0, 67}, {0x21e1, 68}, {0x21f0, 69}, {0x21f1, 70}, {0x2200, 71}, {0x2201, 72}, {0x2210, 61},
    {0x2211, 62}, {0x3004, 77}, {0x4000, 73}, {0x5000, 78}, {0x6000, 80},
}};

int StateFunctionCode(int state) {
    for (const auto& [value, code] : StateFunctionCodes)
        if (value == state) return code;
    return AnyFunctionCode;
}

int PutStateError(const LibreSslSslInternal& internal, int reason, int result) {
    LibreSslPutError_nid_no_patch(SslLibrary, StateFunctionCode(internal.state), reason, __FILE__, __LINE__);
    return result;
}

template<typename TFunction> TFunction Dispatch(TFunction function, const char* name) {
    if (!function) throw std::invalid_argument(std::string(name) + ": null method function");
    return function;
}

int RetryError(const LibreSsl::Bio* bio, int first, int firstError, int second, int secondError, const char* function) {
    if (!bio) throw std::invalid_argument(std::string(function) + ": null BIO");
    if (bio->flags & first) return firstError;
    if (bio->flags & second) return secondError;
    if (bio->flags & BioIoSpecial) {
        if (bio->retryReason == BioRetryConnect) return ErrorWantConnect;
        if (bio->retryReason == BioRetryAccept) return ErrorWantAccept;
        return ErrorSyscall;
    }
    return ErrorNone;
}

bool PushExData(LibreSslStack* stack, void* value, const char* function) {
    ValidateExDataStack(stack, function);
    if (stack->numAlloc - stack->num <= 1) {
        if (stack->numAlloc > std::numeric_limits<int>::max() / 2) NotImplemented_nid_no_patch(function);
        auto* const grown = static_cast<char**>(realloc_nid_postfix(stack->data,
            static_cast<std::size_t>(stack->numAlloc) * 2 * sizeof(char*)));
        if (!grown) return false;
        stack->data = grown;
        stack->numAlloc *= 2;
    }
    stack->data[stack->num] = static_cast<char*>(value);
    ++stack->num;
    stack->sorted = 0;
    return true;
}

}

extern "C" {

void* APS5_VABI SSL_get_SSL_CTX_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).ctx;
}

void* APS5_VABI SSL_get_rbio_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).rbio;
}

void* APS5_VABI SSL_get_wbio_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).wbio;
}

int APS5_VABI SSL_version_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).version;
}

int APS5_VABI SSL_check_private_key_nid_postfix(const LibreSslSsl* ssl) {
    if (!ssl) {
        LibreSslPutError_nid_no_patch(20, 0xfff, 67);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int64_t APS5_VABI SSL_get_verify_result_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).verifyResult;
}

int APS5_VABI SSL_get_shutdown_nid_postfix(const LibreSslSsl* ssl) {
    return Internal(ssl, __func__).shutdown;
}

int APS5_VABI SSL_want_nid_postfix(const LibreSslSsl* ssl) {
    return Internal(ssl, __func__).rwstate;
}

void APS5_VABI SSL_set_verify_nid_postfix(LibreSslSsl* ssl, int mode, void* callback) {
    LibreSslSsl& connection = Connection(ssl, __func__);
    if (callback) Internal(ssl, __func__).verifyCallback = callback;
    connection.verifyMode = mode;
}

void APS5_VABI SSL_get0_alpn_selected_nid_postfix(const LibreSslSsl* ssl, const unsigned char** data, unsigned int* length) {
    if (!data || !length) throw std::invalid_argument(std::string(__func__) + ": null output");
    const Ssl3StateInternal* state = Ssl3Internal(Connection(ssl, __func__), __func__);
    *data = state ? state->alpnSelected : nullptr;
    *length = state ? state->alpnSelectedLength : 0;
}

void APS5_VABI SSL_set_connect_state_nid_postfix(LibreSslSsl* ssl) {
    LibreSslSsl& connection = Connection(ssl, __func__);
    LibreSslSslInternal& internal = Internal(ssl, __func__);
    const SslMethodInternal& method = Method(connection, __func__);
    if (connection.encReadCtx || connection.readHash || internal.encWriteCtx || internal.aeadReadCtx || internal.aeadWriteCtx || internal.writeHash)
        NotImplemented_nid_no_patch("SSL_set_connect_state: freeing a live cipher or hash context");
    connection.server = 0;
    internal.shutdown = 0;
    internal.state = StateConnectBefore;
    internal.handshakeFunc = method.connect;
}

std::int64_t APS5_VABI SSL_ctrl_nid_postfix(LibreSslSsl* ssl, int command, std::int64_t argument, void* pointer) {
    switch (command) {
    case ControlGetReadAhead:
        return Internal(ssl, __func__).readAhead;
    case ControlSetReadAhead: {
        LibreSslSslInternal& internal = Internal(ssl, __func__);
        const int previous = internal.readAhead;
        internal.readAhead = static_cast<int>(argument);
        return previous;
    }
    case ControlSetMessageCallbackArgument:
        Internal(ssl, __func__).msgCallbackArg = pointer;
        return 1;
    case ControlOptions:
        return static_cast<std::int64_t>(Internal(ssl, __func__).options |= static_cast<std::uint64_t>(argument));
    case ControlClearOptions:
        return static_cast<std::int64_t>(Internal(ssl, __func__).options &= ~static_cast<std::uint64_t>(argument));
    case ControlMode:
        return static_cast<std::int64_t>(Internal(ssl, __func__).mode |= static_cast<std::uint64_t>(argument));
    case ControlClearMode:
        return static_cast<std::int64_t>(Internal(ssl, __func__).mode &= ~static_cast<std::uint64_t>(argument));
    case ControlGetMaxCertList:
        return Internal(ssl, __func__).maxCertList;
    case ControlSetMaxCertList: {
        LibreSslSslInternal& internal = Internal(ssl, __func__);
        const std::int64_t previous = internal.maxCertList;
        internal.maxCertList = argument;
        return previous;
    }
    case ControlSetMtu: {
        if (argument < DtlsMinimumMtu) return 0;
        const LibreSslSsl& connection = Connection(ssl, __func__);
        if (Method(connection, __func__).version != Dtls1Version) return 0;
        const auto* state = static_cast<const Dtls1State*>(connection.d1);
        if (!state || !state->internal) throw std::invalid_argument(std::string(__func__) + ": null DTLS state");
        state->internal->mtu = static_cast<std::uint32_t>(argument);
        return argument;
    }
    case ControlSetMaxSendFragment:
        if (argument < MinimumSendFragment || argument > MaximumPlainLength) return 0;
        Connection(ssl, __func__).maxSendFragment = static_cast<unsigned int>(argument);
        return 1;
    case ControlGetRenegotiationInfoSupport: {
        const Ssl3StateInternal* state = Ssl3Internal(Connection(ssl, __func__), __func__);
        return state ? state->sendConnectionBinding : 0;
    }
    default:
        NotImplemented_nid_no_patch(("SSL_ctrl: command " + std::to_string(command)).c_str());
    }
}

int APS5_VABI SSL_set_ex_data_nid_postfix(LibreSslSsl* ssl, int index, void* value) {
    LibreSslSslInternal& internal = Internal(ssl, __func__);
    auto* stack = static_cast<LibreSslStack*>(internal.exData.stack);
    if (!stack) {
        stack = NewExDataStack();
        if (!stack) {
            LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, MallocFailure);
            return 0;
        }
        internal.exData.stack = stack;
    } else {
        ValidateExDataStack(stack, __func__);
    }
    while (stack->num <= index) {
        if (!PushExData(stack, nullptr, __func__)) {
            LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, MallocFailure);
            return 0;
        }
    }
    if (index >= 0 && index < stack->num) stack->data[index] = static_cast<char*>(value);
    return 1;
}

void* APS5_VABI SSL_get_ex_data_nid_postfix(const LibreSslSsl* ssl, int index) {
    return LibreSslGetExData_nid_no_patch(&Internal(ssl, __func__).exData, index);
}

int APS5_VABI SSL_read_nid_postfix(LibreSslSsl* ssl, void* buffer, int length) {
    LibreSslSslInternal& internal = Internal(ssl, __func__);
    if (!internal.handshakeFunc) return PutStateError(internal, ReasonUninitialized, -1);
    if (internal.shutdown & ReceivedShutdown) {
        internal.rwstate = Nothing;
        return 0;
    }
    return Dispatch(Method(*ssl, __func__).read, __func__)(ssl, buffer, length);
}

int APS5_VABI SSL_peek_nid_postfix(LibreSslSsl* ssl, void* buffer, int length) {
    LibreSslSslInternal& internal = Internal(ssl, __func__);
    if (!internal.handshakeFunc) return PutStateError(internal, ReasonUninitialized, -1);
    if (internal.shutdown & ReceivedShutdown) return 0;
    return Dispatch(Method(*ssl, __func__).peek, __func__)(ssl, buffer, length);
}

int APS5_VABI SSL_write_nid_postfix(LibreSslSsl* ssl, const void* buffer, int length) {
    LibreSslSslInternal& internal = Internal(ssl, __func__);
    if (!internal.handshakeFunc) return PutStateError(internal, ReasonUninitialized, -1);
    if (internal.shutdown & SentShutdown) {
        internal.rwstate = Nothing;
        return PutStateError(internal, ReasonProtocolIsShutdown, -1);
    }
    return Dispatch(Method(*ssl, __func__).write, __func__)(ssl, buffer, length);
}

int APS5_VABI SSL_shutdown_nid_postfix(LibreSslSsl* ssl) {
    LibreSslSslInternal& internal = Internal(ssl, __func__);
    if (!internal.handshakeFunc) return PutStateError(internal, ReasonUninitialized, -1);
    if (internal.state & StateInit) return 1;
    return Dispatch(Method(*ssl, __func__).shutdown, __func__)(ssl);
}

int APS5_VABI SSL_pending_nid_postfix(const LibreSslSsl* ssl) {
    return Dispatch(Method(Connection(ssl, __func__), __func__).pending, __func__)(ssl);
}

int APS5_VABI SSL_get_error_nid_postfix(const LibreSslSsl* ssl, int result) {
    if (result > 0) return ErrorNone;
    const std::uint64_t error = ERR_peek_error_nid_postfix();
    if (error != 0) return ((error >> 24) & 0xff) == SystemLibrary ? ErrorSyscall : ErrorSsl;
    const LibreSslSslInternal& internal = Internal(ssl, __func__);
    if (result < 0 && internal.rwstate == Reading) {
        const int retry = RetryError(static_cast<const LibreSsl::Bio*>(ssl->rbio), BioShouldRead, ErrorWantRead, BioShouldWrite, ErrorWantWrite, __func__);
        if (retry != ErrorNone) return retry;
    }
    if (result < 0 && internal.rwstate == Writing) {
        const int retry = RetryError(static_cast<const LibreSsl::Bio*>(ssl->wbio), BioShouldWrite, ErrorWantWrite, BioShouldRead, ErrorWantRead, __func__);
        if (retry != ErrorNone) return retry;
    }
    if (result < 0 && internal.rwstate == X509Lookup) return ErrorWantX509Lookup;
    if (result == 0 && (internal.shutdown & ReceivedShutdown)) {
        const auto* state = static_cast<const Ssl3State*>(ssl->s3);
        if (!state || !state->internal) throw std::invalid_argument(std::string(__func__) + ": null SSL3 state");
        if (state->internal->warnAlert == AlertCloseNotify) return ErrorZeroReturn;
    }
    return ErrorSyscall;
}

void APS5_VABI SSL_set_bio_nid_postfix(LibreSslSsl* ssl, LibreSsl::Bio* readBio, LibreSsl::Bio* writeBio) {
    LibreSslSsl& connection = Connection(ssl, __func__);
    auto* const buffering = static_cast<LibreSsl::Bio*>(connection.bbio);
    if (buffering && connection.wbio == buffering) {
        connection.wbio = buffering->next;
        buffering->next = nullptr;
    }
    if (connection.rbio != readBio && connection.rbio != connection.wbio) LibreSslBioFreeAll_nid_no_patch(static_cast<LibreSsl::Bio*>(connection.rbio));
    if (connection.wbio != writeBio) LibreSslBioFreeAll_nid_no_patch(static_cast<LibreSsl::Bio*>(connection.wbio));
    connection.rbio = readBio;
    connection.wbio = writeBio;
}

int APS5_VABI SSL_set_fd_nid_postfix(LibreSslSsl* ssl, int descriptor) {
    Connection(ssl, __func__);
    LibreSsl::Bio* const bio = BIO_new_nid_postfix(LibreSslSocketMethod_nid_no_patch());
    if (!bio) return PutStateError(Internal(ssl, __func__), ReasonBufferLibrary, 0);
    BIO_ctrl_nid_postfix(bio, BioControlSetDescriptor, BioNoClose, &descriptor);
    SSL_set_bio_nid_postfix(ssl, bio, bio);
    return 1;
}

int APS5_VABI SSL_get_fd_nid_postfix(const LibreSslSsl* ssl) {
    int descriptor = -1;
    LibreSsl::Bio* const bio = LibreSslBioFindType_nid_no_patch(static_cast<LibreSsl::Bio*>(Connection(ssl, __func__).rbio), BioTypeDescriptor);
    if (bio) BIO_ctrl_nid_postfix(bio, BioControlGetDescriptor, 0, &descriptor);
    return descriptor;
}

}
