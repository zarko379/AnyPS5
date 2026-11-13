#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::uint64_t MallocFailure = 0x0dfff041;
constexpr std::uint64_t UnknownNid = 0x08fff065;
constexpr std::uint64_t FirstNumberTooLarge = 0x0dfff07a;
constexpr std::uint64_t InvalidDigit = 0x0dfff082;
constexpr std::uint64_t InvalidSeparator = 0x0dfff083;
constexpr std::uint64_t MissingSecondNumber = 0x0dfff08a;
constexpr std::uint64_t SecondNumberTooLarge = 0x0dfff093;
constexpr std::uint64_t TooLong = 0x0dfff09b;
constexpr std::uint64_t InvalidNumber = 0x0dfff0bb;

int liveBlocks = 0;
int callsBeforeFailure = -1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 OBJ text check failed at line %u\n", location.line());
    std::abort();
}

bool Fail() {
    if (callsBeforeFailure == 0) {
        callsBeforeFailure = -1;
        return true;
    }
    if (callsBeforeFailure > 0) --callsBeforeFailure;
    return false;
}

void* APS5_VABI Allocate(std::size_t bytes) {
    if (Fail()) return nullptr;
    ++liveBlocks;
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    if (Fail()) return nullptr;
    ++liveBlocks;
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}

void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

void RequireErrors(std::initializer_list<std::uint64_t> expected, std::source_location location = std::source_location::current()) {
    for (const std::uint64_t code : expected) Require(ERR_get_error_nid_postfix() == code, location);
    Require(ERR_get_error_nid_postfix() == 0, location);
}

template <typename TAction>
void RequireInvalid(TAction action, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, location);
}

std::string Text(const LibreSslAsn1Object* object, int noName, int* result = nullptr) {
    char buffer[128];
    std::memset(buffer, 'X', sizeof(buffer));
    const int length = OBJ_obj2txt_nid_postfix(buffer, sizeof(buffer), object, noName);
    if (result) *result = length;
    Require(length == static_cast<int>(std::strlen(buffer)) || length == 0);
    return buffer;
}

std::string Encoded(std::initializer_list<unsigned char> bytes, int noName, int nid = 0) {
    const std::vector<unsigned char> data(bytes);
    const LibreSslAsn1Object object {nullptr, nullptr, nid, static_cast<std::int32_t>(data.size()), data.data(), 0};
    return Text(&object, noName);
}

std::vector<unsigned char> Content(const LibreSslAsn1Object* object) {
    return {object->data, object->data + object->length};
}

void Free(LibreSslAsn1Object* object) {
    ASN1_OBJECT_free_nid_postfix(reinterpret_cast<Asn1Object_nid_no_patch*>(object));
}

constexpr unsigned char RsaEncryption[] {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};

void TestBufferContract() {
    const LibreSslAsn1Object rsa {nullptr, nullptr, 6, sizeof(RsaEncryption), RsaEncryption, 0};
    char buffer[8] {'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'};
    Require(OBJ_obj2txt_nid_postfix(buffer, -1, &rsa, 0) == 0 && buffer[0] == 'X');
    Require(OBJ_obj2txt_nid_postfix(nullptr, -1, &rsa, 0) == 0);
    Require(OBJ_obj2txt_nid_postfix(nullptr, 0, &rsa, 0) == 13);
    Require(OBJ_obj2txt_nid_postfix(nullptr, 0, &rsa, 1) == 20);
    Require(OBJ_obj2txt_nid_postfix(buffer, 0, &rsa, 0) == 13 && buffer[0] == 'X');
    Require(OBJ_obj2txt_nid_postfix(buffer, 1, &rsa, 0) == 13 && buffer[0] == '\0' && buffer[1] == 'X');
    Require(OBJ_obj2txt_nid_postfix(buffer, 5, &rsa, 0) == 13 && std::strcmp(buffer, "rsaE") == 0 && buffer[5] == 'X');
    Require(OBJ_obj2txt_nid_postfix(buffer, 8, &rsa, 1) == 20 && std::strcmp(buffer, "1.2.840") == 0);
    RequireInvalid([&] { OBJ_obj2txt_nid_postfix(nullptr, 1, &rsa, 0); });
    std::memset(buffer, 'X', sizeof(buffer));
    Require(OBJ_obj2txt_nid_postfix(buffer, sizeof(buffer), nullptr, 0) == 0 && buffer[0] == '\0' && buffer[1] == 'X');
    const LibreSslAsn1Object withoutData {"sn", "ln", 6, 9, nullptr, 0};
    std::memset(buffer, 'X', sizeof(buffer));
    Require(OBJ_obj2txt_nid_postfix(buffer, sizeof(buffer), &withoutData, 0) == 0 && buffer[0] == '\0');
    RequireErrors({});
}

