#include "prx/libSceIcu/DataMemory.hpp"
#include <unicode/udata.h>
#include "udatamem.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t Untouched = 0x5a5a5a5a;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "ICU DATA MEMORY: %s\n", message.c_str());
        std::abort();
    }
}

template<typename TAction>
bool Invalid(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template<typename TAction>
bool NotImplemented(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void Header(std::uint8_t* bytes, const char* format) {
    bytes[0] = 32;
    bytes[1] = 0;
    bytes[2] = 0xda;
    bytes[3] = 0x27;
    bytes[4] = 20;
    bytes[5] = 0;
    bytes[8] = 0;
    bytes[9] = 0;
    bytes[10] = 2;
    std::memcpy(bytes + 12, format, 4);
    bytes[16] = 1;
    bytes[20] = 7;
}

struct Package {
    std::vector<std::uint64_t> storage = std::vector<std::uint64_t>(32, 0);

    std::uint8_t* bytes() {
        return reinterpret_cast<std::uint8_t*>(storage.data());
    }
};

Package MakePackage(const char* entry) {
    Package package;
    std::uint8_t* bytes = package.bytes();
    Header(bytes, "CmnD");
    std::uint8_t* toc = bytes + 32;
    const std::uint32_t count = 1;
    const std::uint32_t nameOffset = 12;
    const std::uint32_t dataOffset = 48;
    std::memcpy(toc, &count, 4);
    std::memcpy(toc + 4, &nameOffset, 4);
    std::memcpy(toc + 8, &dataOffset, 4);
    std::strcpy(reinterpret_cast<char*>(toc + nameOffset), entry);
    Header(toc + dataOffset, "Helo");
    std::memcpy(toc + dataOffset + 32, "payload", 8);
    return package;
}

struct AcceptLog {
    int calls = 0;
    std::int8_t answer = 1;
    std::string type;
    std::string name;
    std::string format;
};

std::int8_t APS5_VABI GuestAcceptable(void* context, const char* type, const char* name, const GuestDataInfo* info) {
    auto* log = static_cast<AcceptLog*>(context);
    ++log->calls;
    log->type = type ? type : "(null)";
    log->name = name;
    log->format.assign(reinterpret_cast<const char*>(info->dataFormat), 4);
    return log->answer;
}

void TestIcuData() {
    std::int32_t status = Ok;
    UDataMemory* data = udata_open_nid_postfix(nullptr, "icu", "unames", &status);
    Require(data && status == Ok, "open unames");
    UErrorCode hostStatus = U_ZERO_ERROR;
    UDataMemory* host = udata_open(nullptr, "icu", "unames", &hostStatus);
    Require(host && U_SUCCESS(hostStatus), "host open unames");
    const auto* guestRaw = static_cast<const char*>(udata_getRawMemory_nid_postfix(data));
    const auto* hostRaw = static_cast<const char*>(udata_getRawMemory(host));
    Require(static_cast<const char*>(udata_getMemory_nid_postfix(data)) - guestRaw == static_cast<const char*>(udata_getMemory(host)) - hostRaw, "memory offset against ICU");
    Require(std::memcmp(guestRaw, hostRaw, 4096) == 0, "memory bytes against ICU");
    Require(udata_getLength_nid_postfix(data) == udata_getLength(host) && udata_getLength_nid_postfix(data) > 0, "length against ICU");
    const auto* raw = static_cast<const std::uint8_t*>(udata_getRawMemory_nid_postfix(data));
    Require(raw[2] == 0xda && raw[3] == 0x27, "raw memory magic");
    GuestDataInfo info{};
    info.size = sizeof(info);
    udata_getInfo_nid_postfix(data, &info);
    Require(info.size == 20 && std::memcmp(info.dataFormat, "unam", 4) == 0 && info.formatVersion[0] == 1 && info.sizeofUChar == 2 && info.isBigEndian == 0, "getInfo unames");
    GuestDataInfo small{};
    std::memset(&small, 0x77, sizeof(small));
    small.size = 8;
    udata_getInfo_nid_postfix(data, &small);
    Require(small.size == 8 && small.isBigEndian == 0 && small.sizeofUChar == 2 && small.dataFormat[0] == 0x77, "getInfo short size");
    GuestDataInfo two{};
    std::memset(&two, 0x77, sizeof(two));
    two.size = 2;
    udata_getInfo_nid_postfix(data, &two);
    Require(two.size == 2 && two.isBigEndian == 0x77, "getInfo size 2");
    GuestDataInfo one{};
    one.size = 1;
    Require(Invalid([&] { udata_getInfo_nid_postfix(data, &one); }), "getInfo size 1");
    one.size = 0;
    Require(Invalid([&] { udata_getInfo_nid_postfix(data, &one); }), "getInfo size 0");
    one.size = 1;
    udata_getInfo_nid_postfix(nullptr, &one);
    Require(one.size == 0, "getInfo NULL data");
    udata_getInfo_nid_postfix(data, nullptr);
    udata_close(host);
    udata_close_nid_postfix(data);

    status = Ok;
    data = udata_open_nid_postfix("ICUDATA-brkitr", "dict", "thaidict", &status);
    Require(data && status == Ok, "open brkitr tree");
    info.size = sizeof(info);
    udata_getInfo_nid_postfix(data, &info);
    Require(std::memcmp(info.dataFormat, "Dict", 4) == 0, "dictionary format");
    udata_close_nid_postfix(data);
    status = Ok;
    data = udata_open_nid_postfix(U_ICUDATA_NAME "-coll", "res", "root", &status);
    Require(data && status == Ok, "open icudt68l tree");
    udata_close_nid_postfix(data);
    status = Ok;
    data = udata_open_nid_postfix("ICUDATA", "cnv", "ibm-37_P100-1995", &status);
    Require(data && status == Ok, "open ICUDATA alias");
    udata_close_nid_postfix(data);

    status = Ok;
    hostStatus = U_ZERO_ERROR;
    Require(udata_open_nid_postfix(nullptr, "icu", "missing", &status) == nullptr && udata_open(nullptr, "icu", "missing", &hostStatus) == nullptr && status == hostStatus && status != Ok, "missing item against ICU");
    status = Ok;
    data = udata_open_nid_postfix(nullptr, "", "unames.icu", &status);
    Require(data && status == Ok, "empty type");
    udata_close_nid_postfix(data);

    AcceptLog accept;
    status = Ok;
    data = udata_openChoice_nid_postfix(nullptr, "icu", "unames", GuestAcceptable, &accept, &status);
    Require(data && status == Ok && accept.calls == 1 && accept.type == "icu" && accept.name == "unames" && accept.format == "unam", "openChoice accepts");
    udata_close_nid_postfix(data);
    accept.answer = 0;
    status = Ok;
    Require(udata_openChoice_nid_postfix(nullptr, "icu", "unames", GuestAcceptable, &accept, &status) == nullptr && status == U_INVALID_FORMAT_ERROR && accept.calls >= 2, "openChoice rejects");
}

void TestErrors() {
    std::int32_t status = Untouched;
    Require(udata_open_nid_postfix("a/b", nullptr, nullptr, &status) == nullptr && status == Untouched, "open failed status");
    Require(udata_openChoice_nid_postfix("a/b", nullptr, nullptr, nullptr, nullptr, &status) == nullptr && status == Untouched, "openChoice failed status");
    status = Ok;
    Require(udata_open_nid_postfix("a/b", nullptr, nullptr, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "open NULL name");
    status = Ok;
    Require(udata_open_nid_postfix("a/b", nullptr, "", &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "open empty name");
    status = Ok;
    AcceptLog accept;
    Require(udata_openChoice_nid_postfix("a/b", nullptr, "unames", nullptr, &accept, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "openChoice NULL acceptor");
    Require(Invalid([] { udata_open_nid_postfix(nullptr, "icu", "unames", nullptr); }), "open NULL status");
    Require(Invalid([] { udata_openChoice_nid_postfix(nullptr, "icu", "unames", GuestAcceptable, nullptr, nullptr); }), "openChoice NULL status");
    status = Ok;
    Require(Invalid([&] { udata_open_nid_postfix(nullptr, nullptr, "unames", &status); }), "open ICU data NULL type");
    Require(Invalid([&] { udata_openChoice_nid_postfix("ICUDATA-coll", nullptr, "root", GuestAcceptable, &accept, &status); }), "openChoice ICU data NULL type");
    Require(Invalid([&] { udata_open_nid_postfix("ICUDATA", nullptr, "unames", &status); }), "open ICUDATA alias NULL type");
    Require(Invalid([&] { udata_open_nid_postfix(U_ICUDATA_NAME "-coll", nullptr, "root", &status); }), "open icudt68l tree NULL type");
    Require(NotImplemented([&] { status = Ok; udata_open_nid_postfix("ICUDATA-a/b", "res", "root", &status); }), "open path with slash");
    Require(NotImplemented([&] { status = Ok; udata_open_nid_postfix("ICUDATA-a\\b", "res", "root", &status); }), "open path with backslash");
    Require(NotImplemented([&] { status = Ok; udata_open_nid_postfix("unregistered", "typ", "hello", &status); }), "open unregistered package");
    Require(NotImplemented([&] { status = Ok; udata_open_nid_postfix("", "typ", "hello", &status); }), "open empty package");
    Require(NotImplemented([&] { status = Ok; udata_openChoice_nid_postfix("unregistered", nullptr, "hello", GuestAcceptable, &accept, &status); }), "openChoice unregistered package");
    udata_close_nid_postfix(nullptr);
    Require(udata_getMemory_nid_postfix(nullptr) == nullptr && udata_getRawMemory_nid_postfix(nullptr) == nullptr && udata_getLength_nid_postfix(nullptr) == -1, "NULL data memory");
}

void TestAppData() {
    Package package = MakePackage("mypkg/hello.typ");
    std::int32_t status = Ok;
    udata_setAppData_nid_postfix("mypkg", package.bytes(), &status);
    Require(status == Ok, "setAppData");
    UDataMemory* data = udata_open_nid_postfix("mypkg", "typ", "hello", &status);
    Require(data && status == Ok, "open app data");
    Require(std::strcmp(static_cast<const char*>(udata_getMemory_nid_postfix(data)), "payload") == 0, "app data memory");
    Require(udata_getRawMemory_nid_postfix(data) == package.bytes() + 80 && udata_getLength_nid_postfix(data) == -1, "app data raw memory and length");
    GuestDataInfo info{};
    info.size = sizeof(info);
    udata_getInfo_nid_postfix(data, &info);
    Require(std::memcmp(info.dataFormat, "Helo", 4) == 0 && info.formatVersion[0] == 1 && info.reservedWord == 0, "app data info");
    udata_close_nid_postfix(data);
    Require(NotImplemented([&] { status = Ok; udata_open_nid_postfix("mypkg-tree", "typ", "hello", &status); }), "tree missing from an app package");
    AcceptLog accept;
    status = Ok;
    data = udata_openChoice_nid_postfix("mypkg", "typ", "hello", GuestAcceptable, &accept, &status);
    Require(data && status == Ok && accept.calls == 1 && accept.format == "Helo" && accept.type == "typ", "openChoice app data");
    udata_close_nid_postfix(data);
    accept.answer = 0;
    Require(NotImplemented([&] { status = Ok; udata_openChoice_nid_postfix("mypkg", "typ", "hello", GuestAcceptable, &accept, &status); }), "openChoice app data rejected");
    Require(NotImplemented([&] { status = Ok; udata_open_nid_postfix("mypkg", "typ", "missing", &status); }), "open missing app item");
    status = Ok;
    udata_setAppData_nid_postfix("mypkg", package.bytes(), &status);
    Require(status == U_USING_DEFAULT_WARNING, "setAppData again");

    status = Untouched;
    udata_setAppData_nid_postfix(nullptr, nullptr, &status);
    Require(status == Untouched, "setAppData failed status");
    udata_setAppData_nid_postfix(nullptr, nullptr, nullptr);
    status = Ok;
    udata_setAppData_nid_postfix("other", nullptr, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "setAppData NULL data");
    std::vector<std::uint64_t> bad(8, 0);
    status = Ok;
    udata_setAppData_nid_postfix("other", bad.data(), &status);
    Require(status == U_INVALID_FORMAT_ERROR, "setAppData invalid data");
    status = Ok;
    udata_setAppData_nid_postfix(nullptr, bad.data(), &status);
    Require(status == U_INVALID_FORMAT_ERROR, "setAppData NULL path invalid data");
    Package item = MakePackage("x/y.z");
    Header(item.bytes(), "Helo");
    status = Ok;
    udata_setAppData_nid_postfix("other", item.bytes(), &status);
    Require(status == U_INVALID_FORMAT_ERROR, "setAppData not a package");
    status = Ok;
    Require(Invalid([&] { udata_setAppData_nid_postfix(nullptr, package.bytes(), &status); }), "setAppData NULL path");
    Require(NotImplemented([&] { status = Ok; udata_setAppData_nid_postfix("dir/pkg", package.bytes(), &status); }), "setAppData path with slash");
    Require(NotImplemented([&] { status = Ok; udata_setAppData_nid_postfix(U_ICUDATA_NAME, package.bytes(), &status); }), "setAppData ICU data name");
}

}

int main() {
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    TestIcuData();
    TestErrors();
    TestAppData();
    std::puts("ICU DATA MEMORY TESTS PASSED");
    return 0;
}
