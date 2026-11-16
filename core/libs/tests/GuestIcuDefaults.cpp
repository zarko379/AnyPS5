#include "prx/libSceIcu/Defaults.hpp"
#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/CharacterProperties.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

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

std::string SystemDefault() {
    return uloc_getDefault();
}

std::string Country(const char* locale) {
    char buffer[16]{};
    std::int32_t status = Ok;
    uloc_getCountry(locale, buffer, sizeof(buffer), &status);
    Require(status <= Ok, "uloc_getCountry succeeds");
    return buffer;
}

bool NullLocaleFollows(const std::string& expected) {
    return Country(nullptr) == Country(expected.c_str()) && std::string(uloc_getISO3Language(nullptr)) == uloc_getISO3Language(expected.c_str());
}

void CheckLocale() {
    const std::string system = SystemDefault();
    Require(system == uloc_getDefault(), "the system default locale is returned before the guest sets one");
    std::int32_t status = IllegalArgument;
    uloc_setDefault_nid_postfix("fr_CA", &status);
    Require(status == IllegalArgument && system == uloc_getDefault(), "a failed status sets no default");
    status = Ok;
    uloc_setDefault_nid_postfix("fr_CA", &status);
    Require(status == Ok && std::strcmp(uloc_getDefault(), "fr_CA") == 0, "the guest's default locale is returned");
    Require(Country(nullptr) == "CA" && std::strcmp(uloc_getISO3Language(nullptr), "fra") == 0, "a null locale resolves to the guest's default locale");
    uloc_setDefault_nid_postfix("de-AT", &status);
    Require(std::strcmp(uloc_getDefault(), "de_AT") == 0, "a language tag default is canonicalized");
    status = IllegalArgument;
    uloc_setDefault_nid_postfix("ja_JP", &status);
    Require(status == IllegalArgument && std::strcmp(uloc_getDefault(), "de_AT") == 0, "a failed status leaves the default");
    status = Ok;
    uloc_setDefault_nid_postfix(nullptr, &status);
    Require(status == Ok && system == uloc_getDefault(), "a null default returns to the system default");
    Require(NullLocaleFollows(system), "a null locale resolves to the system default again");
    Require(Throws<std::invalid_argument>([] { uloc_setDefault_nid_postfix("en", nullptr); }), "a null status throws");
}

void CheckIDefault() {
    const std::string system = SystemDefault();
    for (const char* locale : {"i-default@x=y", "i-default_US@x=y", "i-default-x@a=b", "i-default.utf8", "I-DEFAULT@x=y", "i_default@x=y", "I-default_US"}) {
        Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; uloc_setDefault_nid_postfix(locale, &ok); }), "a new default that ICU canonicalizes through the old default throws without a guest default");
        Require(system == uloc_getDefault(), "the failed default sets nothing");
        std::int32_t status = Ok;
        uloc_setDefault_nid_postfix("fr_CA", &status);
        Require(Throws<std::runtime_error>([&] { std::int32_t ok = Ok; uloc_setDefault_nid_postfix(locale, &ok); }), "a new default that ICU canonicalizes through the old default throws with a guest default too");
        Require(std::strcmp(uloc_getDefault(), "fr_CA") == 0, "the guest default stays");
        status = IllegalArgument;
        uloc_setDefault_nid_postfix(locale, &status);
        Require(status == IllegalArgument && std::strcmp(uloc_getDefault(), "fr_CA") == 0, "a failed status returns before the locale");
        status = Ok;
        uloc_setDefault_nid_postfix(nullptr, &status);
    }
    std::int32_t status = Ok;
    uloc_setDefault_nid_postfix("i-default", &status);
    Require(status == Ok && std::strcmp(uloc_getDefault(), "en@x=i-default") == 0, "a plain i-default converts as a language tag");
    uloc_setDefault_nid_postfix("I-DEFAULT", &status);
    Require(status == Ok && std::strcmp(uloc_getDefault(), "en@x=i-default") == 0, "a plain i-default in capitals converts as a language tag too");
    uloc_setDefault_nid_postfix("i-defaults@x=y", &status);
    Require(status == Ok && std::strcmp(uloc_getDefault(), "i-defaults@x=y") == 0, "a longer language is not i-default");
    uloc_setDefault_nid_postfix("de_AT@collation=phonebook", &status);
    Require(status == Ok && std::strcmp(uloc_getDefault(), "de_AT@collation=phonebook") == 0, "an ordinary locale with keywords is unaffected");
    uloc_setDefault_nid_postfix(nullptr, &status);
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
    const std::string system = SystemDefault();
    std::int32_t status = Ok;
    uloc_setDefault_nid_postfix("it_IT", &status);
    ucnv_setDefaultName_nid_postfix("UTF-16BE");
    u_cleanup_nid_postfix();
    Require(system == uloc_getDefault(), "u_cleanup forgets the guest's default locale");
    Require(NullLocaleFollows(system), "a null locale resolves to the system default after u_cleanup");
    Require(Throws<std::runtime_error>([] { ucnv_getDefaultName_nid_postfix(); }), "u_cleanup forgets the guest's default converter");
    status = Ok;
    char buffer[48]{};
    Require(u_charFromName_nid_postfix(0, "LATIN CAPITAL LETTER A WITH RING ABOVE", &status) == 0xC5 && status == Ok && u_charName_nid_postfix(0xC5, 0, buffer, 48, &status) == 38 && status == Ok && std::strcmp(buffer, "LATIN CAPITAL LETTER A WITH RING ABOVE") == 0, "the embedded data loads again after u_cleanup");
}

}

int main() {
    CheckLocale();
    CheckIDefault();
    CheckConverter();
    CheckCleanup();
    return 0;
}