void TestNames() {
    int length = 0;
    const LibreSslAsn1Object rsa {nullptr, nullptr, 6, sizeof(RsaEncryption), RsaEncryption, 0};
    Require(Text(&rsa, 0, &length) == "rsaEncryption" && length == 13);
    Require(Text(&rsa, 1, &length) == "1.2.840.113549.1.1.1" && length == 20);
    const LibreSslAsn1Object lookedUp {nullptr, nullptr, 0, sizeof(RsaEncryption), RsaEncryption, 0};
    Require(Text(&lookedUp, 0) == "rsaEncryption");
    const LibreSslAsn1Object emptyNamed {nullptr, nullptr, 6, 0, RsaEncryption, 0};
    Require(Text(&emptyNamed, 0, &length).empty() && length == 0);
    const LibreSslAsn1Object* gost = OBJ_txt2obj_nid_postfix("id-tc26-gost-3410-12-512-paramSetA", 0);
    Require(gost && gost->nid == 943 && gost->flags == 0);
    Require(Text(gost, 0) == "GOST R 34.10-2012 (512 bit) ParamSet A");
    Require(OBJ_txt2obj_nid_postfix("GOST R 34.10-2012 (512 bit) ParamSet B", 0)->nid == 944);
    Require(OBJ_txt2obj_nid_postfix("id-dsa-with-sha256", 0)->nid == 803);
    Require(OBJ_txt2obj_nid_postfix("dsa_with_SHA224", 0)->nid == 802);
    const LibreSslAsn1Object* sha3 = OBJ_txt2obj_nid_postfix("SHA3-256", 0);
    Require(sha3 && sha3->nid == 1032 && Text(sha3, 0) == "sha3-256" && Text(sha3, 1) == "2.16.840.1.101.3.4.2.8");
    const LibreSslAsn1Object sha3Lookup {nullptr, nullptr, 0, sha3->length, sha3->data, 0};
    Require(Text(&sha3Lookup, 0) == "sha3-256");
    Require(OBJ_txt2obj_nid_postfix("tls1-prf", 0)->nid == 1055);
    Require(OBJ_txt2obj_nid_postfix("rsaEncryption", 0)->nid == 6);
    Require(OBJ_txt2obj_nid_postfix("RSA-SHA256", 0)->nid == 668);
    Require(OBJ_txt2obj_nid_postfix("NULL", 0)->nid == 393);
    Require(OBJ_txt2obj_nid_postfix("UNDEF", 0) == nullptr);
    RequireErrors({InvalidSeparator});
    Require(OBJ_txt2obj_nid_postfix("id-tc26-gost-3410-2012-512-paramSetA", 0) == nullptr);
    RequireErrors({InvalidSeparator});
    Require(OBJ_txt2obj_nid_postfix("rsaEncryption", 1) == nullptr);
    RequireErrors({InvalidSeparator});
    RequireErrors({});
}

void TestUnknownNid() {
    for (const int nid : {5000, 102, -1}) {
        Require(Encoded({0x2a, 0x03, 0x04}, 0, nid) == "1.2.3.4");
        RequireErrors({UnknownNid, UnknownNid});
        Require(Encoded({0x2a, 0x03, 0x04}, 1, nid) == "1.2.3.4");
        RequireErrors({});
    }
}

