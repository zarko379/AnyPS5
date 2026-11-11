#include "prx/libSceIcu/Iterator.hpp"
#include <cstdint>

void* GuestStringIterator(const char16_t* text, std::int32_t length) {
    auto* iterator = new UCharIterator{};
    uiter_setString_nid_postfix(iterator, text, length);
    return iterator;
}

void* GuestUtf8Iterator(const char* text, std::int32_t length) {
    auto* iterator = new UCharIterator{};
    uiter_setUTF8_nid_postfix(iterator, text, length);
    return iterator;
}

std::int32_t GuestIteratorIndex(const void* iterator) {
    return static_cast<const UCharIterator*>(iterator)->index;
}

void FreeGuestIterator(void* iterator) {
    delete static_cast<UCharIterator*>(iterator);
}
