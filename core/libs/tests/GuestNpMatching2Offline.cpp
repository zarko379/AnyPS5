#include "prx/libSceNpMatching2/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <initializer_list>





namespace {

constexpr int ResolverTimedOut = static_cast<int>(0x804101E2u);
constexpr int NotInitialized = static_cast<int>(0x80550C03u);
constexpr int ContextMax = static_cast<int>(0x80550C04u);
constexpr int ContextNotStarted = static_cast<int>(0x80550C08u);
constexpr int InvalidArgument = static_cast<int>(0x80550C0Au);
constexpr int InvalidContextId = static_cast<int>(0x80550C0Bu);

int failures = 0;

void Require(bool value, const char* name, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s: %s\n", name, what);
    ++failures;
}

void APS5_VABI IgnoreLobbyEvent(std::uint16_t, std::uint64_t, std::uint16_t, const void*, void*) {
}

void APS5_VABI IgnoreLobbyMessage(std::uint16_t, std::uint64_t, std::uint16_t, std::uint16_t, const void*, void*) {
}

}

int main() {
    NpMatching2InitializeParameter init{};
    NpMatching2CreateContextParameter create{};
    create.size = 0x28;
    NpMatching2CreateContextParameterA createA{};
    createA.userId = 1;
    createA.size = 0x10;
    unsigned char requestOpt[24] = {};
    std::uint16_t context = 77;
    int callbackArgument = 0;
    auto* lobbyEvent = reinterpret_cast<void*>(&IgnoreLobbyEvent);
    auto* lobbyMessage = reinterpret_cast<void*>(&IgnoreLobbyMessage);

    Require(sceNpMatching2CreateContext(nullptr, nullptr) == NotInitialized, "sceNpMatching2CreateContext", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpMatching2DestroyContext(1) == NotInitialized, "sceNpMatching2DestroyContext", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2ContextStart(1, 0) == NotInitialized, "sceNpMatching2ContextStart", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2ContextStop(1) == NotInitialized, "sceNpMatching2ContextStop", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2RegisterContextCallback(nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterContextCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2RegisterRoomEventCallback(1, nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterRoomEventCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2RegisterRoomMessageCallback(1, nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterRoomMessageCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2RegisterSignalingCallback(1, nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterSignalingCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2SetDefaultRequestOptParam(1, nullptr) == NotInitialized, "sceNpMatching2SetDefaultRequestOptParam", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpMatching2CreateContextA(nullptr, nullptr) == NotInitialized, "sceNpMatching2CreateContextA", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpMatching2CreateContextA(&createA, &context) == NotInitialized && context == 77, "sceNpMatching2CreateContextA", "a call before initialization wrote a context id");
    Require(sceNpMatching2RegisterLobbyEventCallback(1, lobbyEvent, nullptr) == NotInitialized, "sceNpMatching2RegisterLobbyEventCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2RegisterLobbyMessageCallback(1, lobbyMessage, nullptr) == NotInitialized, "sceNpMatching2RegisterLobbyMessageCallback", "a call before initialization did not return NOT_INITIALIZED");

    init.size = 0x30;
    Require(sceNpMatching2Initialize(&init) == 0, "sceNpMatching2Initialize", "initialization failed");

    Require(sceNpMatching2CreateContext(nullptr, &context) == InvalidArgument, "sceNpMatching2CreateContext", "a null parameter was taken");
    create.size = 0x30;
    Require(sceNpMatching2CreateContext(&create, &context) == InvalidArgument, "sceNpMatching2CreateContext", "parameter size 0x30 was taken");
    create.size = 0x28;
    Require(sceNpMatching2CreateContext(&create, nullptr) == InvalidArgument, "sceNpMatching2CreateContext", "a null output was taken");
    Require(context == 77, "sceNpMatching2CreateContext", "a refused call wrote the context id");
    Require(sceNpMatching2CreateContextA(nullptr, &context) == InvalidArgument, "sceNpMatching2CreateContextA", "a null parameter was taken");
    for (const std::uint64_t size : {0x0ull, 0xFull, 0x11ull, 0x28ull, 0x100000010ull}) {
        createA.size = size;
        Require(sceNpMatching2CreateContextA(&createA, &context) == InvalidArgument, "sceNpMatching2CreateContextA", "a parameter size other than 0x10 was taken");
    }
    createA.size = 0x10;
    Require(sceNpMatching2CreateContextA(&createA, nullptr) == InvalidArgument, "sceNpMatching2CreateContextA", "a null output was taken");
    Require(context == 77, "sceNpMatching2CreateContextA", "a refused call wrote the context id");
    Require(sceNpMatching2CreateContext(&create, &context) == 0 && context == 1, "sceNpMatching2CreateContext", "the first context is not id 1");
    Require(sceNpMatching2DestroyContext(1) == 0, "sceNpMatching2DestroyContext", "destroying context 1 failed");
    std::uint16_t created = 0;
    for (std::uint16_t expected = 2; expected <= 10; ++expected) {
        Require(sceNpMatching2CreateContext(&create, &created) == 0 && created == expected, "sceNpMatching2CreateContext", "a context id was reused before the ids up to 10 were handed out");
    }
    Require(sceNpMatching2CreateContext(&create, &created) == 0 && created == 1, "sceNpMatching2CreateContext", "after id 10 the allocation did not wrap to the free id 1");
    created = 77;
    Require(sceNpMatching2CreateContext(&create, &created) == ContextMax && created == 77, "sceNpMatching2CreateContext", "an eleventh context did not return CONTEXT_MAX");
    Require(sceNpMatching2CreateContextA(&createA, &created) == ContextMax && created == 77, "sceNpMatching2CreateContextA", "the A form did not share the ten context ids");
    Require(sceNpMatching2DestroyContext(11) == InvalidContextId, "sceNpMatching2DestroyContext", "an unknown context was destroyed");

    Require(sceNpMatching2ContextStart(1, 0) == ResolverTimedOut, "sceNpMatching2ContextStart", "starting a context did not return the offline resolver timeout");
    Require(sceNpMatching2ContextStart(99, 0) == ResolverTimedOut, "sceNpMatching2ContextStart", "the offline answer did not come before the context check");
    Require(sceNpMatching2ContextStop(1) == ContextNotStarted, "sceNpMatching2ContextStop", "a context that never started did not return CONTEXT_NOT_STARTED");
    Require(sceNpMatching2ContextStop(99) == InvalidContextId, "sceNpMatching2ContextStop", "an unknown context was taken");

    Require(sceNpMatching2RegisterContextCallback(nullptr, nullptr) == 0, "sceNpMatching2RegisterContextCallback", "registering the context callback failed");
    Require(sceNpMatching2RegisterRoomEventCallback(99, nullptr, nullptr) == InvalidContextId, "sceNpMatching2RegisterRoomEventCallback", "an unknown context was taken");
    Require(sceNpMatching2RegisterRoomEventCallback(1, nullptr, nullptr) == 0, "sceNpMatching2RegisterRoomEventCallback", "registering failed");
    Require(sceNpMatching2RegisterRoomMessageCallback(99, nullptr, nullptr) == InvalidContextId, "sceNpMatching2RegisterRoomMessageCallback", "an unknown context was taken");
    Require(sceNpMatching2RegisterRoomMessageCallback(1, nullptr, nullptr) == 0, "sceNpMatching2RegisterRoomMessageCallback", "registering failed");
    Require(sceNpMatching2RegisterSignalingCallback(99, nullptr, nullptr) == InvalidContextId, "sceNpMatching2RegisterSignalingCallback", "an unknown context was taken");
    Require(sceNpMatching2RegisterSignalingCallback(1, nullptr, nullptr) == 0, "sceNpMatching2RegisterSignalingCallback", "registering failed");
    Require(sceNpMatching2SetDefaultRequestOptParam(99, nullptr) == InvalidArgument, "sceNpMatching2SetDefaultRequestOptParam", "a null option was not refused before the context");
    Require(sceNpMatching2SetDefaultRequestOptParam(99, requestOpt) == InvalidContextId, "sceNpMatching2SetDefaultRequestOptParam", "an unknown context was taken");
    Require(sceNpMatching2SetDefaultRequestOptParam(1, requestOpt) == 0, "sceNpMatching2SetDefaultRequestOptParam", "setting the default option failed");
    Require(sceNpMatching2RegisterLobbyEventCallback(99, lobbyEvent, &callbackArgument) == InvalidContextId, "sceNpMatching2RegisterLobbyEventCallback", "an unknown context was taken");
    Require(sceNpMatching2RegisterLobbyEventCallback(0, lobbyEvent, &callbackArgument) == InvalidContextId, "sceNpMatching2RegisterLobbyEventCallback", "context id 0 was taken");
    Require(sceNpMatching2RegisterLobbyEventCallback(1, nullptr, nullptr) == 0, "sceNpMatching2RegisterLobbyEventCallback", "a null callback was refused");
    Require(sceNpMatching2RegisterLobbyEventCallback(1, lobbyEvent, &callbackArgument) == 0, "sceNpMatching2RegisterLobbyEventCallback", "registering failed");
    Require(sceNpMatching2RegisterLobbyMessageCallback(99, lobbyMessage, &callbackArgument) == InvalidContextId, "sceNpMatching2RegisterLobbyMessageCallback", "an unknown context was taken");
    Require(sceNpMatching2RegisterLobbyMessageCallback(0, lobbyMessage, &callbackArgument) == InvalidContextId, "sceNpMatching2RegisterLobbyMessageCallback", "context id 0 was taken");
    Require(sceNpMatching2RegisterLobbyMessageCallback(1, nullptr, nullptr) == 0, "sceNpMatching2RegisterLobbyMessageCallback", "a null callback was refused");
    Require(sceNpMatching2RegisterLobbyMessageCallback(1, lobbyMessage, &callbackArgument) == 0, "sceNpMatching2RegisterLobbyMessageCallback", "registering failed");

    Require(sceNpMatching2DestroyContext(3) == 0, "sceNpMatching2DestroyContext", "destroying context 3 failed");
    Require(sceNpMatching2RegisterLobbyEventCallback(3, lobbyEvent, nullptr) == InvalidContextId, "sceNpMatching2RegisterLobbyEventCallback", "a destroyed context was taken");
    Require(sceNpMatching2RegisterLobbyMessageCallback(3, lobbyMessage, nullptr) == InvalidContextId, "sceNpMatching2RegisterLobbyMessageCallback", "a destroyed context was taken");
    Require(sceNpMatching2CreateContextA(&createA, &created) == 0 && created == 3, "sceNpMatching2CreateContextA", "the A form did not take the id freed in the shared pool");
    Require(sceNpMatching2CreateContext(&create, &created) == ContextMax && created == 3, "sceNpMatching2CreateContext", "an A form context did not count against the ten ids");
    Require(sceNpMatching2RegisterLobbyEventCallback(3, lobbyEvent, nullptr) == 0, "sceNpMatching2CreateContextA", "an A form context was not found by sceNpMatching2RegisterLobbyEventCallback");
    Require(sceNpMatching2RegisterRoomEventCallback(3, nullptr, nullptr) == 0, "sceNpMatching2CreateContextA", "an A form context was not found by sceNpMatching2RegisterRoomEventCallback");
    Require(sceNpMatching2SetDefaultRequestOptParam(3, requestOpt) == 0, "sceNpMatching2CreateContextA", "an A form context was not found by sceNpMatching2SetDefaultRequestOptParam");
    Require(sceNpMatching2ContextStop(3) == ContextNotStarted, "sceNpMatching2CreateContextA", "an A form context did not answer CONTEXT_NOT_STARTED");
    Require(sceNpMatching2DestroyContext(3) == 0, "sceNpMatching2CreateContextA", "an A form context was not destroyed");

    Require(sceNpMatching2Terminate() == 0, "sceNpMatching2Terminate", "termination failed");
    Require(sceNpMatching2DestroyContext(1) == NotInitialized, "sceNpMatching2DestroyContext", "a context was destroyed after termination");
    Require(sceNpMatching2CreateContextA(&createA, &created) == NotInitialized, "sceNpMatching2CreateContextA", "a context was created after termination");
    Require(sceNpMatching2RegisterLobbyEventCallback(1, lobbyEvent, nullptr) == NotInitialized, "sceNpMatching2RegisterLobbyEventCallback", "a call after termination did not return NOT_INITIALIZED");
    Require(sceNpMatching2RegisterLobbyMessageCallback(1, lobbyMessage, nullptr) == NotInitialized, "sceNpMatching2RegisterLobbyMessageCallback", "a call after termination did not return NOT_INITIALIZED");
    Require(sceNpMatching2Initialize(&init) == 0, "sceNpMatching2Initialize", "initializing again failed");
    Require(sceNpMatching2RegisterRoomEventCallback(5, nullptr, nullptr) == InvalidContextId, "sceNpMatching2Terminate", "a context created before termination is still found");
    Require(sceNpMatching2RegisterLobbyEventCallback(5, nullptr, nullptr) == InvalidContextId, "sceNpMatching2Terminate", "a context created before termination is still found by sceNpMatching2RegisterLobbyEventCallback");
    Require(sceNpMatching2RegisterLobbyMessageCallback(5, nullptr, nullptr) == InvalidContextId, "sceNpMatching2Terminate", "a context created before termination is still found by sceNpMatching2RegisterLobbyMessageCallback");
    Require(sceNpMatching2CreateContext(&create, &created) == 0 && created == 1, "sceNpMatching2Terminate", "termination did not restart the context ids at 1");
    createA.userId = 2;
    createA.serviceLabel = 1;
    Require(sceNpMatching2DestroyContext(1) == 0, "sceNpMatching2DestroyContext", "destroying context 1 failed");
    Require(sceNpMatching2CreateContextA(&createA, &created) == 0 && created == 2, "sceNpMatching2CreateContextA", "after id 1 of the NpId form the A form did not hand out id 2");
    Require(sceNpMatching2DestroyContext(2) == 0, "sceNpMatching2DestroyContext", "destroying context 2 failed");
    Require(sceNpMatching2CreateContext(&create, &created) == 0 && created == 3, "sceNpMatching2CreateContext", "after id 2 of the A form the NpId form did not hand out id 3");
    Require(sceNpMatching2Terminate() == 0, "sceNpMatching2Terminate", "the second termination failed");

    if (failures != 0) return 1;
    std::puts("NpMatching2 offline tests passed");
    return 0;
}
