#include "prx/libSceIcu/Converter.hpp"
#include "prx/libSceIcu/ConverterCallbacks.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include <unicode/uclean.h>
#include <unicode/ucnv.h>
#include <unicode/ucnv_cb.h>
#include <unicode/ucnv_err.h>
#include <unicode/udata.h>
#include <unicode/ustring.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t Untouched = 0x5a5a5a5a;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "ICU CONVERTER: %s\n", message.c_str());
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

void SetDefault(const char* name) {
    u_cleanup_nid_postfix();
    u_cleanup();
    UErrorCode accessStatus = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &accessStatus);
    if (name) ucnv_setDefaultName_nid_postfix(name);
}

std::string Bytes(const char* data, std::int32_t length) {
    return std::string(data, static_cast<std::size_t>(length));
}

std::string Bytes(std::initializer_list<int> values) {
    std::string text;
    for (int value : values) text.push_back(static_cast<char>(value));
    return text;
}

std::u16string Units(std::initializer_list<char16_t> values) {
    return std::u16string(values);
}

UConverter* Open(const char* name) {
    std::int32_t status = Ok;
    UConverter* converter = ucnv_open_nid_postfix(name, &status);
    Require(converter && status <= Ok, std::string("ucnv_open ") + name);
    return converter;
}

std::string FromUnicode(UConverter* converter, const std::u16string& text) {
    std::int32_t status = Ok;
    char buffer[256];
    const std::int32_t length = ucnv_fromUChars_nid_postfix(converter, buffer, sizeof(buffer), text.data(), static_cast<std::int32_t>(text.size()), &status);
    Require(status <= Ok, "ucnv_fromUChars status " + std::to_string(status));
    return Bytes(buffer, length);
}

std::u16string ToUnicode(UConverter* converter, const std::string& bytes) {
    std::int32_t status = Ok;
    char16_t buffer[256];
    const std::int32_t length = ucnv_toUChars_nid_postfix(converter, buffer, 256, bytes.data(), static_cast<std::int32_t>(bytes.size()), &status);
    Require(status <= Ok, "ucnv_toUChars status " + std::to_string(status));
    return std::u16string(buffer, static_cast<std::size_t>(length));
}

void CheckKnownConversions() {
    struct Case {
        const char* name;
        std::u16string text;
        std::string bytes;
    };
    const Case cases[] = {
        {"UTF-8", Units({0x41, 0xe9, 0x20ac, 0xd800, 0xdc00}), Bytes({0x41, 0xc3, 0xa9, 0xe2, 0x82, 0xac, 0xf0, 0x90, 0x80, 0x80})},
        {"ISO-8859-1", Units({0x41, 0xe9, 0xff}), Bytes({0x41, 0xe9, 0xff})},
        {"Shift_JIS", Units({0x3042, 0x41, 0xff71}), Bytes({0x82, 0xa0, 0x41, 0xb1})},
        {"ibm-1047", Units({0x41, 0x61, 0x30}), Bytes({0xc1, 0x81, 0xf0})},
        {"UTF-16BE", Units({0x41, 0x3042}), Bytes({0x00, 0x41, 0x30, 0x42})},
        {"UTF-32LE", Units({0x41}), Bytes({0x41, 0x00, 0x00, 0x00})},
        {"windows-1252", Units({0x20ac, 0x41}), Bytes({0x80, 0x41})},
        {"EUC-KR", Units({0xac00}), Bytes({0xb0, 0xa1})},
        {"GB18030", Units({0x4e00, 0x80}), Bytes({0xd2, 0xbb, 0x81, 0x30, 0x81, 0x30})},
    };
    for (const Case& item : cases) {
        UConverter* converter = Open(item.name);
        Require(FromUnicode(converter, item.text) == item.bytes, std::string("fromUnicode ") + item.name);
        Require(ToUnicode(converter, item.bytes) == item.text, std::string("toUnicode ") + item.name);
        ucnv_close_nid_postfix(converter);
    }
    UConverter* ascii = Open("US-ASCII");
    Require(FromUnicode(ascii, Units({0x41, 0xe9, 0x42})) == Bytes({0x41, 0x1a, 0x42}), "US-ASCII substitutes 0x1a");
    Require(ToUnicode(ascii, Bytes({0x41, 0x80})) == Units({0x41, 0xfffd}), "US-ASCII illegal byte becomes U+FFFD");
    ucnv_close_nid_postfix(ascii);
    UConverter* utf8 = Open("UTF-8");
    Require(ToUnicode(utf8, Bytes({0x61, 0xff, 0x62})) == Units({0x61, 0xfffd, 0x62}), "UTF-8 illegal byte");
    ucnv_close_nid_postfix(utf8);
}

void CheckStreaming() {
    UConverter* converter = Open("UTF-8");
    const std::u16string text = Units({0x61, 0xd83d, 0xde00, 0x62});
    char buffer[16] = {};
    char* target = buffer;
    const char16_t* source = text.data();
    std::int32_t offsets[16] = {};
    std::int32_t status = Ok;
    ucnv_fromUnicode_nid_postfix(converter, &target, buffer + sizeof(buffer), &source, text.data() + 2, offsets, 0, &status);
    Require(status == Ok && target - buffer == 1 && source == text.data() + 2, "fromUnicode keeps a lead surrogate pending");
    Require(ucnv_fromUCountPending_nid_postfix(converter, &status) == 1, "one pending unit");
    ucnv_fromUnicode_nid_postfix(converter, &target, buffer + sizeof(buffer), &source, text.data() + text.size(), offsets + 1, 1, &status);
    Require(status == Ok && Bytes(buffer, static_cast<std::int32_t>(target - buffer)) == Bytes({0x61, 0xf0, 0x9f, 0x98, 0x80, 0x62}), "fromUnicode completes the pair");
    UErrorCode hostStatus = U_ZERO_ERROR;
    UConverter* host = ucnv_open("UTF-8", &hostStatus);
    char hostBuffer[16] = {};
    char* hostTarget = hostBuffer;
    const char16_t* hostSource = text.data();
    std::int32_t hostOffsets[16] = {};
    ucnv_fromUnicode(host, &hostTarget, hostBuffer + sizeof(hostBuffer), &hostSource, text.data() + 2, hostOffsets, 0, &hostStatus);
    ucnv_fromUnicode(host, &hostTarget, hostBuffer + sizeof(hostBuffer), &hostSource, text.data() + text.size(), hostOffsets + 1, 1, &hostStatus);
    ucnv_close(host);
    Require(std::memcmp(offsets, hostOffsets, sizeof(offsets)) == 0 && offsets[0] == 0 && offsets[5] == 1, "fromUnicode offsets");
    Require(ucnv_fromUCountPending_nid_postfix(converter, &status) == 0, "nothing pending");
    char small[2];
    target = small;
    source = text.data();
    ucnv_fromUnicode_nid_postfix(converter, &target, small + sizeof(small), &source, text.data() + text.size(), nullptr, 1, &status);
    Require(status == U_BUFFER_OVERFLOW_ERROR && target == small + 2, "fromUnicode overflow");
    ucnv_resetFromUnicode_nid_postfix(converter);
    status = Ok;
    const std::string bytes = Bytes({0x61, 0xe2, 0x82});
    char16_t units[8];
    char16_t* unitTarget = units;
    const char* byteSource = bytes.data();
    ucnv_toUnicode_nid_postfix(converter, &unitTarget, units + 8, &byteSource, bytes.data() + bytes.size(), nullptr, 0, &status);
    Require(status == Ok && unitTarget - units == 1, "toUnicode keeps a partial sequence");
    Require(ucnv_toUCountPending_nid_postfix(converter, &status) == 2, "two pending bytes");
    const std::string tail = Bytes({0xac});
    byteSource = tail.data();
    ucnv_toUnicode_nid_postfix(converter, &unitTarget, units + 8, &byteSource, tail.data() + 1, nullptr, 1, &status);
    Require(status == Ok && unitTarget - units == 2 && units[1] == 0x20ac, "toUnicode completes the euro sign");
    ucnv_toUnicode_nid_postfix(converter, &unitTarget, units + 8, &byteSource, tail.data() + 1, nullptr, 0, nullptr);
    std::int32_t failed = U_ILLEGAL_ARGUMENT_ERROR;
    ucnv_toUnicode_nid_postfix(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, &failed);
    Require(failed == U_ILLEGAL_ARGUMENT_ERROR, "toUnicode keeps a failed status");
    status = Ok;
    ucnv_toUnicode_nid_postfix(nullptr, &unitTarget, units + 8, &byteSource, tail.data(), nullptr, 0, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "toUnicode rejects a null converter");
    status = Ok;
    ucnv_fromUnicode_nid_postfix(converter, &target, small, &source, source - 1, nullptr, 1, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "fromUnicode rejects a limit before the source");
    status = Ok;
    const std::string four = Bytes({0xf0, 0x90, 0x80, 0x80, 0x41});
    byteSource = four.data();
    Require(ucnv_getNextUChar_nid_postfix(converter, &byteSource, four.data() + four.size(), &status) == 0x10000, "getNextUChar supplementary");
    Require(ucnv_getNextUChar_nid_postfix(converter, &byteSource, four.data() + four.size(), &status) == 0x41 && status == Ok, "getNextUChar ASCII");
    ucnv_getNextUChar_nid_postfix(converter, &byteSource, four.data() + four.size(), &status);
    Require(status == U_INDEX_OUTOFBOUNDS_ERROR, "getNextUChar at the end");
    status = Ok;
    Require(ucnv_getNextUChar_nid_postfix(converter, nullptr, nullptr, &status) == 0xffff && status == U_ILLEGAL_ARGUMENT_ERROR, "getNextUChar null source");
    ucnv_reset_nid_postfix(converter);
    ucnv_resetToUnicode_nid_postfix(nullptr);
    ucnv_close_nid_postfix(converter);
    ucnv_close_nid_postfix(nullptr);
}

