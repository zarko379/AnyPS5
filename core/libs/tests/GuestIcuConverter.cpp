#include "prx/libSceIcu/Converter.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Defaults.hpp"
#include <unicode/uclean.h>
#include <unicode/ucnv.h>
#include <unicode/udata.h>
#include <unicode/ustring.h>
#include <array>
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
    SetDefault(nullptr);
    std::puts("guest_icu_converter passed");
    return 0;
}
