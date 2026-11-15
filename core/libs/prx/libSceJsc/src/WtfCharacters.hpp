#pragma once

namespace Jsc::WtfAbi {

inline bool IsASCIISpace(char16_t character) {
    return character <= ' ' && (character == ' ' || (character <= 0xD && character >= 0x9));
}

inline bool IsSpaceOrNewline(char16_t character) {
    if (character <= 0xFF) return IsASCIISpace(character);
    return character == 0x1680 || (character >= 0x2000 && character <= 0x200A) || character == 0x2028 || character == 0x205F || character == 0x3000;
}

} // namespace Jsc::WtfAbi
