#include "SceTypes.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "prx/libSceSystemService/libSceSystemService.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <stdexcept>
#include <string_view>

namespace {

constexpr auto LOGGED_IN_USER = static_cast<std::uint32_t>(USER_SERVICE_INITIAL_USER_ID);

int failures = 0;

template<typename TCall>
void Case(const char* name, TCall call) {
    try {
        if (call()) return;
        std::printf("%s: wrong answer\n", name);
    } catch (const std::exception& error) {
        std::printf("%s: %s\n", name, error.what());
    }
    ++failures;
}

void TestSuspend() {
    Case("sceLncUtilBlockAppSuspend", [] {
        return sceLncUtilBlockAppSuspend("saving") == SYSTEM_SERVICE_OK && sceLncUtilBlockAppSuspend("saving") == SYSTEM_SERVICE_OK;
    });
    Case("sceLncUtilUnblockAppSuspend", [] {
        return sceLncUtilUnblockAppSuspend("saving") == SYSTEM_SERVICE_OK && sceLncUtilUnblockAppSuspend("saving") == SYSTEM_SERVICE_OK;
    });
    Case("sceLncUtilUnblockAppSuspend with no block left", [] {
        return sceLncUtilUnblockAppSuspend("saving") == SCE_KERNEL_ERROR_EBUSY && sceLncUtilUnblockAppSuspend(nullptr) == SCE_KERNEL_ERROR_EBUSY &&
            sceLncUtilBlockAppSuspend("saving") == SYSTEM_SERVICE_OK && sceLncUtilUnblockAppSuspend("saving") == SYSTEM_SERVICE_OK &&
            sceLncUtilUnblockAppSuspend("saving") == SCE_KERNEL_ERROR_EBUSY;
    });
    Case("sceLncUtilEnableSuspendNotification", [] {
        return sceLncUtilEnableSuspendNotification() == SYSTEM_SERVICE_OK && sceLncUtilEnableSuspendNotification() == SYSTEM_SERVICE_OK;
    });
    Case("sceLncUtilDisableSuspendNotification", [] {
        return sceLncUtilDisableSuspendNotification() == SYSTEM_SERVICE_OK && sceLncUtilDisableSuspendNotification() == SYSTEM_SERVICE_OK;
    });
    Case("sceSystemServiceEnableSuspendNotification", [] {
        return sceSystemServiceEnableSuspendNotification() == sceLncUtilEnableSuspendNotification();
    });
    Case("sceSystemServiceDisableSuspendNotification", [] {
        return sceSystemServiceDisableSuspendNotification() == sceLncUtilDisableSuspendNotification();
    });
    Case("sceLncUtilTryBlockAppSuspend", [] {
        return sceLncUtilTryBlockAppSuspend("saving") == SYSTEM_SERVICE_OK && sceLncUtilTryBlockAppSuspend("saving") == SYSTEM_SERVICE_OK &&
            sceLncUtilUnblockAppSuspend("saving") == SYSTEM_SERVICE_OK && sceLncUtilUnblockAppSuspend("saving") == SYSTEM_SERVICE_OK &&
            sceLncUtilUnblockAppSuspend("saving") == SCE_KERNEL_ERROR_EBUSY;
    });
    Case("sceLncUtilTryBlockAppSuspend with a null reason", [] {
        try {
            sceLncUtilTryBlockAppSuspend(nullptr);
        } catch (const std::exception&) {
            return sceLncUtilUnblockAppSuspend("saving") == SCE_KERNEL_ERROR_EBUSY;
        }
        return false;
    });
}

template<typename TCall>
bool ReportsInactive(TCall call) {
    bool active = true;
    return call(&active) == SYSTEM_SERVICE_OK && !active;
}

template<typename TCall>
bool RejectsNullOutput(TCall call, int error) {
    return call(nullptr) == error;
}

template<typename TCall>
bool RejectsUnwritableOutput(TCall call, int error) {
    return call(reinterpret_cast<bool*>(std::uintptr_t{0x10})) == error;
}

void TestSessions() {
    Case("sceShellCoreUtilGetRemotePlayStatus", [] { return ReportsInactive(sceShellCoreUtilGetRemotePlayStatus); });
    Case("sceShellCoreUtilGetRemotePlayStatus null output", [] {
        return RejectsNullOutput(sceShellCoreUtilGetRemotePlayStatus, SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT);
    });
    Case("sceShellCoreUtilGetRemotePlayStatus unwritable output", [] {
        return RejectsUnwritableOutput(sceShellCoreUtilGetRemotePlayStatus, SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT);
    });
    Case("sceSystemServiceGetRemoteplayStatus", [] { return ReportsInactive(sceSystemServiceGetRemoteplayStatus); });
    Case("sceSystemServiceGetRemoteplayStatus null output", [] {
        return RejectsNullOutput(sceSystemServiceGetRemoteplayStatus, SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT);
    });
    Case("sceSystemServiceGetRemoteplayStatus unwritable output", [] {
        return RejectsUnwritableOutput(sceSystemServiceGetRemoteplayStatus, SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT);
    });
    Case("sceShellCoreUtilGetSharePlayStatus", [] { return ReportsInactive(sceShellCoreUtilGetSharePlayStatus); });
    Case("sceShellCoreUtilGetSharePlayStatus null output", [] {
        return RejectsNullOutput(sceShellCoreUtilGetSharePlayStatus, SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT);
    });
    Case("sceShellCoreUtilGetSharePlayStatus unwritable output", [] {
        return RejectsUnwritableOutput(sceShellCoreUtilGetSharePlayStatus, SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT);
    });
    Case("sceShellCoreUtilGetGameLiveStreamingStatus", [] { return ReportsInactive(sceShellCoreUtilGetGameLiveStreamingStatus); });
    Case("sceShellCoreUtilGetGameLiveStreamingStatus null output", [] {
        return RejectsNullOutput(sceShellCoreUtilGetGameLiveStreamingStatus, SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT);
    });
    Case("sceShellCoreUtilGetGameLiveStreamingStatus unwritable output", [] {
        return RejectsUnwritableOutput(sceShellCoreUtilGetGameLiveStreamingStatus, SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT);
    });
    Case("sceShellCoreUtilIsGameLiveStreamingOnAir", [] { return !sceShellCoreUtilIsGameLiveStreamingOnAir(); });
    Case("sceSystemServiceIsGameLiveStreamingOnAir", [] {
        return !sceSystemServiceIsGameLiveStreamingOnAir() && sceSystemServiceIsGameLiveStreamingOnAir() == sceShellCoreUtilIsGameLiveStreamingOnAir();
    });
}

template<typename TCall>
bool ReportsNotAdjusted(TCall call) {
    const auto user = LOGGED_IN_USER;
    std::uint8_t adjusted = 0xff;
    return call(user, &adjusted) == SYSTEM_SERVICE_OK && adjusted == 0;
}

void TestShell() {
    Case("sceLncUtilIsShellUiFgAndGameBgCpuMode", [] { return !sceLncUtilIsShellUiFgAndGameBgCpuMode(); });
    Case("sceSystemServiceIsShellUiFgAndGameBgCpuMode", [] {
        return !sceSystemServiceIsShellUiFgAndGameBgCpuMode() && sceSystemServiceIsShellUiFgAndGameBgCpuMode() == sceLncUtilIsShellUiFgAndGameBgCpuMode();
    });
    Case("sceShellCoreUtilIsImposeScreenOverlaid", [] { return !sceShellCoreUtilIsImposeScreenOverlaid(); });
    Case("sceShellCoreUtilIsScreenSaverOn", [] { return !sceShellCoreUtilIsScreenSaverOn(); });
    Case("sceSystemServiceIsScreenSaverOn", [] {
        return !sceSystemServiceIsScreenSaverOn() && sceSystemServiceIsScreenSaverOn() == sceShellCoreUtilIsScreenSaverOn();
    });
    Case("sceSystemStateMgrIsShellUIShutdownInProgress", [] { return !sceSystemStateMgrIsShellUIShutdownInProgress(); });
    Case("sceShellCoreUtilGetSocialScreenStatus", [] { return ReportsInactive(sceShellCoreUtilGetSocialScreenStatus); });
    Case("sceShellCoreUtilIsEyeToEyeDistanceAdjusted", [] { return ReportsNotAdjusted(sceShellCoreUtilIsEyeToEyeDistanceAdjusted); });
    Case("sceSystemServiceIsEyeToEyeDistanceAdjusted", [] {
        return ReportsNotAdjusted(sceSystemServiceIsEyeToEyeDistanceAdjusted) &&
            sceSystemServiceIsEyeToEyeDistanceAdjusted(LOGGED_IN_USER, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER;
    });
    Case("sceShellCoreUtilIsEyeToEyeDistanceAdjusted unwritable output", [] {
        errno = 73;
        try {
            sceShellCoreUtilIsEyeToEyeDistanceAdjusted(LOGGED_IN_USER, reinterpret_cast<std::uint8_t*>(0x10));
        } catch (const std::runtime_error&) {
            return errno == 73;
        }
        return false;
    });
    Case("sceSystemServiceIsEyeToEyeDistanceAdjusted unwritable output", [] {
        errno = 73;
        return sceSystemServiceIsEyeToEyeDistanceAdjusted(LOGGED_IN_USER, reinterpret_cast<std::uint8_t*>(0x10)) ==
            SYSTEM_SERVICE_ERROR_PARAMETER && errno == 73;
    });
}

template<typename TCall>
bool ReportsActive(TCall call) {
    bool active = false;
    return call(&active) == SYSTEM_SERVICE_OK && active;
}

template<typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

template<typename TSet, typename TGet>
bool StoresStatus(TSet set, TGet get) {
    if (!ReportsInactive(get) || set(1) != SYSTEM_SERVICE_OK || !ReportsActive(get) || set(1) != SYSTEM_SERVICE_OK || !ReportsActive(get)) return false;
    for (const std::int32_t value : {2, -1, 0x100, 0x7fffffff, static_cast<std::int32_t>(0x80000000u)}) {
        if (!Throws([&] { set(value); }) || !ReportsActive(get)) return false;
    }
    if (set(0) != SYSTEM_SERVICE_OK || !ReportsInactive(get)) return false;
    for (const std::int32_t value : {2, -1, 0x100}) {
        if (!Throws([&] { set(value); }) || !ReportsInactive(get)) return false;
    }
    return set(1) == SYSTEM_SERVICE_OK && ReportsActive(get);
}

using StatusGetter = decltype(&sceShellCoreUtilGetRemotePlayStatus);
using StatusSetter = decltype(&sceShellCoreUtilSetRemotePlayStatus);

bool OthersInactive(StatusGetter active) {
    const StatusGetter statuses[] = {sceShellCoreUtilGetRemotePlayStatus, sceShellCoreUtilGetSharePlayStatus, sceShellCoreUtilGetSocialScreenStatus};
    for (auto* status : statuses) {
        if (status != active && !ReportsInactive(status)) return false;
    }
    return ReportsInactive(sceShellCoreUtilGetGameLiveStreamingStatus) && !sceShellCoreUtilIsGameLiveStreamingOnAir();
}

bool ResetStatus(StatusSetter set) {
    return set(0) == SYSTEM_SERVICE_OK;
}

void TestStored() {
    Case("sceShellCoreUtilSetRemotePlayStatus", [] {
        return StoresStatus(sceShellCoreUtilSetRemotePlayStatus, sceShellCoreUtilGetRemotePlayStatus) && ReportsActive(sceSystemServiceGetRemoteplayStatus) &&
            OthersInactive(sceShellCoreUtilGetRemotePlayStatus) &&
            RejectsNullOutput(sceShellCoreUtilGetRemotePlayStatus, SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT) &&
            RejectsUnwritableOutput(sceSystemServiceGetRemoteplayStatus, SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT) &&
            ResetStatus(sceShellCoreUtilSetRemotePlayStatus) && ReportsInactive(sceSystemServiceGetRemoteplayStatus);
    });
    Case("sceShellCoreUtilSetSharePlayStatus", [] {
        return StoresStatus(sceShellCoreUtilSetSharePlayStatus, sceShellCoreUtilGetSharePlayStatus) && OthersInactive(sceShellCoreUtilGetSharePlayStatus) &&
            RejectsNullOutput(sceShellCoreUtilGetSharePlayStatus, SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT) &&
            ResetStatus(sceShellCoreUtilSetSharePlayStatus);
    });
    Case("sceShellCoreUtilSetSocialScreenStatus", [] {
        return StoresStatus(sceShellCoreUtilSetSocialScreenStatus, sceShellCoreUtilGetSocialScreenStatus) &&
            OthersInactive(sceShellCoreUtilGetSocialScreenStatus) && Throws([] { sceShellCoreUtilGetSocialScreenStatus(nullptr); }) &&
            ResetStatus(sceShellCoreUtilSetSocialScreenStatus) && OthersInactive(nullptr);
    });
    Case("sceShellCoreUtilSetUserFocus", [] {
        const int runningAppId = sceSystemServiceGetAppIdOfRunningBigApp();
        std::int32_t user = 0;
        if (sceShellCoreUtilSetUserFocus(USER_SERVICE_INITIAL_USER_ID) != SYSTEM_SERVICE_OK) return false;
        if (sceShellCoreUtilGetUserFocus(runningAppId, &user) != SYSTEM_SERVICE_OK || user != USER_SERVICE_INITIAL_USER_ID) return false;
        for (const std::int32_t other : {USER_SERVICE_INITIAL_USER_ID + 1, 0, -1, 0x7fffffff}) {
            if (!Throws([&] { sceShellCoreUtilSetUserFocus(other); })) return false;
        }
        user = 0;
        return sceShellCoreUtilGetUserFocus(runningAppId, &user) == SYSTEM_SERVICE_OK && user == USER_SERVICE_INITIAL_USER_ID;
    });
}

void TestDevices() {
    Case("sceSystemStateMgrIsBdDriveReady", [] { return !sceSystemStateMgrIsBdDriveReady(); });
    Case("sceShellCoreUtilIsUsbMassStorageMounted", [] {
        for (std::uint32_t device = 0; device < 8; ++device) {
            if (sceShellCoreUtilIsUsbMassStorageMounted(device)) return false;
        }
        return true;
    });
    Case("sceShellCoreUtilIsExternalStorageAppMoveInProgress", [] { return !sceShellCoreUtilIsExternalStorageAppMoveInProgress(); });
    Case("sceLncUtilIsPs2Emu", [] { return ReportsInactive(sceLncUtilIsPs2Emu); });
    Case("sceLncUtilIsPs2Emu null output", [] { return RejectsNullOutput(sceLncUtilIsPs2Emu, LNC_UTIL_ERROR_INVALID_ARGUMENT); });
    Case("sceLncUtilIsPs2Emu unwritable output", [] { return RejectsUnwritableOutput(sceLncUtilIsPs2Emu, LNC_UTIL_ERROR_INVALID_ARGUMENT); });
}

}

int main(int argc, char** argv) {
    const std::string_view mode = argc > 1 ? argv[1] : "";
    if (mode == "suspend") TestSuspend();
    else if (mode == "sessions") TestSessions();
    else if (mode == "shell") TestShell();
    else if (mode == "devices") TestDevices();
    else if (mode == "stored") TestStored();
    else return 2;
    if (failures == 0) std::printf("all cases passed\n");
    return failures == 0 ? 0 : 1;
}
