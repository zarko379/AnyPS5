#include "prx/libSceNKWebKit/include/EventExports.hpp"

namespace {

constexpr std::uint8_t InputTypeSetComposition = 1;
constexpr std::uint8_t InputTypeConfirmComposition = 2;
constexpr std::uint8_t InputTypeCancelComposition = 3;

}  // namespace

extern "C" {

NKWebKit::KeyboardEvent APS5_VABI WKKeyboardEventMake(std::int32_t type, std::uint8_t inputType, const char* text, std::uint32_t length,
    const char* keyIdentifier, std::int32_t virtualKeyCode, std::int32_t caretOffset, std::uint32_t, std::uint32_t modifiers) {
    NKWebKit::KeyboardEvent keyboardEvent{};
    keyboardEvent.type = type;
    keyboardEvent.virtualKeyCode = virtualKeyCode;
    keyboardEvent.modifiers = modifiers;
    keyboardEvent.caretOffset = caretOffset;
    if (inputType == InputTypeSetComposition)
        keyboardEvent.keyIdentifier = "Convert";
    else if (inputType == InputTypeConfirmComposition)
        keyboardEvent.keyIdentifier = "Accept";
    else if (inputType == InputTypeCancelComposition)
        keyboardEvent.keyIdentifier = "Cancel";
    else
        keyboardEvent.keyIdentifier = keyIdentifier;
    if (length > 0) {
        keyboardEvent.text = text;
        keyboardEvent.length = static_cast<std::int32_t>(length);
    } else {
        keyboardEvent.text = nullptr;
        keyboardEvent.length = 0;
    }
    return keyboardEvent;
}

NKWebKit::MouseEvent APS5_VABI WKMouseEventMake(std::int32_t type, std::int32_t button, NKWebKit::PointValue position, std::int32_t clickCount,
    std::uint32_t modifiers) {
    NKWebKit::MouseEvent mouseEvent{};
    mouseEvent.type = type;
    mouseEvent.button = button;
    mouseEvent.position = position;
    mouseEvent.clickCount = clickCount;
    mouseEvent.modifiers = modifiers;
    return mouseEvent;
}

NKWebKit::WheelEvent APS5_VABI WKWheelEventMake(std::int32_t type, NKWebKit::PointValue position, NKWebKit::SizeValue delta,
    NKWebKit::SizeValue wheelTicks, std::uint32_t modifiers) {
    NKWebKit::WheelEvent wheelEvent{};
    wheelEvent.type = type;
    wheelEvent.position = position;
    wheelEvent.delta = delta;
    wheelEvent.wheelTicks = wheelTicks;
    wheelEvent.modifiers = modifiers;
    return wheelEvent;
}

}