void CheckPreflight() {
    UConverter* converter = Open("UTF-8");
    const std::u16string text = Units({0x3042, 0x3044, 0x3046});
    std::int32_t status = Ok;
    Require(ucnv_fromUChars_nid_postfix(converter, nullptr, 0, text.data(), 3, &status) == 9 && status == U_BUFFER_OVERFLOW_ERROR, "fromUChars preflight");
    status = Ok;
    char exact[9];
    Require(ucnv_fromUChars_nid_postfix(converter, exact, 9, text.data(), 3, &status) == 9 && status == U_STRING_NOT_TERMINATED_WARNING, "fromUChars exact fit");
    status = Ok;
    char16_t units[4] = {1, 1, 1, 1};
    const std::string bytes = Bytes({0xe3, 0x81, 0x82, 0});
    Require(ucnv_toUChars_nid_postfix(converter, units, 4, bytes.data(), -1, &status) == 1 && status == Ok && units[0] == 0x3042 && units[1] == 0, "toUChars NUL-terminated source");
    status = Ok;
    Require(ucnv_toUChars_nid_postfix(converter, nullptr, 1, bytes.data(), 3, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "toUChars null destination");
    status = Ok;
    Require(ucnv_fromUChars_nid_postfix(converter, exact, 9, text.data(), -2, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "fromUChars bad length");
    Require(ucnv_fromUChars_nid_postfix(converter, exact, 9, text.data(), 3, nullptr) == 0, "fromUChars null status");
    ucnv_close_nid_postfix(converter);
}

void CheckConvert() {
    const std::string latin = Bytes({0x41, 0xe9, 0});
    char target[16];
    std::int32_t status = Ok;
    std::int32_t length = ucnv_convert_nid_postfix("UTF-8", "ISO-8859-1", target, sizeof(target), latin.data(), -1, &status);
    Require(status == Ok && Bytes(target, length) == Bytes({0x41, 0xc3, 0xa9}), "ucnv_convert Latin-1 to UTF-8");
    status = Ok;
    length = ucnv_convert_nid_postfix("ibm-1047", "UTF-8", nullptr, 0, "AB", 2, &status);
    Require(length == 2 && status == U_BUFFER_OVERFLOW_ERROR, "ucnv_convert preflight");
    status = Ok;
    length = ucnv_convert_nid_postfix("UTF-8", "no-such-converter", target, sizeof(target), "AB", 2, &status);
    Require(length == 0 && status == U_FILE_ACCESS_ERROR, "ucnv_convert unknown source converter");
    UConverter* latin1 = Open("ISO-8859-1");
    status = Ok;
    length = ucnv_toAlgorithmic_nid_postfix(UCNV_UTF8, latin1, target, sizeof(target), latin.data(), 2, &status);
    Require(status == Ok && Bytes(target, length) == Bytes({0x41, 0xc3, 0xa9}), "toAlgorithmic");
    const std::string utf8 = Bytes({0xc3, 0xa9, 0x42});
    status = Ok;
    length = ucnv_fromAlgorithmic_nid_postfix(latin1, UCNV_UTF8, target, sizeof(target), utf8.data(), 3, &status);
    Require(status == Ok && Bytes(target, length) == Bytes({0xe9, 0x42}), "fromAlgorithmic");
    status = Ok;
    ucnv_fromAlgorithmic_nid_postfix(latin1, UCNV_SBCS, target, sizeof(target), utf8.data(), 3, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "fromAlgorithmic rejects a data-based type");
    UConverter* utf8Converter = Open("UTF-8");
    char16_t pivot[4];
    char16_t* pivotSource = pivot;
    char16_t* pivotTarget = pivot;
    char* out = target;
    const char* in = latin.data();
    status = Ok;
    ucnv_convertEx_nid_postfix(utf8Converter, latin1, &out, target + sizeof(target), &in, latin.data() + 2, pivot, &pivotSource, &pivotTarget, pivot + 4, 1, 1, &status);
    Require(status == Ok && Bytes(target, static_cast<std::int32_t>(out - target)) == Bytes({0x41, 0xc3, 0xa9}), "convertEx");
    status = Ok;
    ucnv_convertEx_nid_postfix(utf8Converter, latin1, &out, target + sizeof(target), &in, latin.data() + 2, nullptr, nullptr, nullptr, nullptr, 0, 0, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "convertEx streaming needs a pivot");
    out = target;
    in = latin.data();
    status = Ok;
    ucnv_convertEx_nid_postfix(utf8Converter, latin1, &out, target + sizeof(target), &in, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 1, &status);
    Require(status == Ok && Bytes(target, static_cast<std::int32_t>(out - target)) == Bytes({0x41, 0xc3, 0xa9}) && target[3] == 0, "convertEx flushing with its own pivot and a NUL-terminated source");
    ucnv_close_nid_postfix(utf8Converter);
    ucnv_close_nid_postfix(latin1);
}

void CheckProperties() {
    UConverter* sjis = Open("ibm-943_P15A-2003");
    UConverter* utf8 = Open("UTF8");
    std::int32_t status = Ok;
    Require(std::strcmp(ucnv_getName_nid_postfix(utf8, &status), "UTF-8") == 0, "UTF8 canonical name");
    Require(std::strcmp(ucnv_getName_nid_postfix(sjis, &status), "ibm-943_P15A-2003") == 0, "ibm-943 name");
    Require(ucnv_getCCSID_nid_postfix(sjis, &status) == 943 && ucnv_getPlatform_nid_postfix(sjis, &status) == UCNV_IBM, "ibm-943 CCSID and platform");
    Require(ucnv_getType_nid_postfix(utf8) == UCNV_UTF8 && ucnv_getType_nid_postfix(sjis) == UCNV_MBCS, "types");
    Require(ucnv_getMaxCharSize_nid_postfix(utf8) == 3 && ucnv_getMinCharSize_nid_postfix(utf8) == 1, "UTF-8 sizes");
    Require(ucnv_getMaxCharSize_nid_postfix(sjis) == 2, "ibm-943 max size");
    UConverter* ambiguous = Open("ibm-943_P130-1999");
    Require(ucnv_isAmbiguous_nid_postfix(ambiguous) == 1 && ucnv_isAmbiguous_nid_postfix(sjis) == 0 && ucnv_isAmbiguous_nid_postfix(utf8) == 0 && ucnv_isAmbiguous_nid_postfix(nullptr) == 0, "isAmbiguous");
    char16_t path[] = {u'a', 0xa5, u'b', 0xa5, 0};
    ucnv_fixFileSeparator_nid_postfix(ambiguous, path, 3);
    Require(path[1] == 0x5c && path[3] == 0xa5, "fixFileSeparator replaces the yen sign within the length");
    path[1] = 0xa5;
    ucnv_fixFileSeparator_nid_postfix(utf8, path, 4);
    ucnv_fixFileSeparator_nid_postfix(sjis, path, 4);
    ucnv_fixFileSeparator_nid_postfix(ambiguous, nullptr, 4);
    ucnv_close_nid_postfix(ambiguous);
    Require(path[1] == 0xa5, "fixFileSeparator keeps unambiguous converters");
    std::array<std::int8_t, 258> starters{};
    starters.fill(9);
    ucnv_getStarters_nid_postfix(sjis, starters.data() + 1, &status);
    Require(status == Ok && starters[1 + 0x81] == 1 && starters[1 + 0x41] == 0 && starters[1 + 0xe0] == 1 && starters[0] == 9 && starters[257] == 9, "getStarters writes 256 flags");
    ucnv_getStarters_nid_postfix(utf8, nullptr, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "getStarters without starters");
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_getStarters_nid_postfix(sjis, nullptr, &error); }), "getStarters null array");
    ucnv_getStarters_nid_postfix(sjis, nullptr, nullptr);
    status = Ok;
    Require(ucnv_isFixedWidth_nid_postfix(utf8, &status) == 0 && status == Ok, "UTF-8 is not fixed width");
    UConverter* utf32 = Open("UTF-32");
    Require(ucnv_isFixedWidth_nid_postfix(utf32, &status) == 1, "UTF-32 is fixed width");
    Require(ucnv_isFixedWidth_nid_postfix(nullptr, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR, "isFixedWidth null converter");
    Require(Invalid([&] { ucnv_isFixedWidth_nid_postfix(utf8, nullptr); }), "isFixedWidth null status");
    ucnv_close_nid_postfix(utf32);
    status = Ok;
    Require(ucnv_usesFallback_nid_postfix(sjis) == 0, "fallback off");
    ucnv_setFallback_nid_postfix(sjis, 1);
    Require(ucnv_usesFallback_nid_postfix(sjis) == 1, "fallback on");
    Require(Invalid([] { ucnv_setFallback_nid_postfix(nullptr, 1); }) && Invalid([] { ucnv_usesFallback_nid_postfix(nullptr); }), "fallback null converter");
    std::int32_t failed = U_INVALID_CHAR_FOUND;
    Require(ucnv_getName_nid_postfix(nullptr, &failed) == nullptr && ucnv_getCCSID_nid_postfix(nullptr, &failed) == -1 && ucnv_getPlatform_nid_postfix(nullptr, &failed) == UCNV_UNKNOWN, "failed status returns first");
    Require(Invalid([] { std::int32_t error = Ok; ucnv_getName_nid_postfix(nullptr, &error); }), "getName null converter");
    Require(Invalid([&] { ucnv_getName_nid_postfix(utf8, nullptr); }), "getName null status");
    Require(Invalid([] { ucnv_getType_nid_postfix(nullptr); }) && Invalid([] { ucnv_getMaxCharSize_nid_postfix(nullptr); }) && Invalid([] { ucnv_getMinCharSize_nid_postfix(nullptr); }), "null converter getters");
    UConverter* ebcdic = ucnv_openCCSID_nid_postfix(1047, UCNV_IBM, &status);
    Require(ebcdic && std::strcmp(ucnv_getName_nid_postfix(ebcdic, &status), "ibm-1047_P100-1995") == 0, "openCCSID");
    ucnv_close_nid_postfix(ebcdic);
    for (std::int32_t platform : {static_cast<std::int32_t>(UCNV_UNKNOWN), static_cast<std::int32_t>(UCNV_IBM), 7}) {
        for (std::int32_t codepage : {0, 37, 943, 1047, 5348, 1208, 99999, -1}) {
            status = Ok;
            UErrorCode hostCode = U_ZERO_ERROR;
            UConverter* guest = ucnv_openCCSID_nid_postfix(codepage, platform, &status);
            UConverter* host = ucnv_openCCSID(codepage, static_cast<UConverterPlatform>(platform), &hostCode);
            const std::string label = "openCCSID " + std::to_string(platform) + " " + std::to_string(codepage);
            Require(status == hostCode && (guest == nullptr) == (host == nullptr), label);
            if (guest) Require(std::strcmp(ucnv_getName_nid_postfix(guest, &status), ucnv_getName(host, &hostCode)) == 0, label + " name");
            ucnv_close_nid_postfix(guest);
            ucnv_close(host);
        }
    }
    status = Ok;
    Require(ucnv_openCCSID_nid_postfix(1047, UCNV_IBM, nullptr) == nullptr, "openCCSID null status");
    ucnv_close_nid_postfix(utf8);
    ucnv_close_nid_postfix(sjis);
}

