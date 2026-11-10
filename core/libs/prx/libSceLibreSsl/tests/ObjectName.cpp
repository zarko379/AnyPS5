#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

namespace {

constexpr std::uint64_t UnknownNidError = (8ull << 24) | (0xfffull << 12) | 101;
constexpr std::array UnassignedNids = {102, 118, 350, 511, 772, 774, 775};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, message);
}

bool IsUnassigned(int nid) {
    for (const int value : UnassignedNids) if (value == nid) return true;
    return false;
}

bool Equal(const char* actual, const char* expected) {
    return actual && std::strcmp(actual, expected) == 0;
}

void CheckName(int nid, const char* name, const char* message) {
    Require(Equal(OBJ_nid2sn_nid_postfix(nid), name) && OBJ_sn2nid_nid_postfix(name) == nid, message);
}

void CheckUnknownNid(int nid, const char* message) {
    ERR_clear_error_nid_postfix();
    Require(OBJ_nid2sn_nid_postfix(nid) == nullptr, message);
    Require(ERR_get_error_nid_postfix() == UnknownNidError && ERR_get_error_nid_postfix() == 0, message);
}

void CheckBuiltIn() {
    CheckName(0, "UNDEF", "NID 0");
    CheckName(4, "MD5", "MD5");
    CheckName(6, "rsaEncryption", "rsaEncryption");
    CheckName(13, "CN", "commonName");
    CheckName(64, "SHA1", "SHA1");
    CheckName(672, "SHA256", "SHA256");
    CheckName(950, "X25519", "X25519");
    CheckName(955, "Ed448ph", "the last 2.5.4 NID");
    Require(Equal(OBJ_nid2sn_nid_postfix(393), "NULL") && Equal(OBJ_nid2sn_nid_postfix(404), "NULL"), "393 and 404 share the short name NULL");
    Require(OBJ_sn2nid_nid_postfix("NULL") == 404, "the short-name search resolves NULL to 404");
    ERR_clear_error_nid_postfix();
    Require(OBJ_nid2sn_nid_postfix(-1) == nullptr && OBJ_nid2sn_nid_postfix(LibreSslObjectCount) == nullptr, "NIDs outside the table");
    Require(ERR_get_error_nid_postfix() == 0, "NIDs outside the table report no error before OBJ_create");
    for (const int nid : UnassignedNids) CheckUnknownNid(nid, "an unassigned NID returns null and reports OBJ_R_UNKNOWN_NID");
    Require(OBJ_sn2nid_nid_postfix("") == 0 && OBJ_sn2nid_nid_postfix("sha256") == 0 && OBJ_sn2nid_nid_postfix("ZZZ") == 0, "unknown and case-mismatched names");
    for (int nid = 1; nid < LibreSslObjectCount; ++nid) {
        if (IsUnassigned(nid) || nid == 393) continue;
        Require(OBJ_sn2nid_nid_postfix(OBJ_nid2sn_nid_postfix(nid)) == nid, "every assigned short name maps back to its NID");
    }
    RequireThrows([] { OBJ_sn2nid_nid_postfix(nullptr); }, "a null name");
}

void CheckAdded() {
    const int added = OBJ_create_nid_postfix("1.2.3.4.5", "addedShort", "added long");
    Require(added == LibreSslObjectCount, "the first added object takes NID 956");
    CheckName(added, "addedShort", "an added object is found by NID and by short name");
    const int shadow = OBJ_create_nid_postfix("1.2.3.4.6", "CN", nullptr);
    Require(OBJ_sn2nid_nid_postfix("CN") == shadow && Equal(OBJ_nid2sn_nid_postfix(13), "CN"), "added objects are searched before the built-in table");
    const int renamed = OBJ_create_nid_postfix("1.2.3.4.7", "addedShort", nullptr);
    Require(OBJ_sn2nid_nid_postfix("addedShort") == renamed && Equal(OBJ_nid2sn_nid_postfix(added), "addedShort"), "the newest added object owns a reused short name");
    const int unnamed = OBJ_create_nid_postfix("1.2.3.4.8", nullptr, "long only");
    ERR_clear_error_nid_postfix();
    Require(OBJ_nid2sn_nid_postfix(unnamed) == nullptr && ERR_get_error_nid_postfix() == 0, "an added object without a short name");
    CheckUnknownNid(unnamed + 1, "an unused NID past the table reports OBJ_R_UNKNOWN_NID once objects are added");
    CheckUnknownNid(-1, "a negative NID reports OBJ_R_UNKNOWN_NID once objects are added");
    OBJ_cleanup_nid_postfix();
    Require(OBJ_sn2nid_nid_postfix("CN") == 13 && OBJ_sn2nid_nid_postfix("addedShort") == 0, "OBJ_cleanup removes the added names");
    ERR_clear_error_nid_postfix();
    Require(OBJ_nid2sn_nid_postfix(added) == nullptr && ERR_get_error_nid_postfix() == 0, "OBJ_cleanup removes the added NIDs");
}