void TestEncodedArcs() {
    Require(Encoded({0x00}, 1) == "0.0");
    Require(Encoded({0x27}, 1) == "0.39");
    Require(Encoded({0x28}, 1) == "1.0");
    Require(Encoded({0x4f}, 1) == "1.39");
    Require(Encoded({0x50}, 1) == "2.0");
    Require(Encoded({0x88, 0x37}, 1) == "2.999");
    Require(Encoded({0x2a, 0x00, 0x7f, 0x81, 0x00}, 1) == "1.2.0.127.128");
    Require(Encoded({0x81, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f}, 1) == "2.18446744073709551535");
    Require(Encoded({0x2a, 0x81, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f}, 1) == "1.2.18446744073709551615");
    Require(Encoded({0x2a, 0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00}, 1).empty());
    Require(Encoded({0x80, 0x01}, 1).empty());
    Require(Encoded({0x2a, 0x80, 0x01}, 1).empty());
    Require(Encoded({0x2a, 0x81, 0x80, 0x01}, 1) == "1.2.16385");
    Require(Encoded({0x2a, 0x86}, 1).empty());
    Require(Encoded({0x2a, 0x86}, 0).empty());
    const unsigned char data[] {0x2a};
    const LibreSslAsn1Object empty {nullptr, nullptr, 0, 0, data, 0};
    Require(Text(&empty, 1).empty());
    const LibreSslAsn1Object negative {nullptr, nullptr, 0, -1, data, 0};
    RequireInvalid([&] { OBJ_obj2txt_nid_postfix(nullptr, 0, &negative, 1); });
    const LibreSslAsn1Object negativeNamed {nullptr, nullptr, 6, -1, data, 0};
    Require(Text(&negativeNamed, 0) == "rsaEncryption");
    RequireErrors({});
}

void RequireParsed(const char* text, std::vector<unsigned char> expected, std::source_location location = std::source_location::current()) {
    const int live = liveBlocks;
    LibreSslAsn1Object* object = OBJ_txt2obj_nid_postfix(text, 1);
    Require(object != nullptr, location);
    Require(object->shortName == nullptr && object->longName == nullptr && object->nid == 0 && object->flags == 0x09, location);
    Require(Content(object) == expected, location);
    Require(liveBlocks == live + 2, location);
    Free(object);
    Require(liveBlocks == live, location);
    RequireErrors({}, location);
}

void RequireRejected(const char* text, std::uint64_t error, std::source_location location = std::source_location::current()) {
    const int live = liveBlocks;
    Require(OBJ_txt2obj_nid_postfix(text, 1) == nullptr, location);
    Require(OBJ_txt2obj_nid_postfix(text, 0) == nullptr, location);
    Require(liveBlocks == live, location);
    RequireErrors({error, error}, location);
}

void TestTextToObject() {
    RequireParsed("1.2.840.113549.1.1.1", {RsaEncryption, RsaEncryption + sizeof(RsaEncryption)});
    LibreSslAsn1Object* dotted = OBJ_txt2obj_nid_postfix("1.2.840.113549.1.1.1", 0);
    Require(dotted && dotted->nid == 0 && dotted->flags == 0x09 && Text(dotted, 0) == "rsaEncryption");
    Free(dotted);
    RequireParsed("1 2 3", {0x2a, 0x03});
    RequireParsed("0.39", {0x27});
    RequireParsed("2.40", {0x78});
    RequireParsed("2.999", {0x88, 0x37});
    RequireParsed("1.01", {0x29});
    RequireParsed("1.2.0.127.128", {0x2a, 0x00, 0x7f, 0x81, 0x00});
    RequireParsed("2.18446744073709551535", {0x81, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f});
    RequireParsed("1.2.18446744073709551615", {0x2a, 0x81, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f});
    RequireParsed("1.2.18446744073709551616", {0x2a, 0x00});
    RequireRejected("", InvalidNumber);
    RequireRejected(".1.2", InvalidNumber);
    RequireRejected(" 1.2", InvalidNumber);
    RequireRejected("1", MissingSecondNumber);
    RequireRejected("1.", InvalidNumber);
    RequireRejected("1..2", InvalidNumber);
    RequireRejected("1.2.", InvalidNumber);
    RequireRejected("3.1", FirstNumberTooLarge);
    RequireRejected("1.40", SecondNumberTooLarge);
    RequireRejected("0.40", SecondNumberTooLarge);
    RequireRejected("2.18446744073709551536", SecondNumberTooLarge);
    RequireRejected("1.2a", InvalidDigit);
    RequireRejected("1.2.\xc3", InvalidDigit);
    RequireRejected("a.1", InvalidSeparator);
    RequireRejected("1.2 3", InvalidSeparator);
    RequireRejected("1 2.3", InvalidSeparator);
    RequireRejected("00.1", InvalidNumber);
    RequireRejected("1.00", InvalidNumber);
    RequireRejected("1.2.184467440737095516150", TooLong);
    RequireRejected("3.a", InvalidDigit);
    RequireInvalid([] { OBJ_txt2obj_nid_postfix(nullptr, 0); });
    RequireInvalid([] { OBJ_txt2obj_nid_postfix(nullptr, 1); });
}