void CheckSubstitution() {
    UConverter* latin1 = Open("ISO-8859-1");
    char chars[4] = {9, 9, 9, 9};
    std::int8_t length = 4;
    std::int32_t status = Ok;
    ucnv_getSubstChars_nid_postfix(latin1, chars, &length, &status);
    Require(status == Ok && length == 1 && chars[0] == 0x1a && chars[1] == 9, "default subst char");
    length = 0;
    ucnv_getSubstChars_nid_postfix(latin1, nullptr, &length, &status);
    Require(status == U_INDEX_OUTOFBOUNDS_ERROR, "getSubstChars short buffer");
    Require(Invalid([&] { std::int32_t error = Ok; std::int8_t size = 4; ucnv_getSubstChars_nid_postfix(latin1, nullptr, &size, &error); }), "getSubstChars null buffer");
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_getSubstChars_nid_postfix(latin1, chars, nullptr, &error); }), "getSubstChars null length");
    status = Ok;
    ucnv_setSubstChars_nid_postfix(latin1, "?", 1, &status);
    Require(status == Ok && FromUnicode(latin1, Units({0x41, 0x20ac})) == "A?", "setSubstChars");
    ucnv_setSubstChars_nid_postfix(latin1, "??", 2, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "setSubstChars too long");
    ucnv_setSubstChars_nid_postfix(latin1, nullptr, 2, &status);
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_setSubstChars_nid_postfix(latin1, nullptr, 1, &error); }), "setSubstChars null bytes");
    Require(Invalid([&] { ucnv_setSubstChars_nid_postfix(latin1, "?", 1, nullptr); }), "setSubstChars null status");
    std::int32_t failed = U_INVALID_CHAR_FOUND;
    ucnv_setSubstChars_nid_postfix(nullptr, nullptr, 1, &failed);
    status = Ok;
    const std::u16string replacement = Units({0x3c, 0x78, 0x3e});
    ucnv_setSubstString_nid_postfix(latin1, replacement.data(), 3, &status);
    Require(status == Ok && FromUnicode(latin1, Units({0x41, 0x20ac})) == "A<x>", "setSubstString");
    length = 4;
    ucnv_getSubstChars_nid_postfix(latin1, chars, &length, &status);
    Require(status == Ok && length == 3 && std::memcmp(chars, "<x>", 3) == 0, "getSubstChars after setSubstString");
    const std::u16string euro = Units({0x20ac});
    ucnv_setSubstString_nid_postfix(latin1, euro.data(), 1, &status);
    Require(status == U_INVALID_CHAR_FOUND, "setSubstString unmappable");
    Require(Invalid([&] { ucnv_setSubstString_nid_postfix(latin1, euro.data(), 1, nullptr); }), "setSubstString null status");
    status = Ok;
    ucnv_setSubstString_nid_postfix(nullptr, euro.data(), 1, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "setSubstString null converter");
    UConverter* ascii = Open("US-ASCII");
    UErrorCode hostStatus = U_ZERO_ERROR;
    UConverter* host = ucnv_open("US-ASCII", &hostStatus);
    const std::string bytes = Bytes({0x41, 0x80, 0x42});
    char16_t units[8];
    char16_t* unitTarget = units;
    const char* byteSource = bytes.data();
    status = Ok;
    ucnv_toUnicode_nid_postfix(ascii, &unitTarget, units + 8, &byteSource, bytes.data() + 3, nullptr, 1, &status);
    unitTarget = units;
    byteSource = bytes.data();
    ucnv_toUnicode(host, &unitTarget, units + 8, &byteSource, bytes.data() + 3, nullptr, 1, &hostStatus);
    std::int8_t invalidLength = 4;
    std::int8_t hostLength = 4;
    char invalid[4] = {};
    char hostInvalid[4] = {};
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    ucnv_getInvalidChars_nid_postfix(ascii, invalid, &invalidLength, &status);
    ucnv_getInvalidChars(host, hostInvalid, &hostLength, &hostStatus);
    Require(status == hostStatus && invalidLength == hostLength && std::memcmp(invalid, hostInvalid, 4) == 0, "getInvalidChars");
    invalidLength = -1;
    ucnv_getInvalidChars_nid_postfix(ascii, invalid, &invalidLength, &status);
    Require(status == (hostLength > -1 ? U_INDEX_OUTOFBOUNDS_ERROR : Ok), "getInvalidChars short");
    status = Ok;
    ucnv_getInvalidChars_nid_postfix(ascii, nullptr, &invalidLength, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "getInvalidChars null buffer");
    ucnv_getInvalidChars_nid_postfix(ascii, nullptr, &invalidLength, nullptr);
    const std::u16string text = Units({0x41, 0xe9, 0x42});
    char out[8];
    char* outTarget = out;
    const char16_t* unitSource = text.data();
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    ucnv_fromUnicode_nid_postfix(ascii, &outTarget, out + 8, &unitSource, text.data() + 3, nullptr, 1, &status);
    outTarget = out;
    unitSource = text.data();
    ucnv_fromUnicode(host, &outTarget, out + 8, &unitSource, text.data() + 3, nullptr, 1, &hostStatus);
    char16_t hostUnits[4] = {};
    char16_t guestUnits[4] = {};
    invalidLength = 4;
    hostLength = 4;
    status = Ok;
    hostStatus = U_ZERO_ERROR;
    ucnv_getInvalidUChars_nid_postfix(ascii, guestUnits, &invalidLength, &status);
    ucnv_getInvalidUChars(host, hostUnits, &hostLength, &hostStatus);
    Require(status == hostStatus && invalidLength == hostLength && std::memcmp(guestUnits, hostUnits, sizeof(hostUnits)) == 0, "getInvalidUChars");
    status = Ok;
    ucnv_getInvalidUChars_nid_postfix(nullptr, guestUnits, &invalidLength, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "getInvalidUChars null converter");
    ucnv_close(host);
    ucnv_close_nid_postfix(ascii);
    ucnv_close_nid_postfix(latin1);
}

void CheckOpenErrors() {
    std::int32_t status = Ok;
    Require(ucnv_open_nid_postfix("no-such-converter", &status) == nullptr && status == U_FILE_ACCESS_ERROR, "unknown converter");
    status = U_INVALID_CHAR_FOUND;
    Require(ucnv_open_nid_postfix(nullptr, &status) == nullptr && status == U_INVALID_CHAR_FOUND, "failed status does not need the default");
    Require(ucnv_open_nid_postfix(nullptr, nullptr) == nullptr && ucnv_open_nid_postfix("UTF-8", nullptr) == nullptr, "null status");
    Require(NotImplemented([] { std::int32_t error = Ok; ucnv_open_nid_postfix(nullptr, &error); }), "default converter without a guest default");
    Require(NotImplemented([] { std::int32_t error = Ok; ucnv_createConverter_nid_postfix(nullptr, nullptr, &error); }), "createConverter default");
    std::array<std::uint64_t, 128> fill{};
    Require(NotImplemented([&] { std::int32_t error = Ok; ucnv_createConverter_nid_postfix(reinterpret_cast<UConverter*>(fill.data()), "UTF-8", &error); }), "createConverter into caller memory");
    Require(Invalid([] { ucnv_createConverter_nid_postfix(nullptr, "UTF-8", nullptr); }), "createConverter null status");
    status = U_INVALID_CHAR_FOUND;
    Require(ucnv_createConverter_nid_postfix(reinterpret_cast<UConverter*>(fill.data()), nullptr, &status) == nullptr, "createConverter failed status");
    status = Ok;
    UConverter* created = ucnv_createConverter_nid_postfix(nullptr, "ibm-1047", &status);
    Require(created && std::strcmp(ucnv_getName_nid_postfix(created, &status), "ibm-1047_P100-1995") == 0, "createConverter by name");
    ucnv_close_nid_postfix(created);
    const std::u16string name = Units({u'U', u'T', u'F', u'-', u'8'});
    Require(NotImplemented([&] { std::int32_t error = Ok; ucnv_openU_nid_postfix(name.c_str(), &error); }), "openU converts the name with the default converter");
    Require(NotImplemented([] { std::int32_t error = Ok; ucnv_openU_nid_postfix(nullptr, &error); }), "openU default");
    std::u16string longName(60, u'a');
    status = Ok;
    Require(ucnv_openU_nid_postfix(longName.c_str(), &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "openU long name needs no default");
    Require(ucnv_openU_nid_postfix(name.c_str(), nullptr) == nullptr, "openU null status");
    std::int32_t error = Ok;
    Require(NotImplemented([] { std::int32_t inner = Ok; char target[4]; ucnv_convert_nid_postfix(nullptr, "ISO-8859-1", target, 4, "A", 1, &inner); }), "convert default target");
    Require(NotImplemented([] { std::int32_t inner = Ok; char target[4]; ucnv_convert_nid_postfix("UTF-8", nullptr, target, 4, "A", 1, &inner); }), "convert default source");
    char target[4];
    Require(ucnv_convert_nid_postfix(nullptr, "no-such-converter", target, 4, "A", 1, &error) == 0 && error == U_FILE_ACCESS_ERROR, "convert fails on the source first");
    error = Ok;
    Require(ucnv_convert_nid_postfix(nullptr, nullptr, target, 4, "", -1, &error) == 0 && error == Ok, "convert of empty input");
    error = Ok;
    Require(ucnv_convert_nid_postfix(nullptr, nullptr, target, -1, "A", 1, &error) == 0 && error == U_ILLEGAL_ARGUMENT_ERROR, "convert argument errors first");
    Require(NotImplemented([] { std::int32_t inner = Ok; ucnv_openPackage_nid_postfix("mypackage", "conv", &inner); }), "openPackage outside ICU data");
    Require(NotImplemented([] { std::int32_t inner = Ok; ucnv_openPackage_nid_postfix("ICUDATA\\x", "conv", &inner); }), "openPackage with a backslash");
    Require(Invalid([] { std::int32_t inner = Ok; ucnv_openPackage_nid_postfix(nullptr, nullptr, &inner); }), "openPackage null name");
    Require(Invalid([] { ucnv_openPackage_nid_postfix(nullptr, "UTF-8", nullptr); }), "openPackage null status");
    status = U_INVALID_CHAR_FOUND;
    Require(ucnv_openPackage_nid_postfix("mypackage", nullptr, &status) == nullptr, "openPackage failed status");
    for (const char* package : {static_cast<const char*>(nullptr), "ICUDATA", "icudt68l-brkitr"}) {
        for (const char* item : {"ibm-943_P15A-2003", "UTF-8", "ibm-1047_P100-1995,swaplfnl", "missing"}) {
            std::int32_t guestStatus = Ok;
            UConverter* guest = ucnv_openPackage_nid_postfix(package, item, &guestStatus);
            UErrorCode hostStatus = U_ZERO_ERROR;
            UConverter* host = ucnv_openPackage(package, item, &hostStatus);
            const std::string label = std::string("openPackage ") + (package ? package : "NULL") + " " + item;
            Require(guestStatus == hostStatus && (guest == nullptr) == (host == nullptr), label);
            if (guest) {
                std::int32_t nameStatus = Ok;
                Require(std::strcmp(ucnv_getName_nid_postfix(guest, &nameStatus), ucnv_getName(host, &hostStatus)) == 0, label + " name");
            }
            ucnv_close_nid_postfix(guest);
            ucnv_close(host);
        }
    }
}

void CheckDefault() {
    SetDefault("ISO-8859-1");
    std::int32_t status = Ok;
    UConverter* converter = ucnv_open_nid_postfix(nullptr, &status);
    Require(converter && std::strcmp(ucnv_getName_nid_postfix(converter, &status), "ISO-8859-1") == 0, "default converter after setDefaultName");
    ucnv_close_nid_postfix(converter);
    const std::u16string name = Units({u'i', u'b', u'm', u'-', u'1', u'0', u'4', u'7'});
    converter = ucnv_openU_nid_postfix(name.c_str(), &status);
    Require(converter && std::strcmp(ucnv_getName_nid_postfix(converter, &status), "ibm-1047_P100-1995") == 0, "openU with a guest default");
    ucnv_close_nid_postfix(converter);
    char target[4];
    Require(ucnv_convert_nid_postfix(nullptr, "UTF-8", target, 4, "A", 1, &status) == 1 && target[0] == 'A', "convert to the default");
    SetDefault("UTF-8");
    std::u16string wide(30, char16_t{0x4e00});
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_openU_nid_postfix(wide.c_str(), &error); }), "openU name longer than ICU's buffer in the default charset");
    std::u16string exact(20, char16_t{0x4e00});
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_openU_nid_postfix(exact.c_str(), &error); }), "openU name of 60 bytes in the default charset");
    std::u16string fits(19, char16_t{0x4e00});
    fits += u"ab";
    status = Ok;
    Require(ucnv_openU_nid_postfix(fits.c_str(), &status) == nullptr && status == U_FILE_ACCESS_ERROR, "openU name of 59 bytes in the default charset");
    std::u16string ascii(59, u'a');
    status = Ok;
    Require(ucnv_openU_nid_postfix(ascii.c_str(), &status) == nullptr && status == U_FILE_ACCESS_ERROR, "openU 59 unit name");
    SetDefault(nullptr);
    Require(NotImplemented([] { std::int32_t error = Ok; ucnv_open_nid_postfix(nullptr, &error); }), "u_cleanup forgets the default");
}

