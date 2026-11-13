#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cstddef>
#include <cstdint>

namespace NKWebKit {

struct KeyboardEvent {
    std::int32_t type;
    const char* text;
    std::int32_t length;
    const char* keyIdentifier;
    std::uint32_t modifiers;
    std::int32_t virtualKeyCode;
    std::int32_t caretOffset;
};

struct MouseEvent {
    std::int32_t type;
    std::int32_t button;
    PointValue position;
    std::int32_t clickCount;
    std::uint32_t modifiers;
};

struct WheelEvent {
    std::int32_t type;
    PointValue position;
    SizeValue delta;
    SizeValue wheelTicks;
    std::uint32_t modifiers;
};

static_assert(sizeof(PointValue) == 16);
static_assert(sizeof(SizeValue) == 16);
static_assert(sizeof(KeyboardEvent) == 48);
static_assert(offsetof(KeyboardEvent, type) == 0);
static_assert(offsetof(KeyboardEvent, text) == 8);
static_assert(offsetof(KeyboardEvent, length) == 16);
static_assert(offsetof(KeyboardEvent, keyIdentifier) == 24);
static_assert(offsetof(KeyboardEvent, modifiers) == 32);
static_assert(offsetof(KeyboardEvent, virtualKeyCode) == 36);
static_assert(offsetof(KeyboardEvent, caretOffset) == 40);
static_assert(sizeof(MouseEvent) == 32);
static_assert(offsetof(MouseEvent, type) == 0);
static_assert(offsetof(MouseEvent, button) == 4);
static_assert(offsetof(MouseEvent, position) == 8);
static_assert(offsetof(MouseEvent, clickCount) == 24);
static_assert(offsetof(MouseEvent, modifiers) == 28);
static_assert(sizeof(WheelEvent) == 64);
static_assert(offsetof(WheelEvent, type) == 0);
static_assert(offsetof(WheelEvent, position) == 8);
static_assert(offsetof(WheelEvent, delta) == 24);
static_assert(offsetof(WheelEvent, wheelTicks) == 40);
static_assert(offsetof(WheelEvent, modifiers) == 56);

}  // namespace NKWebKit

extern "C" {

NKWebKit::KeyboardEvent APS5_VABI WKKeyboardEventMake(std::int32_t type, std::uint8_t inputType, const char* text, std::uint32_t length,
    const char* keyIdentifier, std::int32_t virtualKeyCode, std::int32_t caretOffset, std::uint32_t attributes, std::uint32_t modifiers);
NKWebKit::MouseEvent APS5_VABI WKMouseEventMake(std::int32_t type, std::int32_t button, NKWebKit::PointValue position, std::int32_t clickCount,
    std::uint32_t modifiers);
NKWebKit::WheelEvent APS5_VABI WKWheelEventMake(std::int32_t type, NKWebKit::PointValue position, NKWebKit::SizeValue delta,
    NKWebKit::SizeValue wheelTicks, std::uint32_t modifiers);

}