void TestAllocationFailures() {
    callsBeforeFailure = 0;
    Require(OBJ_txt2obj_nid_postfix("1.2.3", 1) == nullptr && liveBlocks == 0);
    RequireErrors({});
    callsBeforeFailure = 1;
    Require(OBJ_txt2obj_nid_postfix("1.2.3", 1) == nullptr && liveBlocks == 0);
    RequireErrors({MallocFailure});
    callsBeforeFailure = 0;
    LibreSslAsn1Object* rsa = OBJ_txt2obj_nid_postfix("rsaEncryption", 0);
    Require(rsa && rsa->nid == 6 && rsa->flags == 0 && liveBlocks == 0);
    Free(rsa);
    Require(rsa->nid == 6 && rsa->data != nullptr && liveBlocks == 0);
    callsBeforeFailure = -1;
    RequireErrors({});
}

void TestAddedObjects() {
    const int first = OBJ_create_nid_postfix("1.2.3.4.5.6", "addedShort", "added long name");
    const int shadow = OBJ_create_nid_postfix("1.2.3.4.5.7", "SHA256", nullptr);
    const int longOnly = OBJ_create_nid_postfix("1.2.3.4.5.8", nullptr, "sha512");
    const int unnamed = OBJ_create_nid_postfix("1.2.3.4.5.9", nullptr, nullptr);
    Require(first != 0 && shadow != 0 && longOnly != 0 && unnamed != 0);
    LibreSslAsn1Object* added = OBJ_txt2obj_nid_postfix("addedShort", 0);
    Require(added && added->nid == first && std::strcmp(added->longName, "added long name") == 0);
    Require(OBJ_txt2obj_nid_postfix("added long name", 0) == added);
    Require(Text(added, 0) == "added long name" && Text(added, 1) == "1.2.3.4.5.6");
    LibreSslAsn1Object* copy = OBJ_txt2obj_nid_postfix("1.2.3.4.5.6", 1);
    Require(copy && copy != added && Text(copy, 0) == "added long name");
    Free(copy);
    const LibreSslAsn1Object* builtIn = OBJ_txt2obj_nid_postfix("jurisdictionLocalityName", 0);
    Require(builtIn && builtIn->nid == 956 && builtIn != added && Text(builtIn, 0) == "jurisdictionLocalityName");
    const LibreSslAsn1Object* shadowed = OBJ_txt2obj_nid_postfix("SHA256", 0);
    Require(shadowed && shadowed->nid == shadow && Text(shadowed, 0) == "SHA256");
    const LibreSslAsn1Object* longNamed = OBJ_txt2obj_nid_postfix("sha512", 0);
    Require(longNamed && longNamed->nid == longOnly && Text(longNamed, 0) == "sha512");
    Require(OBJ_txt2obj_nid_postfix("sha256", 0)->nid == 672);
    LibreSslAsn1Object* bare = OBJ_txt2obj_nid_postfix("1.2.3.4.5.9", 1);
    Require(bare && Text(bare, 0) == "1.2.3.4.5.9");
    Free(bare);
    OBJ_cleanup_nid_postfix();
    Require(OBJ_txt2obj_nid_postfix("SHA256", 0)->nid == 672);
    Require(OBJ_txt2obj_nid_postfix("sha512", 0)->nid == 674);
    Require(OBJ_txt2obj_nid_postfix("addedShort", 0) == nullptr);
    RequireErrors({InvalidSeparator});
    Require(liveBlocks == 0);
}

} // namespace

int main() {
    InitializeHeap();
    TestBufferContract();
    TestNames();
    TestUnknownNid();
    TestEncodedArcs();
    TestTextToObject();
    TestAllocationFailures();
    TestAddedObjects();
    std::puts("LibreSSL3 OBJ text tests passed");
    return 0;
}