void CheckSweep() {
    UErrorCode hostStatus = U_ZERO_ERROR;
    const std::int32_t count = ucnv_countAvailable();
    Require(count > 100, "available converters");
    std::u16string text;
    for (char16_t unit : {0x41, 0x7e, 0xa0, 0xe9, 0x3b1, 0x430, 0x5d0, 0x627, 0xe01, 0x3042, 0x30a2, 0x4e00, 0xac00, 0xff21, 0x20ac, 0x2122}) text.push_back(unit);
    text.push_back(0xd835);
    text.push_back(0xdc00);
    std::string bytes;
    for (int value = 1; value < 256; value += 3) bytes.push_back(static_cast<char>(value));
    for (std::int32_t index = 0; index < count; ++index) {
        const char* name = ucnv_getAvailableName(index);
        std::int32_t status = Ok;
        UConverter* guest = ucnv_open_nid_postfix(name, &status);
        hostStatus = U_ZERO_ERROR;
        UConverter* host = ucnv_open(name, &hostStatus);
        Require(status == hostStatus && guest && host, std::string("open ") + name);
        Require(std::strcmp(ucnv_getName_nid_postfix(guest, &status), ucnv_getName(host, &hostStatus)) == 0, std::string("name ") + name);
        Require(ucnv_getType_nid_postfix(guest) == ucnv_getType(host) && ucnv_getMaxCharSize_nid_postfix(guest) == ucnv_getMaxCharSize(host) && ucnv_getMinCharSize_nid_postfix(guest) == ucnv_getMinCharSize(host), std::string("sizes ") + name);
        Require(ucnv_getCCSID_nid_postfix(guest, &status) == ucnv_getCCSID(host, &hostStatus) && ucnv_getPlatform_nid_postfix(guest, &status) == ucnv_getPlatform(host, &hostStatus), std::string("ccsid ") + name);
        Require(ucnv_isFixedWidth_nid_postfix(guest, &status) == ucnv_isFixedWidth(host, &hostStatus) && ucnv_isAmbiguous_nid_postfix(guest) == ucnv_isAmbiguous(host), std::string("width ") + name);
        char guestBytes[256];
        char hostBytes[256];
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        const std::int32_t guestLength = ucnv_fromUChars_nid_postfix(guest, guestBytes, sizeof(guestBytes), text.data(), static_cast<std::int32_t>(text.size()), &status);
        const std::int32_t hostLength = ucnv_fromUChars(host, hostBytes, sizeof(hostBytes), text.data(), static_cast<std::int32_t>(text.size()), &hostStatus);
        Require(status == hostStatus && guestLength == hostLength && std::memcmp(guestBytes, hostBytes, static_cast<std::size_t>(guestLength)) == 0, std::string("fromUChars ") + name);
        char16_t guestUnits[512];
        char16_t hostUnits[512];
        status = Ok;
        hostStatus = U_ZERO_ERROR;
        const std::int32_t guestUnitLength = ucnv_toUChars_nid_postfix(guest, guestUnits, 512, bytes.data(), static_cast<std::int32_t>(bytes.size()), &status);
        const std::int32_t hostUnitLength = ucnv_toUChars(host, hostUnits, 512, bytes.data(), static_cast<std::int32_t>(bytes.size()), &hostStatus);
        Require(status == hostStatus && guestUnitLength == hostUnitLength && std::memcmp(guestUnits, hostUnits, sizeof(char16_t) * static_cast<std::size_t>(guestUnitLength)) == 0, std::string("toUChars ") + name);
        ucnv_close_nid_postfix(guest);
        ucnv_close(host);
    }
}

}

#if defined(_WIN32)
extern "C" {
extern void* __imp_UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix;
extern void* __imp_UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix;
extern void* __imp_UCNV_FROM_U_CALLBACK_STOP_nid_postfix;
extern void* __imp_UCNV_FROM_U_CALLBACK_SKIP_nid_postfix;
extern void* __imp_UCNV_TO_U_CALLBACK_SUBSTITUTE_nid_postfix;
extern void* __imp_UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix;
extern void* __imp_UCNV_TO_U_CALLBACK_STOP_nid_postfix;
extern void* __imp_UCNV_TO_U_CALLBACK_SKIP_nid_postfix;
}
#endif

