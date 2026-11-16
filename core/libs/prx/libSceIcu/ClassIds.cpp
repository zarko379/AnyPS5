#include "prx/libSceIcu/ClassIds.hpp"
#include "prx/libSceIcu/CoreClasses.hpp"
#include "prx/libSceIcu/ErrorName.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/caniter.h>
#include <unicode/locid.h>
#include <unicode/normlzr.h>
#include <unicode/parsepos.h>
#include <unicode/rbbi.h>
#include <unicode/resbund.h>
#include <unicode/schriter.h>
#include <unicode/uchriter.h>
#include <unicode/unifilt.h>
#include <unicode/unifunct.h>
#include <unicode/uniset.h>
#include <unicode/unistr.h>
#include <unicode/usetiter.h>
#include "serv.h"
#include "servloc.h"
#include "servnotf.h"
#include "ustrenum.h"
#include "util.h"
#include "uvector.h"
#include "uvectr32.h"
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

template<typename TObject>
TObject* Object(TObject* object) {
    if (!object) APS5_INVALID_ARG_EX;
    return object;
}

std::int32_t* Code(void* self) {
    return reinterpret_cast<std::int32_t*>(static_cast<char*>(Object(self)) + sizeof(void*));
}

} // namespace

extern "C" {

const void* APS5_VABI _ZN6icu_6810Normalizer16getStaticClassIDEv_nid_postfix() {
    return icu::Normalizer::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6810UnicodeSet16getStaticClassIDEv_nid_postfix() {
    return icu::UnicodeSet::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6813EventListener16getStaticClassIDEv_nid_postfix() {
    return icu::EventListener::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6813ICUServiceKey16getStaticClassIDEv_nid_postfix() {
    return icu::ICUServiceKey::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6813ParsePosition16getStaticClassIDEv_nid_postfix() {
    return icu::ParsePosition::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6813SimpleFactory16getStaticClassIDEv_nid_postfix() {
    return icu::SimpleFactory::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6813UnicodeFilter16getStaticClassIDEv_nid_postfix() {
    return icu::UnicodeFilter::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6813UnicodeString16getStaticClassIDEv_nid_postfix() {
    return icu::UnicodeString::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6814ResourceBundle16getStaticClassIDEv_nid_postfix() {
    return icu::ResourceBundle::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6814UnicodeFunctor16getStaticClassIDEv_nid_postfix() {
    return icu::UnicodeFunctor::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6815ServiceListener16getStaticClassIDEv_nid_postfix() {
    return icu::ServiceListener::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6816LocaleKeyFactory16getStaticClassIDEv_nid_postfix() {
    return icu::LocaleKeyFactory::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6817CanonicalIterator16getStaticClassIDEv_nid_postfix() {
    return icu::CanonicalIterator::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6818UStringEnumeration16getStaticClassIDEv_nid_postfix() {
    return icu::UStringEnumeration::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6818UnicodeSetIterator16getStaticClassIDEv_nid_postfix() {
    return icu::UnicodeSetIterator::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6822RuleBasedBreakIterator16getStaticClassIDEv_nid_postfix() {
    return icu::RuleBasedBreakIterator::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6822SimpleLocaleKeyFactory16getStaticClassIDEv_nid_postfix() {
    return icu::SimpleLocaleKeyFactory::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6822UCharCharacterIterator16getStaticClassIDEv_nid_postfix() {
    return icu::UCharCharacterIterator::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6823StringCharacterIterator16getStaticClassIDEv_nid_postfix() {
    return icu::StringCharacterIterator::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_6824ICUResourceBundleFactory16getStaticClassIDEv_nid_postfix() {
    return icu::ICUResourceBundleFactory::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_686Locale16getStaticClassIDEv_nid_postfix() {
    return icu::Locale::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_686UStack16getStaticClassIDEv_nid_postfix() {
    return icu::UStack::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_687UVector16getStaticClassIDEv_nid_postfix() {
    return icu::UVector::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_689LocaleKey16getStaticClassIDEv_nid_postfix() {
    return icu::LocaleKey::getStaticClassID();
}

const void* APS5_VABI _ZN6icu_689UVector3216getStaticClassIDEv_nid_postfix() {
    return icu::UVector32::getStaticClassID();
}

const void* APS5_VABI _ZNK6icu_6813ParsePosition17getDynamicClassIDEv_nid_postfix(const void* self) {
    Object(self);
    return icu::ParsePosition::getStaticClassID();
}

const void* APS5_VABI _ZNK6icu_686UStack17getDynamicClassIDEv_nid_postfix(const void* self) {
    Object(self);
    return icu::UStack::getStaticClassID();
}

std::int32_t APS5_VABI _ZN6icu_689ErrorCode5resetEv_nid_postfix(void* self) {
    std::int32_t* code = Code(self);
    const std::int32_t previous = *code;
    *code = 0;
    return previous;
}

void APS5_VABI _ZN6icu_689ErrorCodeD0Ev_nid_postfix(void* self) {
    _ZN6icu_687UMemorydlEPv_nid_postfix(Object(self));
}

void APS5_VABI _ZN6icu_689ErrorCodeD1Ev_nid_postfix(void* self) {
    Object(self);
}

void APS5_VABI _ZN6icu_689ErrorCodeD2Ev_nid_postfix(void* self) {
    Object(self);
}

void APS5_VABI _ZNK6icu_689ErrorCode13handleFailureEv_nid_postfix(const void* self) {
    Object(self);
}

const char* APS5_VABI _ZNK6icu_689ErrorCode9errorNameEv_nid_postfix(const void* self) {
    return u_errorName_nid_postfix(*Code(const_cast<void*>(self)));
}

std::int8_t APS5_VABI _ZN6icu_6811ICU_Utility13isUnprintableEi_nid_postfix(std::int32_t c) {
    return icu::ICU_Utility::isUnprintable(c);
}

}
