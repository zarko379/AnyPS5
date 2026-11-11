#include "prx/libSceIcu/Defaults.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Normalization.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t IllegalArgument = 1;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "ICU DEFAULTS: %s\n", message);
        std::abort();
    }
}

template<typename TException, typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const TException&) {
        return true;
    }
    return false;
}

void CheckLocale() {
    Require(Throws<std::runtime_error>([] { uloc_getDefault_nid_postfix(); }), "the default locale throws before the guest sets one");
    std::int32_t status = IllegalArgument;
    uloc_setDefault_nid_postfix("fr_CA", &status);
    Require(status == IllegalArgument && Throws<std::runtime_error>([] { uloc_getDefault_nid_postfix(); }), "a failed status sets no default");
    status = Ok;
    uloc_setDefault_nid_postfix("fr_CA", &status);
    Require(status == Ok && std::strcmp(uloc_getDefault_nid_postfix(), "fr_CA") == 0, "the guest's default locale is returned");
    uloc_setDefault_nid_postfix("de-AT", &status);
    Require(std::strcmp(uloc_getDefault_nid_postfix(), "de_AT") == 0, "a language tag default is canonicalized");
    status = IllegalArgument;
    uloc_setDefault_nid_postfix("ja_JP", &status);
    Require(status == IllegalArgument && std::strcmp(uloc_getDefault_nid_postfix(), "de_AT") == 0, "a failed status leaves the default");
    status = Ok;
    uloc_setDefault_nid_postfix(nullptr, &status);
    Require(status == Ok && Throws<std::runtime_error>([] { uloc_getDefault_nid_postfix(); }), "a null default returns to the unknown console default");
    Require(Throws<std::invalid_argument>([] { uloc_setDefault_nid_postfix("en", nullptr); }), "a null status throws");
}

void CheckConverter() {
    Require(Throws<std::runtime_error>([] { ucnv_getDefaultName_nid_postfix(); }), "the default converter throws before the guest sets one");
    ucnv_setDefaultName_nid_postfix("no-such-converter");
    Require(Throws<std::runtime_error>([] { ucnv_getDefaultName_nid_postfix(); }), "an unknown first converter name sets no default");
    ucnv_setDefaultName_nid_postfix("ISO-8859-1");
    Require(std::strcmp(ucnv_getDefaultName_nid_postfix(), "ISO-8859-1") == 0, "the guest's default converter is returned");
    ucnv_setDefaultName_nid_postfix("no-such-converter");
    Require(std::strcmp(ucnv_getDefaultName_nid_postfix(), "ISO-8859-1") == 0, "an unknown converter name leaves the default");
    ucnv_setDefaultName_nid_postfix("utf8");
    Require(std::strcmp(ucnv_getDefaultName_nid_postfix(), "UTF-8") == 0, "the default converter name is canonical");
    ucnv_setDefaultName_nid_postfix(nullptr);
    Require(Throws<std::runtime_error>([] { ucnv_getDefaultName_nid_postfix(); }), "a null converter returns to the unknown console default");
}

void CheckCleanup() {
    std::int32_t status = Ok;
    uloc_setDefault_nid_postfix("it_IT", &status);
    ucnv_setDefaultName_nid_postfix("UTF-16BE");
    u_cleanup_nid_postfix();
    Require(Throws<std::runtime_error>([] { uloc_getDefault_nid_postfix(); }), "u_cleanup forgets the guest's default locale");
    Require(Throws<std::runtime_error>([] { ucnv_getDefaultName_nid_postfix(); }), "u_cleanup forgets the guest's default converter");
    const UNormalizer2* nfc = unorm2_getNFCInstance_nid_postfix(&status);
    char16_t buffer[4]{};
    Require(nfc && status == Ok && unorm2_normalize_nid_postfix(nfc, u"Å", 2, buffer, 4, &status) == 1 && buffer[0] == u'Å', "the embedded data loads again after u_cleanup");
}

}

int main() {
    CheckLocale();
    CheckConverter();
    CheckCleanup();
    return 0;
}