namespace {

template<typename TCallback>
TCallback Library(TCallback local, void* imported) {
#if defined(_WIN32)
    (void)local;
    return reinterpret_cast<TCallback>(imported);
#else
    (void)imported;
    return local;
#endif
}

#if defined(_WIN32)
#define APS5_TEST_IMPORT(name) __imp_##name
#else
#define APS5_TEST_IMPORT(name) nullptr
#endif

Icu::ConverterFromUCallback FromSubstitute() {
    return Library<Icu::ConverterFromUCallback>(UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix, APS5_TEST_IMPORT(UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix));
}

Icu::ConverterFromUCallback FromEscape() {
    return Library<Icu::ConverterFromUCallback>(UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix, APS5_TEST_IMPORT(UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix));
}

Icu::ConverterFromUCallback FromStop() {
    return Library<Icu::ConverterFromUCallback>(UCNV_FROM_U_CALLBACK_STOP_nid_postfix, APS5_TEST_IMPORT(UCNV_FROM_U_CALLBACK_STOP_nid_postfix));
}

Icu::ConverterFromUCallback FromSkip() {
    return Library<Icu::ConverterFromUCallback>(UCNV_FROM_U_CALLBACK_SKIP_nid_postfix, APS5_TEST_IMPORT(UCNV_FROM_U_CALLBACK_SKIP_nid_postfix));
}

Icu::ConverterToUCallback ToSubstitute() {
    return Library<Icu::ConverterToUCallback>(UCNV_TO_U_CALLBACK_SUBSTITUTE_nid_postfix, APS5_TEST_IMPORT(UCNV_TO_U_CALLBACK_SUBSTITUTE_nid_postfix));
}

Icu::ConverterToUCallback ToEscape() {
    return Library<Icu::ConverterToUCallback>(UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix, APS5_TEST_IMPORT(UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix));
}

Icu::ConverterToUCallback ToStop() {
    return Library<Icu::ConverterToUCallback>(UCNV_TO_U_CALLBACK_STOP_nid_postfix, APS5_TEST_IMPORT(UCNV_TO_U_CALLBACK_STOP_nid_postfix));
}

Icu::ConverterToUCallback ToSkip() {
    return Library<Icu::ConverterToUCallback>(UCNV_TO_U_CALLBACK_SKIP_nid_postfix, APS5_TEST_IMPORT(UCNV_TO_U_CALLBACK_SKIP_nid_postfix));
}

struct Call {
    std::int32_t reason;
    UConverter* converter;
    std::int32_t length;
    std::int32_t codePoint;
    std::int32_t firstUnit;
    std::uint16_t size;
};

struct Recorder {
    std::vector<Call> calls;
    std::string replacement;
    bool checkOnClose = false;
};

void APS5_VABI RecordFromUnicode_nid_no_patch(const void* context, Icu::ConverterFromUnicodeArgs* arguments, const char16_t* codeUnits, std::int32_t length, std::int32_t codePoint, std::int32_t reason, std::int32_t* status) {
    auto* recorder = static_cast<Recorder*>(const_cast<void*>(context));
    auto* args = reinterpret_cast<UConverterFromUnicodeArgs*>(arguments);
    recorder->calls.push_back({reason, args->converter, length, codePoint, length > 0 ? codeUnits[0] : -1, args->size});
    if (reason == UCNV_CLOSE && recorder->checkOnClose) {
        Icu::ConverterFromUCallback action = nullptr;
        const void* current = nullptr;
        ucnv_getFromUCallBack_nid_postfix(args->converter, &action, &current);
        Require(action == RecordFromUnicode_nid_no_patch && current == context, "the record is alive while ICU closes the converter");
    }
    if (reason <= UCNV_IRREGULAR) {
        *status = U_ZERO_ERROR;
        ucnv_cbFromUWriteBytes_nid_postfix(arguments, recorder->replacement.data(), static_cast<std::int32_t>(recorder->replacement.size()), 0, status);
    }
}

void APS5_VABI RecordToUnicode_nid_no_patch(const void* context, Icu::ConverterToUnicodeArgs* arguments, const char* codeUnits, std::int32_t length, std::int32_t reason, std::int32_t* status) {
    auto* recorder = static_cast<Recorder*>(const_cast<void*>(context));
    auto* args = reinterpret_cast<UConverterToUnicodeArgs*>(arguments);
    recorder->calls.push_back({reason, args->converter, length, 0, length > 0 ? static_cast<std::uint8_t>(codeUnits[0]) : -1, args->size});
    if (reason <= UCNV_IRREGULAR) {
        *status = U_ZERO_ERROR;
        const char16_t marks[] = {0x2329, 0x232a};
        ucnv_cbToUWriteUChars_nid_postfix(arguments, marks, 2, 0, status);
    }
}

Recorder* clonedRecorder = nullptr;

void APS5_VABI CloneFromUnicode_nid_no_patch(const void* context, Icu::ConverterFromUnicodeArgs* arguments, const char16_t*, std::int32_t, std::int32_t, std::int32_t reason, std::int32_t* status) {
    auto* args = reinterpret_cast<UConverterFromUnicodeArgs*>(arguments);
    if (reason == UCNV_CLONE) {
        Icu::ConverterFromUCallback oldAction = nullptr;
        const void* oldContext = nullptr;
        std::int32_t inner = Ok;
        ucnv_setFromUCallBack_nid_postfix(args->converter, RecordFromUnicode_nid_no_patch, clonedRecorder, &oldAction, &oldContext, &inner);
        Require(inner == Ok && oldAction == CloneFromUnicode_nid_no_patch && oldContext == context, "a clone callback sees the copied guest callback");
    }
    if (reason <= UCNV_IRREGULAR) *status = U_ZERO_ERROR;
}

std::string Convert(UConverter* converter, const std::u16string& text, std::int32_t* status) {
    char buffer[256];
    const std::int32_t length = ucnv_fromUChars_nid_postfix(converter, buffer, sizeof(buffer), text.data(), static_cast<std::int32_t>(text.size()), status);
    return Bytes(buffer, length);
}

std::u16string ConvertBack(UConverter* converter, const std::string& bytes, std::int32_t* status) {
    char16_t buffer[256];
    const std::int32_t length = ucnv_toUChars_nid_postfix(converter, buffer, 256, bytes.data(), static_cast<std::int32_t>(bytes.size()), status);
    return std::u16string(buffer, static_cast<std::size_t>(length));
}

const void* HostFromContext(const UConverter* converter) {
    UConverterFromUCallback action = nullptr;
    const void* context = nullptr;
    ucnv_getFromUCallBack(converter, &action, &context);
    return context;
}

const void* HostToContext(const UConverter* converter) {
    UConverterToUCallback action = nullptr;
    const void* context = nullptr;
    ucnv_getToUCallBack(converter, &action, &context);
    return context;
}

void CheckDefaultCallbacks() {
    UConverter* converter = Open("US-ASCII");
    Icu::ConverterFromUCallback fromAction = nullptr;
    const void* fromContext = &fromAction;
    ucnv_getFromUCallBack_nid_postfix(converter, &fromAction, &fromContext);
    Require(fromAction == FromSubstitute() && fromContext == nullptr, "the default from-Unicode callback is the SUBSTITUTE export");
    Icu::ConverterToUCallback toAction = nullptr;
    const void* toContext = &toAction;
    ucnv_getToUCallBack_nid_postfix(converter, &toAction, &toContext);
    Require(toAction == ToSubstitute() && toContext == nullptr, "the default to-Unicode callback is the SUBSTITUTE export");
    Require(Invalid([&] { ucnv_getFromUCallBack_nid_postfix(converter, nullptr, &fromContext); }) && Invalid([&] { ucnv_getToUCallBack_nid_postfix(converter, &toAction, nullptr); }), "get with null out-params");
    Require(Invalid([&] { ucnv_getFromUCallBack_nid_postfix(nullptr, &fromAction, &fromContext); }), "get with a null converter");
    ucnv_close_nid_postfix(converter);
    UConverter* iso2022 = Open("ISO-2022-JP");
    const std::string jis = Bytes({0x1b, 0x24, 0x42, 0x30, 0x21, 0x1b, 0x28, 0x42, 0x41});
    std::int32_t status = Ok;
    Require(ConvertBack(iso2022, jis, &status) == Units({0x4e9c, 0x41}) && status == Ok, "ISO-2022-JP");
    ucnv_getToUCallBack_nid_postfix(iso2022, &toAction, &toContext);
    Require(toAction == ToSubstitute(), "ISO-2022-JP keeps the guest-visible callback");
    ucnv_close_nid_postfix(iso2022);
}

void CheckGuestCallbacks() {
    UConverter* converter = Open("US-ASCII");
    Recorder from{{}, "[?]"};
    Icu::ConverterFromUCallback oldFrom = nullptr;
    const void* oldFromContext = &from;
    std::int32_t status = Ok;
    ucnv_setFromUCallBack_nid_postfix(converter, RecordFromUnicode_nid_no_patch, &from, &oldFrom, &oldFromContext, &status);
    Require(status == Ok && oldFrom == FromSubstitute() && oldFromContext == nullptr, "set returns the SUBSTITUTE export as the old callback");
    Require(HostFromContext(converter) != &from, "a guest callback runs through a host record");
    const std::u16string text = Units({0x41, 0x20ac, 0x42, 0xd83d, 0xde00});
    Require(Convert(converter, text, &status) == "A[?]B[?]" && status == Ok, "a guest callback writes its replacement");
    Require(from.calls.size() >= 2, "the guest callback is called");
    std::vector<Call> errors;
    for (const Call& call : from.calls)
        if (call.reason <= UCNV_IRREGULAR) errors.push_back(call);
    Require(errors.size() == 2, "two unassigned characters");
    Require(errors[0].reason == UCNV_UNASSIGNED && errors[0].converter == converter && errors[0].length == 1 && errors[0].codePoint == 0x20ac && errors[0].firstUnit == 0x20ac && errors[0].size == sizeof(UConverterFromUnicodeArgs), "the guest callback receives ICU's arguments");
    Require(errors[1].length == 2 && errors[1].codePoint == 0x1f600 && errors[1].firstUnit == 0xd83d, "the guest callback receives a surrogate pair");
    Icu::ConverterFromUCallback action = nullptr;
    const void* context = nullptr;
    ucnv_getFromUCallBack_nid_postfix(converter, &action, &context);
    Require(action == RecordFromUnicode_nid_no_patch && context == &from, "get returns the guest callback and context");
    from.calls.clear();
    ucnv_reset_nid_postfix(converter);
    Require(from.calls.size() == 1 && from.calls[0].reason == UCNV_RESET && from.calls[0].converter == converter, "reset calls the guest callback");
    from.calls.clear();
    ucnv_resetToUnicode_nid_postfix(converter);
    Require(from.calls.empty(), "a to-Unicode reset leaves the from-Unicode callback");
    Recorder to{{}, ""};
    Icu::ConverterToUCallback oldTo = nullptr;
    const void* oldToContext = &to;
    ucnv_setToUCallBack_nid_postfix(converter, RecordToUnicode_nid_no_patch, &to, &oldTo, &oldToContext, &status);
    Require(status == Ok && oldTo == ToSubstitute() && oldToContext == nullptr, "setToUCallBack old values");
    Require(ConvertBack(converter, Bytes({0x41, 0x80, 0x42}), &status) == Units({0x41, 0x2329, 0x232a, 0x42}) && status == Ok, "a guest to-Unicode callback writes its replacement");
    Require(!to.calls.empty() && to.calls.back().reason == UCNV_ILLEGAL && to.calls.back().length == 1 && to.calls.back().firstUnit == 0x80 && to.calls.back().converter == converter, "the guest to-Unicode callback receives ICU's arguments");
    Icu::ConverterToUCallback toAction = nullptr;
    const void* toContext = nullptr;
    ucnv_getToUCallBack_nid_postfix(converter, &toAction, &toContext);
    Require(toAction == RecordToUnicode_nid_no_patch && toContext == &to, "getToUCallBack returns the guest callback");
    ucnv_setFromUCallBack_nid_postfix(converter, FromEscape(), UCNV_ESCAPE_XML_DEC, &oldFrom, &oldFromContext, &status);
    Require(status == Ok && oldFrom == RecordFromUnicode_nid_no_patch && oldFromContext == &from, "replacing returns the guest callback");
    Require(HostFromContext(converter) == UCNV_ESCAPE_XML_DEC, "the ESCAPE export is installed as ICU's own callback");
    Require(Convert(converter, Units({0x41, 0x20ac, 0xd83d, 0xde00}), &status) == "A&#8364;&#128512;" && status == Ok, "ESCAPE with the XML decimal context");
    ucnv_getFromUCallBack_nid_postfix(converter, &action, &context);
    Require(action == FromEscape() && context == UCNV_ESCAPE_XML_DEC, "get returns the ESCAPE export and its context");
    ucnv_setToUCallBack_nid_postfix(converter, ToEscape(), UCNV_ESCAPE_C, &oldTo, &oldToContext, &status);
    Require(oldTo == RecordToUnicode_nid_no_patch && oldToContext == &to && HostToContext(converter) == UCNV_ESCAPE_C, "the to-Unicode ESCAPE export is installed directly");
    const std::u16string escaped = ConvertBack(converter, Bytes({0x41, 0x80}), &status);
    Require(escaped == Units({0x41, 0x5c, 0x78, 0x38, 0x30}), "to-Unicode ESCAPE with the C context");
    const std::u16string skipping = Units({0x41, 0xe9, 0x42});
    ucnv_setFromUCallBack_nid_postfix(converter, FromSkip(), nullptr, nullptr, nullptr, &status);
    Require(HostFromContext(converter) != nullptr && Convert(converter, skipping, &status) == "AB" && status == Ok, "the hand SKIP runs through the bridge");
    ucnv_getFromUCallBack_nid_postfix(converter, &action, &context);
    Require(action == FromSkip() && context == nullptr, "get returns the SKIP export");
    ucnv_setFromUCallBack_nid_postfix(converter, FromStop(), nullptr, nullptr, nullptr, &status);
    Require(Convert(converter, Units({0x41, 0x180f, 0x42}), &status) == "AB" && status == Ok, "the hand STOP clears the error for U+180F");
    Convert(converter, skipping, &status);
    Require(status == U_INVALID_CHAR_FOUND, "the hand STOP stops on an unassigned character");
    status = Ok;
    ucnv_setToUCallBack_nid_postfix(converter, ToStop(), nullptr, nullptr, nullptr, &status);
    ConvertBack(converter, Bytes({0x41, 0x80}), &status);
    Require(status == U_ILLEGAL_CHAR_FOUND, "to-Unicode STOP");
    status = Ok;
    ucnv_setToUCallBack_nid_postfix(converter, ToSkip(), nullptr, nullptr, nullptr, &status);
    Require(ConvertBack(converter, Bytes({0x41, 0x80, 0x42}), &status) == Units({0x41, 0x42}) && status == Ok, "to-Unicode SKIP");
    ucnv_getToUCallBack_nid_postfix(converter, &toAction, &toContext);
    Require(toAction == ToSkip(), "get returns the to-Unicode SKIP export");
    ucnv_setFromUCallBack_nid_postfix(converter, UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix, UCNV_ESCAPE_XML_HEX, nullptr, nullptr, &status);
    Require(Convert(converter, Units({0x41, 0x20ac}), &status) == "A&#x20AC;" && status == Ok, "the ESCAPE export through a guest-visible address");
    ucnv_getFromUCallBack_nid_postfix(converter, &action, &context);
    Require(action == UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix && context == UCNV_ESCAPE_XML_HEX, "get returns the address the guest set");
    ucnv_setFromUCallBack_nid_postfix(converter, FromSubstitute(), nullptr, nullptr, nullptr, &status);
    Require(HostFromContext(converter) == nullptr && Convert(converter, Units({0x41, 0x20ac}), &status) == Bytes({0x41, 0x1a}), "SUBSTITUTE again");
    from.calls.clear();
    from.checkOnClose = true;
    ucnv_setFromUCallBack_nid_postfix(converter, RecordFromUnicode_nid_no_patch, &from, nullptr, nullptr, &status);
    ucnv_close_nid_postfix(converter);
    Require(from.calls.size() == 1 && from.calls[0].reason == UCNV_CLOSE && from.calls[0].converter == converter, "close calls the guest callback");
}

void CheckCallbackErrors() {
    UConverter* converter = Open("US-ASCII");
    Icu::ConverterFromUCallback oldFrom = reinterpret_cast<Icu::ConverterFromUCallback>(std::uintptr_t{1});
    const void* oldContext = &oldFrom;
    std::int32_t status = U_INVALID_CHAR_FOUND;
    Recorder recorder{{}, "x"};
    ucnv_setFromUCallBack_nid_postfix(converter, RecordFromUnicode_nid_no_patch, &recorder, &oldFrom, &oldContext, &status);
    Require(oldFrom == reinterpret_cast<Icu::ConverterFromUCallback>(std::uintptr_t{1}) && oldContext == &oldFrom, "a failed status leaves everything");
    Icu::ConverterFromUCallback action = nullptr;
    const void* context = nullptr;
    ucnv_getFromUCallBack_nid_postfix(converter, &action, &context);
    Require(action == FromSubstitute(), "a failed status sets nothing");
    ucnv_setFromUCallBack_nid_postfix(nullptr, nullptr, nullptr, nullptr, nullptr, &status);
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_setFromUCallBack_nid_postfix(converter, nullptr, nullptr, nullptr, nullptr, &error); }), "a null callback");
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_setToUCallBack_nid_postfix(converter, nullptr, nullptr, nullptr, nullptr, &error); }), "a null to-Unicode callback");
    Require(Invalid([&] { ucnv_setFromUCallBack_nid_postfix(converter, RecordFromUnicode_nid_no_patch, &recorder, nullptr, nullptr, nullptr); }), "a null status");
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_setToUCallBack_nid_postfix(nullptr, RecordToUnicode_nid_no_patch, &recorder, nullptr, nullptr, &error); }), "a null converter");
    ucnv_getFromUCallBack_nid_postfix(converter, &action, &context);
    Require(action == FromSubstitute(), "rejected calls set nothing");
    ucnv_close_nid_postfix(converter);
}