LibreSslAsn1Object Unnamed(const unsigned char* data, int length) {
    return {nullptr, nullptr, 0, length, data, 0};
}

void CheckObjectNid() {
    Require(OBJ_obj2nid_nid_postfix(nullptr) == 0, "a null object is NID_undef");
    constexpr unsigned char sha256[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01};
    constexpr unsigned char unknown[] = {0x2a, 0x03, 0x04, 0x63};
    constexpr unsigned char added[] = {0x2a, 0x03, 0x04, 0x05};
    LibreSslAsn1Object object = Unnamed(sha256, sizeof(sha256));
    Require(OBJ_obj2nid_nid_postfix(&object) == 672, "an unnamed object is found by its encoding");
    object.nid = 4;
    Require(OBJ_obj2nid_nid_postfix(&object) == 4, "a set NID is returned without looking at the encoding");
    LibreSslAsn1Object nidOnly{nullptr, nullptr, 13, 5, nullptr, 0};
    Require(OBJ_obj2nid_nid_postfix(&nidOnly) == 13, "a set NID is returned for an object without data");
    object = Unnamed(sha256, sizeof(sha256) - 1);
    Require(OBJ_obj2nid_nid_postfix(&object) == 0, "a truncated encoding is NID_undef");
    object = Unnamed(unknown, sizeof(unknown));
    Require(OBJ_obj2nid_nid_postfix(&object) == 0, "an unknown encoding is NID_undef");
    object = Unnamed(nullptr, 0);
    Require(OBJ_obj2nid_nid_postfix(&object) == 404, "an empty object lands on the zero-length ccitt entry, as the reference binary search does");
    object = Unnamed(sha256, -1);
    Require(OBJ_obj2nid_nid_postfix(&object) == 0, "a negative length is NID_undef");
    object = Unnamed(nullptr, 3);
    RequireThrows([&] { OBJ_obj2nid_nid_postfix(&object); }, "an unnamed object with a length and no data");
    for (int nid = 1; nid < LibreSslObjectCount; ++nid) {
        const LibreSslAsn1Object& entry = LibreSslObjects_nid_no_patch[nid];
        if (entry.length <= 0) continue;
        object = Unnamed(entry.data, entry.length);
        const int found = OBJ_obj2nid_nid_postfix(&object);
        const LibreSslAsn1Object& match = LibreSslObjects_nid_no_patch[found];
        Require(found > 0 && match.length == entry.length && std::memcmp(match.data, entry.data, static_cast<std::size_t>(entry.length)) == 0,
            "every built-in encoding maps to a NID with that encoding");
    }
    object = Unnamed(added, sizeof(added));
    Require(OBJ_obj2nid_nid_postfix(&object) == 0, "an encoding is unknown before OBJ_create");
    const int created = OBJ_create_nid_postfix("1.2.3.4.5", "objectNidShort", nullptr);
    Require(created > 0 && OBJ_obj2nid_nid_postfix(&object) == created, "an encoding registered by OBJ_create maps to its NID");
    const int shadow = OBJ_create_nid_postfix("2.16.840.1.101.3.4.2.1", "shadowSha256", nullptr);
    object = Unnamed(sha256, sizeof(sha256));
    Require(shadow > 0 && OBJ_obj2nid_nid_postfix(&object) == shadow, "added objects are searched before the built-in table");
    OBJ_cleanup_nid_postfix();
    Require(OBJ_obj2nid_nid_postfix(&object) == 672, "OBJ_cleanup restores the built-in match");
    object = Unnamed(added, sizeof(added));
    Require(OBJ_obj2nid_nid_postfix(&object) == 0, "OBJ_cleanup removes added encodings");
}

} // namespace

int main() {
    CheckBuiltIn();
    CheckAdded();
    CheckObjectNid();
    return 0;
}