void CheckClones() {
    UConverter* original = Open("US-ASCII");
    Recorder from{{}, "<c>"};
    Recorder to{{}, ""};
    std::int32_t status = Ok;
    ucnv_setFromUCallBack_nid_postfix(original, RecordFromUnicode_nid_no_patch, &from, nullptr, nullptr, &status);
    ucnv_setToUCallBack_nid_postfix(original, RecordToUnicode_nid_no_patch, &to, nullptr, nullptr, &status);
    std::int32_t size = 0;
    Require(ucnv_safeClone_nid_postfix(original, nullptr, &size, &status) == nullptr && size > 0 && status == Ok, "safeClone preflight");
    from.calls.clear();
    UConverter* clone = ucnv_safeClone_nid_postfix(original, nullptr, nullptr, &status);
    Require(clone && status == U_SAFECLONE_ALLOCATED_WARNING, "safeClone allocates");
    Require(!from.calls.empty() && from.calls.back().reason == UCNV_CLONE && from.calls.back().converter == clone, "safeClone calls the guest callback for the clone");
    Require(HostFromContext(clone) != HostFromContext(original) && HostToContext(clone) != HostToContext(original), "the clone has its own records");
    Icu::ConverterFromUCallback action = nullptr;
    const void* context = nullptr;
    ucnv_getFromUCallBack_nid_postfix(clone, &action, &context);
    Require(action == RecordFromUnicode_nid_no_patch && context == &from, "the clone reports the guest callback");
    Icu::ConverterToUCallback toAction = nullptr;
    const void* toContext = nullptr;
    ucnv_getToUCallBack_nid_postfix(clone, &toAction, &toContext);
    Require(toAction == RecordToUnicode_nid_no_patch && toContext == &to, "the clone reports the guest to-Unicode callback");
    from.calls.clear();
    ucnv_close_nid_postfix(original);
    Require(from.calls.size() == 1 && from.calls[0].reason == UCNV_CLOSE && from.calls[0].converter == original, "closing the original");
    status = Ok;
    from.calls.clear();
    Require(Convert(clone, Units({0x41, 0x20ac}), &status) == "A<c>" && status == Ok, "the clone still runs the guest callback");
    Require(from.calls.back().converter == clone, "the clone passes itself");
    Require(ConvertBack(clone, Bytes({0x80}), &status) == Units({0x2329, 0x232a}), "the clone runs the guest to-Unicode callback");
    alignas(16) std::array<char, U_CNV_SAFECLONE_BUFFERSIZE> buffer{};
    std::int32_t bufferSize = static_cast<std::int32_t>(buffer.size());
    status = Ok;
    UConverter* stackClone = ucnv_safeClone_nid_postfix(clone, buffer.data(), &bufferSize, &status);
    Require(stackClone && status == Ok && reinterpret_cast<char*>(stackClone) >= buffer.data() && reinterpret_cast<char*>(stackClone) < buffer.data() + buffer.size(), "safeClone into a buffer");
    Require(HostFromContext(stackClone) != HostFromContext(clone), "a buffer clone has its own record");
    ucnv_close_nid_postfix(clone);
    Require(Convert(stackClone, Units({0x20ac}), &status) == "<c>", "a buffer clone outlives its source");
    ucnv_close_nid_postfix(stackClone);
    UConverter* source = Open("US-ASCII");
    Recorder later{{}, "{n}"};
    clonedRecorder = &later;
    int marker = 0;
    ucnv_setFromUCallBack_nid_postfix(source, CloneFromUnicode_nid_no_patch, &marker, nullptr, nullptr, &status);
    status = Ok;
    UConverter* swapped = ucnv_safeClone_nid_postfix(source, nullptr, nullptr, &status);
    ucnv_getFromUCallBack_nid_postfix(swapped, &action, &context);
    Require(action == RecordFromUnicode_nid_no_patch && context == &later, "a callback set during UCNV_CLONE stays on the clone");
    ucnv_getFromUCallBack_nid_postfix(source, &action, &context);
    Require(action == CloneFromUnicode_nid_no_patch && context == &marker, "the source keeps its callback");
    status = Ok;
    Require(Convert(swapped, Units({0x20ac}), &status) == "{n}", "the clone runs the callback set during UCNV_CLONE");
    ucnv_close_nid_postfix(swapped);
    ucnv_close_nid_postfix(source);
    UConverter* substituted = Open("US-ASCII");
    Recorder internal{{}, "?"};
    ucnv_setFromUCallBack_nid_postfix(substituted, RecordFromUnicode_nid_no_patch, &internal, nullptr, nullptr, &status);
    const std::u16string replacement = Units({0x2a});
    status = Ok;
    ucnv_setSubstString_nid_postfix(substituted, replacement.data(), 1, &status);
    Require(status == Ok && internal.calls.size() == 1 && internal.calls[0].reason == UCNV_CLONE && internal.calls[0].converter != substituted, "setSubstString clones through ICU");
    ucnv_getFromUCallBack_nid_postfix(substituted, &action, &context);
    Require(action == RecordFromUnicode_nid_no_patch && context == &internal, "setSubstString keeps the guest callback");
    ucnv_close_nid_postfix(substituted);
    Require(ucnv_safeClone_nid_postfix(nullptr, nullptr, nullptr, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR, "safeClone of a null converter");
    Require(ucnv_safeClone_nid_postfix(nullptr, nullptr, nullptr, nullptr) == nullptr, "safeClone with a null status");
}

UConverterFromUnicodeArgs FromArguments(UConverter* converter, char* target, std::size_t capacity) {
    UConverterFromUnicodeArgs args{};
    args.size = sizeof(args);
    args.flush = 1;
    args.converter = converter;
    args.target = target;
    args.targetLimit = target + capacity;
    return args;
}

UConverterToUnicodeArgs ToArguments(UConverter* converter, char16_t* target, std::size_t capacity) {
    UConverterToUnicodeArgs args{};
    args.size = sizeof(args);
    args.flush = 1;
    args.converter = converter;
    args.target = target;
    args.targetLimit = target + capacity;
    return args;
}

Icu::ConverterFromUnicodeArgs* Guest(UConverterFromUnicodeArgs* args) {
    return reinterpret_cast<Icu::ConverterFromUnicodeArgs*>(args);
}

Icu::ConverterToUnicodeArgs* Guest(UConverterToUnicodeArgs* args) {
    return reinterpret_cast<Icu::ConverterToUnicodeArgs*>(args);
}

void CheckEscapeExports() {
    UConverter* guest = Open("US-ASCII");
    UErrorCode hostStatus = U_ZERO_ERROR;
    UConverter* host = ucnv_open("US-ASCII", &hostStatus);
    const char* contexts[] = {nullptr, UCNV_ESCAPE_JAVA, UCNV_ESCAPE_C, UCNV_ESCAPE_XML_DEC, UCNV_ESCAPE_XML_HEX, UCNV_ESCAPE_UNICODE, UCNV_ESCAPE_CSS2, "", "Q"};
    const char16_t pair[] = {0xd83d, 0xde00, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47};
    for (const char* context : contexts) {
        for (std::int32_t length : {0, 1, 2, 8}) {
            char guestTarget[64] = {};
            char hostTarget[64] = {};
            UConverterFromUnicodeArgs guestArgs = FromArguments(guest, guestTarget, sizeof(guestTarget));
            UConverterFromUnicodeArgs hostArgs = FromArguments(host, hostTarget, sizeof(hostTarget));
            std::int32_t status = U_INVALID_CHAR_FOUND;
            hostStatus = U_INVALID_CHAR_FOUND;
            UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix(context, Guest(&guestArgs), pair, length, 0x1f600, UCNV_UNASSIGNED, &status);
            UCNV_FROM_U_CALLBACK_ESCAPE(context, &hostArgs, pair, length, 0x1f600, UCNV_UNASSIGNED, &hostStatus);
            const std::string label = std::string("from-Unicode ESCAPE ") + (context ? context : "NULL") + " " + std::to_string(length);
            Require(status == hostStatus && guestArgs.target - guestTarget == hostArgs.target - hostTarget && std::memcmp(guestTarget, hostTarget, sizeof(hostTarget)) == 0, label);
        }
    }
    const char bytes[] = {char(0x80), char(0xff), char(0x01), char(0x7f), char(0x80), char(0xff), char(0x01), char(0x7f), char(0x10), char(0x20), char(0x30), char(0x40)};
    for (const char* context : {static_cast<const char*>(nullptr), UCNV_ESCAPE_XML_DEC, UCNV_ESCAPE_XML_HEX, UCNV_ESCAPE_C, UCNV_ESCAPE_UNICODE, ""}) {
        for (std::int32_t length : {0, 1, 2, 7}) {
            char16_t guestTarget[64] = {};
            char16_t hostTarget[64] = {};
            UConverterToUnicodeArgs guestArgs = ToArguments(guest, guestTarget, 64);
            UConverterToUnicodeArgs hostArgs = ToArguments(host, hostTarget, 64);
            std::int32_t status = U_ILLEGAL_CHAR_FOUND;
            hostStatus = U_ILLEGAL_CHAR_FOUND;
            UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix(context, Guest(&guestArgs), bytes, length, UCNV_ILLEGAL, &status);
            UCNV_TO_U_CALLBACK_ESCAPE(context, &hostArgs, bytes, length, UCNV_ILLEGAL, &hostStatus);
            const std::string label = std::string("to-Unicode ESCAPE ") + (context ? context : "NULL") + " " + std::to_string(length);
            Require(status == hostStatus && guestArgs.target - guestTarget == hostArgs.target - hostTarget && std::memcmp(guestTarget, hostTarget, sizeof(hostTarget)) == 0, label);
        }
    }
    for (std::int32_t reason : {0, 1, 2, 3, 4, 5}) {
        for (const char* context : {static_cast<const char*>(nullptr), UCNV_SUB_STOP_ON_ILLEGAL, "x"}) {
            for (std::int32_t codePoint : {0x20ac, 0x200b}) {
                char guestTarget[8] = {};
                char hostTarget[8] = {};
                UConverterFromUnicodeArgs guestArgs = FromArguments(guest, guestTarget, sizeof(guestTarget));
                UConverterFromUnicodeArgs hostArgs = FromArguments(host, hostTarget, sizeof(hostTarget));
                std::int32_t status = U_INVALID_CHAR_FOUND;
                hostStatus = U_INVALID_CHAR_FOUND;
                UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix(context, Guest(&guestArgs), nullptr, 1, codePoint, reason, &status);
                UCNV_FROM_U_CALLBACK_SUBSTITUTE(context, &hostArgs, nullptr, 1, codePoint, static_cast<UConverterCallbackReason>(reason), &hostStatus);
                const std::string label = "from-Unicode SUBSTITUTE " + std::to_string(reason) + " " + (context ? context : "NULL") + " " + std::to_string(codePoint);
                Require(status == hostStatus && guestArgs.target - guestTarget == hostArgs.target - hostTarget && std::memcmp(guestTarget, hostTarget, 8) == 0, label);
                char16_t guestUnits[4] = {};
                char16_t hostUnits[4] = {};
                UConverterToUnicodeArgs guestTo = ToArguments(guest, guestUnits, 4);
                UConverterToUnicodeArgs hostTo = ToArguments(host, hostUnits, 4);
                status = U_ILLEGAL_CHAR_FOUND;
                hostStatus = U_ILLEGAL_CHAR_FOUND;
                UCNV_TO_U_CALLBACK_SUBSTITUTE_nid_postfix(context, Guest(&guestTo), "\x80", 1, reason, &status);
                UCNV_TO_U_CALLBACK_SUBSTITUTE(context, &hostTo, "\x80", 1, static_cast<UConverterCallbackReason>(reason), &hostStatus);
                Require(status == hostStatus && guestTo.target - guestUnits == hostTo.target - hostUnits && std::memcmp(guestUnits, hostUnits, sizeof(hostUnits)) == 0, "to-Unicode " + label);
            }
        }
    }
    char target[64];
    UConverterFromUnicodeArgs args = FromArguments(guest, target, sizeof(target));
    const char16_t nine[9] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49};
    Require(Invalid([&] { std::int32_t error = U_INVALID_CHAR_FOUND; UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix(nullptr, Guest(&args), nine, 9, 0x41, UCNV_UNASSIGNED, &error); }), "ESCAPE past ICU's 48-unit buffer");
    Require(Invalid([&] { std::int32_t error = U_INVALID_CHAR_FOUND; UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix(UCNV_ESCAPE_JAVA, Guest(&args), nine, 9, 0x41, UCNV_UNASSIGNED, &error); }), "Java ESCAPE past ICU's buffer");
    std::int32_t escapeStatus = U_INVALID_CHAR_FOUND;
    UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix(UCNV_ESCAPE_CSS2, Guest(&args), nullptr, 9, 0x41, UCNV_UNASSIGNED, &escapeStatus);
    Require(escapeStatus == Ok, "CSS2 ESCAPE reads only the code point");
    Require(Invalid([&] { std::int32_t error = U_INVALID_CHAR_FOUND; UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix(UCNV_ESCAPE_C, Guest(&args), nullptr, 1, 0x41, UCNV_UNASSIGNED, &error); }), "C ESCAPE reads the first unit");
    Require(Invalid([&] { std::int32_t error = U_INVALID_CHAR_FOUND; UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix(nullptr, nullptr, nine, 1, 0x41, UCNV_UNASSIGNED, &error); }), "ESCAPE with null arguments");
    Require(Invalid([&] { UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix(nullptr, Guest(&args), nine, 1, 0x41, UCNV_UNASSIGNED, nullptr); }), "ESCAPE with a null status");
    UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix(nullptr, nullptr, nullptr, 9, 0x41, UCNV_CLOSE, nullptr);
    escapeStatus = U_INVALID_CHAR_FOUND;
    UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix(nullptr, nullptr, nullptr, 9, 0x200b, UCNV_UNASSIGNED, &escapeStatus);
    Require(escapeStatus == Ok, "ESCAPE skips a default ignorable without arguments");
    escapeStatus = U_INVALID_CHAR_FOUND;
    UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix("x", nullptr, nullptr, 1, 0x41, UCNV_UNASSIGNED, &escapeStatus);
    Require(escapeStatus == U_INVALID_CHAR_FOUND, "SUBSTITUTE with another context leaves the error");
    UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix("x", nullptr, nullptr, 1, 0x41, UCNV_UNASSIGNED, nullptr);
    escapeStatus = U_INVALID_CHAR_FOUND;
    UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix(nullptr, nullptr, nullptr, 1, 0x200b, UCNV_UNASSIGNED, &escapeStatus);
    Require(escapeStatus == Ok, "SUBSTITUTE skips a default ignorable without arguments");
    Require(Invalid([] { std::int32_t error = U_INVALID_CHAR_FOUND; UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix(nullptr, nullptr, nullptr, 1, 0x41, UCNV_UNASSIGNED, &error); }), "SUBSTITUTE with null arguments");
    Require(Invalid([] { UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix(nullptr, nullptr, nullptr, 1, 0x200b, UCNV_UNASSIGNED, nullptr); }), "SUBSTITUTE with a null status");
    UConverterFromUnicodeArgs detached = FromArguments(nullptr, target, sizeof(target));
    Require(Invalid([&] { std::int32_t error = U_INVALID_CHAR_FOUND; UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix(nullptr, Guest(&detached), nullptr, 1, 0x41, UCNV_UNASSIGNED, &error); }), "SUBSTITUTE without a converter");
    char16_t units[64];
    UConverterToUnicodeArgs toArgs = ToArguments(guest, units, 64);
    const char dec[9] = {char(0xff), char(0xff), char(0xff), char(0xff), char(0xff), char(0xff), char(0xff), char(0xff), char(0xff)};
    std::int32_t toStatus = U_ILLEGAL_CHAR_FOUND;
    UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix(UCNV_ESCAPE_XML_DEC, Guest(&toArgs), dec, 8, UCNV_ILLEGAL, &toStatus);
    Require(toStatus == Ok && toArgs.target - units == 48, "XML decimal ESCAPE fills ICU's buffer exactly");
    Require(Invalid([&] { std::int32_t error = U_ILLEGAL_CHAR_FOUND; UConverterToUnicodeArgs more = ToArguments(guest, units, 64); UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix(UCNV_ESCAPE_XML_DEC, Guest(&more), dec, 9, UCNV_ILLEGAL, &error); }), "XML decimal ESCAPE past ICU's buffer");
    const char small[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    toArgs = ToArguments(guest, units, 64);
    toStatus = U_ILLEGAL_CHAR_FOUND;
    UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix(UCNV_ESCAPE_XML_HEX, Guest(&toArgs), small, 9, UCNV_ILLEGAL, &toStatus);
    Require(toStatus == Ok && toArgs.target - units == 45, "XML hex ESCAPE of one-digit bytes");
    Require(Invalid([&] { std::int32_t error = U_ILLEGAL_CHAR_FOUND; UConverterToUnicodeArgs more = ToArguments(guest, units, 64); UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix(UCNV_ESCAPE_XML_HEX, Guest(&more), small, 10, UCNV_ILLEGAL, &error); }), "XML hex ESCAPE past ICU's buffer");
    toArgs = ToArguments(guest, units, 64);
    toStatus = U_ILLEGAL_CHAR_FOUND;
    UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix(UCNV_ESCAPE_C, Guest(&toArgs), bytes, 12, UCNV_ILLEGAL, &toStatus);
    Require(toStatus == Ok && toArgs.target - units == 48, "C ESCAPE of twelve bytes");
    const char thirteen[13] = {};
    Require(Invalid([&] { std::int32_t error = U_ILLEGAL_CHAR_FOUND; UConverterToUnicodeArgs more = ToArguments(guest, units, 64); UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix(nullptr, Guest(&more), thirteen, 13, UCNV_ILLEGAL, &error); }), "ESCAPE of thirteen bytes");
    Require(Invalid([&] { std::int32_t error = U_ILLEGAL_CHAR_FOUND; UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix(nullptr, Guest(&toArgs), nullptr, 1, UCNV_ILLEGAL, &error); }), "to-Unicode ESCAPE null bytes");
    Require(Invalid([&] { UCNV_TO_U_CALLBACK_SUBSTITUTE_nid_postfix(nullptr, Guest(&toArgs), nullptr, 1, UCNV_ILLEGAL, nullptr); }), "to-Unicode SUBSTITUTE null status");
    UCNV_TO_U_CALLBACK_SUBSTITUTE_nid_postfix("x", nullptr, nullptr, 1, UCNV_ILLEGAL, nullptr);
    UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix(nullptr, nullptr, nullptr, 99, UCNV_RESET, nullptr);
    ucnv_close(host);
    ucnv_close_nid_postfix(guest);
}

void CheckWriteExports() {
    UConverter* converter = Open("US-ASCII");
    char target[4];
    UConverterFromUnicodeArgs args = FromArguments(converter, target, sizeof(target));
    std::string bytes(36, 'z');
    std::int32_t status = Ok;
    ucnv_cbFromUWriteBytes_nid_postfix(Guest(&args), bytes.data(), 36, 0, &status);
    Require(status == U_BUFFER_OVERFLOW_ERROR && args.target == target + 4, "cbFromUWriteBytes keeps 32 bytes for later");
    char rest[40];
    char* restTarget = rest;
    const char16_t* none = nullptr;
    status = Ok;
    ucnv_fromUnicode_nid_postfix(converter, &restTarget, rest + sizeof(rest), &none, nullptr, nullptr, 1, &status);
    Require(status == Ok && restTarget - rest == 32, "the overflow bytes come out on the next call");
    args = FromArguments(converter, target, sizeof(target));
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_cbFromUWriteBytes_nid_postfix(Guest(&args), bytes.data(), 37, 0, &error); }), "cbFromUWriteBytes past the overflow buffer");
    UConverterFromUnicodeArgs detached = FromArguments(nullptr, target, sizeof(target));
    std::string many(100, 'y');
    status = Ok;
    ucnv_cbFromUWriteBytes_nid_postfix(Guest(&detached), many.data(), 100, 0, &status);
    Require(status == U_BUFFER_OVERFLOW_ERROR && detached.target == target + 4, "without a converter ICU drops the overflow");
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_cbFromUWriteBytes_nid_postfix(Guest(&args), nullptr, 1, 0, &error); }), "cbFromUWriteBytes null bytes");
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_cbFromUWriteBytes_nid_postfix(nullptr, "a", 1, 0, &error); }), "cbFromUWriteBytes null arguments");
    std::int32_t failed = U_INVALID_CHAR_FOUND;
    ucnv_cbFromUWriteBytes_nid_postfix(nullptr, nullptr, 99, 0, &failed);
    ucnv_cbFromUWriteSub_nid_postfix(nullptr, 0, &failed);
    ucnv_cbFromUWriteUChars_nid_postfix(nullptr, nullptr, nullptr, 0, &failed);
    ucnv_cbToUWriteUChars_nid_postfix(nullptr, nullptr, 99, 0, &failed);
    Require(failed == U_INVALID_CHAR_FOUND, "a failed status returns first");
    ucnv_resetFromUnicode_nid_postfix(converter);
    char subTarget[8] = {};
    UConverterFromUnicodeArgs subArgs = FromArguments(converter, subTarget, sizeof(subTarget));
    status = Ok;
    ucnv_cbFromUWriteSub_nid_postfix(Guest(&subArgs), 0, &status);
    Require(status == Ok && subArgs.target == subTarget + 1 && subTarget[0] == 0x1a, "cbFromUWriteSub");
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_cbFromUWriteSub_nid_postfix(Guest(&detached), 0, &error); }), "cbFromUWriteSub without a converter");
    const std::u16string text = Units({0x41, 0x42});
    const char16_t* source = text.data();
    status = Ok;
    ucnv_cbFromUWriteUChars_nid_postfix(Guest(&subArgs), &source, text.data() + 2, 0, &status);
    Require(status == Ok && subArgs.target == subTarget + 3 && subTarget[1] == 'A' && subTarget[2] == 'B' && source == text.data() + 2, "cbFromUWriteUChars");
    status = Ok;
    ucnv_cbFromUWriteUChars_nid_postfix(Guest(&detached), &source, text.data() + 2, 0, &status);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR, "cbFromUWriteUChars without a converter");
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_cbFromUWriteUChars_nid_postfix(nullptr, &source, text.data(), 0, &error); }), "cbFromUWriteUChars null arguments");
    char16_t units[2];
    UConverterToUnicodeArgs toArgs = ToArguments(converter, units, 2);
    std::u16string unitText(34, u'q');
    status = Ok;
    ucnv_cbToUWriteUChars_nid_postfix(Guest(&toArgs), unitText.data(), 34, 0, &status);
    Require(status == U_BUFFER_OVERFLOW_ERROR && toArgs.target == units + 2, "cbToUWriteUChars keeps 32 units");
    ucnv_resetToUnicode_nid_postfix(converter);
    toArgs = ToArguments(converter, units, 2);
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_cbToUWriteUChars_nid_postfix(Guest(&toArgs), unitText.data(), 35, 0, &error); }), "cbToUWriteUChars past the overflow buffer");
    Require(Invalid([&] { std::int32_t error = Ok; ucnv_cbToUWriteUChars_nid_postfix(Guest(&toArgs), nullptr, 1, 0, &error); }), "cbToUWriteUChars null units");
    char16_t subUnits[4] = {};
    UConverterToUnicodeArgs subTo = ToArguments(converter, subUnits, 4);
    status = Ok;
    ucnv_cbToUWriteSub_nid_postfix(Guest(&subTo), 0, &status);
    Require(status == Ok && subTo.target == subUnits + 1 && subUnits[0] == 0xfffd, "cbToUWriteSub");
    Require(Invalid([&] { ucnv_cbToUWriteSub_nid_postfix(Guest(&subTo), 0, nullptr); }), "cbToUWriteSub null status");
    Require(Invalid([] { std::int32_t error = U_INVALID_CHAR_FOUND; ucnv_cbToUWriteSub_nid_postfix(nullptr, 0, &error); }), "cbToUWriteSub reads the converter first");
    ucnv_close_nid_postfix(converter);
}

void CheckThreads() {
    std::vector<std::thread> threads;
    std::atomic<int> failures{0};
    for (int index = 0; index < 8; ++index) {
        threads.emplace_back([&failures, index] {
            for (int round = 0; round < 50; ++round) {
                std::int32_t status = Ok;
                UConverter* converter = ucnv_open_nid_postfix("US-ASCII", &status);
                Recorder recorder{{}, std::string(1, static_cast<char>('a' + index))};
                ucnv_setFromUCallBack_nid_postfix(converter, RecordFromUnicode_nid_no_patch, &recorder, nullptr, nullptr, &status);
                UConverter* clone = ucnv_safeClone_nid_postfix(converter, nullptr, nullptr, &status);
                ucnv_close_nid_postfix(converter);
                status = Ok;
                const std::u16string text = Units({0x20ac});
                char out[8];
                const std::int32_t length = ucnv_fromUChars_nid_postfix(clone, out, sizeof(out), text.data(), 1, &status);
                if (status != Ok || length != 1 || out[0] != static_cast<char>('a' + index)) ++failures;
                ucnv_close_nid_postfix(clone);
            }
        });
    }
    for (std::thread& thread : threads) thread.join();
    Require(failures.load() == 0, "guest callbacks on clones across threads");
}

}

int main() {
    UErrorCode status = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &status);
    SetDefault(nullptr);
    CheckKnownConversions();
    CheckStreaming();
    CheckPreflight();
    CheckConvert();
    CheckProperties();
    CheckSubstitution();
    CheckOpenErrors();
    CheckDefault();
    CheckSweep();
    CheckDefaultCallbacks();
    CheckGuestCallbacks();
    CheckCallbackErrors();
    CheckClones();
    CheckEscapeExports();
    CheckWriteExports();
    CheckThreads();
    SetDefault(nullptr);
    std::puts("guest_icu_converter passed");
    return 0;
}
