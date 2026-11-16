// Derived from icu4c/source/common/bytestrie.cpp, ucharstrie.cpp, unicode/bytestrie.h and unicode/ucharstrie.h
// @84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/bytestrie.cpp
//
// The upstream files' notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2010-2011, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "StringTries.hpp"
#include "Configuration.hpp"
#include "prx/libc/include/General.hpp"
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t NoMatch = 0;
constexpr std::int32_t NoValue = 1;
constexpr std::int32_t FinalValue = 2;
constexpr std::int32_t IntermediateValue = 3;

template<typename TObject>
TObject* Object(TObject* object) {
    if (!object) APS5_INVALID_ARG_EX;
    return object;
}

namespace Bytes {

using Icu::BytesTrie;

constexpr std::int32_t MaxBranchLinearSubNodeLength = 5;
constexpr std::int32_t MinLinearMatch = 0x10;
constexpr std::int32_t MinValueLead = 0x20;
constexpr std::int32_t ValueIsFinal = 1;
constexpr std::int32_t MinOneByteValueLead = 0x10;
constexpr std::int32_t MinTwoByteValueLead = 0x51;
constexpr std::int32_t MinThreeByteValueLead = 0x6c;
constexpr std::int32_t FourByteValueLead = 0x7e;
constexpr std::int32_t MinTwoByteDeltaLead = 0xc0;
constexpr std::int32_t MinThreeByteDeltaLead = 0xf0;
constexpr std::int32_t FourByteDeltaLead = 0xfe;

std::int32_t ValueResult(std::int32_t node) {
    return IntermediateValue - (node & ValueIsFinal);
}

std::int32_t ReadValue(const std::uint8_t* pos, std::int32_t leadByte) {
    if (leadByte < MinTwoByteValueLead) return leadByte - MinOneByteValueLead;
    if (leadByte < MinThreeByteValueLead) return ((leadByte - MinTwoByteValueLead) << 8) | *pos;
    if (leadByte < FourByteValueLead) return ((leadByte - MinThreeByteValueLead) << 16) | (pos[0] << 8) | pos[1];
    if (leadByte == FourByteValueLead) return (pos[0] << 16) | (pos[1] << 8) | pos[2];
    return static_cast<std::int32_t>((static_cast<std::uint32_t>(pos[0]) << 24) | (pos[1] << 16) | (pos[2] << 8) | pos[3]);
}

const std::uint8_t* SkipValue(const std::uint8_t* pos, std::int32_t leadByte) {
    if (leadByte >= (MinTwoByteValueLead << 1)) {
        if (leadByte < (MinThreeByteValueLead << 1)) ++pos;
        else if (leadByte < (FourByteValueLead << 1)) pos += 2;
        else pos += 3 + ((leadByte >> 1) & 1);
    }
    return pos;
}

const std::uint8_t* SkipValue(const std::uint8_t* pos) {
    const std::int32_t leadByte = *pos++;
    return SkipValue(pos, leadByte);
}

const std::uint8_t* JumpByDelta(const std::uint8_t* pos) {
    std::int32_t delta = *pos++;
    if (delta < MinTwoByteDeltaLead) {
    } else if (delta < MinThreeByteDeltaLead) {
        delta = ((delta - MinTwoByteDeltaLead) << 8) | *pos++;
    } else if (delta < FourByteDeltaLead) {
        delta = ((delta - MinThreeByteDeltaLead) << 16) | (pos[0] << 8) | pos[1];
        pos += 2;
    } else if (delta == FourByteDeltaLead) {
        delta = (pos[0] << 16) | (pos[1] << 8) | pos[2];
        pos += 3;
    } else {
        delta = static_cast<std::int32_t>((static_cast<std::uint32_t>(pos[0]) << 24) | (pos[1] << 16) | (pos[2] << 8) | pos[3]);
        pos += 4;
    }
    return pos + delta;
}

const std::uint8_t* SkipDelta(const std::uint8_t* pos) {
    const std::int32_t delta = *pos++;
    if (delta >= MinTwoByteDeltaLead) {
        if (delta < MinThreeByteDeltaLead) ++pos;
        else if (delta < FourByteDeltaLead) pos += 2;
        else pos += 3 + (delta & 1);
    }
    return pos;
}

std::int32_t Current(const BytesTrie* trie) {
    const std::uint8_t* pos = trie->pos;
    if (!pos) return NoMatch;
    std::int32_t node;
    return (trie->remainingMatchLength < 0 && (node = *pos) >= MinValueLead) ? ValueResult(node) : NoValue;
}

std::int32_t BranchNext(BytesTrie* trie, const std::uint8_t* pos, std::int32_t length, std::int32_t inByte) {
    if (length == 0) length = *pos++;
    ++length;
    while (length > MaxBranchLinearSubNodeLength) {
        if (inByte < *pos++) {
            length >>= 1;
            pos = JumpByDelta(pos);
        } else {
            length = length - (length >> 1);
            pos = SkipDelta(pos);
        }
    }
    do {
        if (inByte == *pos++) {
            std::int32_t result;
            std::int32_t node = *pos;
            if (node & ValueIsFinal) {
                result = FinalValue;
            } else {
                ++pos;
                node >>= 1;
                std::int32_t delta;
                if (node < MinTwoByteValueLead) {
                    delta = node - MinOneByteValueLead;
                } else if (node < MinThreeByteValueLead) {
                    delta = ((node - MinTwoByteValueLead) << 8) | *pos++;
                } else if (node < FourByteValueLead) {
                    delta = ((node - MinThreeByteValueLead) << 16) | (pos[0] << 8) | pos[1];
                    pos += 2;
                } else if (node == FourByteValueLead) {
                    delta = (pos[0] << 16) | (pos[1] << 8) | pos[2];
                    pos += 3;
                } else {
                    delta = static_cast<std::int32_t>((static_cast<std::uint32_t>(pos[0]) << 24) | (pos[1] << 16) | (pos[2] << 8) | pos[3]);
                    pos += 4;
                }
                pos += delta;
                node = *pos;
                result = node >= MinValueLead ? ValueResult(node) : NoValue;
            }
            trie->pos = pos;
            return result;
        }
        --length;
        pos = SkipValue(pos);
    } while (length > 1);
    if (inByte == *pos++) {
        trie->pos = pos;
        const std::int32_t node = *pos;
        return node >= MinValueLead ? ValueResult(node) : NoValue;
    }
    trie->pos = nullptr;
    return NoMatch;
}

std::int32_t NextImpl(BytesTrie* trie, const std::uint8_t* pos, std::int32_t inByte) {
    for (;;) {
        std::int32_t node = *pos++;
        if (node < MinLinearMatch) {
            return BranchNext(trie, pos, node, inByte);
        } else if (node < MinValueLead) {
            std::int32_t length = node - MinLinearMatch;
            if (inByte == *pos++) {
                trie->remainingMatchLength = --length;
                trie->pos = pos;
                return (length < 0 && (node = *pos) >= MinValueLead) ? ValueResult(node) : NoValue;
            }
            break;
        } else if (node & ValueIsFinal) {
            break;
        } else {
            pos = SkipValue(pos, node);
        }
    }
    trie->pos = nullptr;
    return NoMatch;
}

std::int32_t Next(BytesTrie* trie, std::int32_t inByte) {
    const std::uint8_t* pos = trie->pos;
    if (!pos) return NoMatch;
    if (inByte < 0) inByte += 0x100;
    std::int32_t length = trie->remainingMatchLength;
    if (length >= 0) {
        if (inByte == *pos++) {
            trie->remainingMatchLength = --length;
            trie->pos = pos;
            std::int32_t node;
            return (length < 0 && (node = *pos) >= MinValueLead) ? ValueResult(node) : NoValue;
        }
        trie->pos = nullptr;
        return NoMatch;
    }
    return NextImpl(trie, pos, inByte);
}

std::int32_t NextString(BytesTrie* trie, const char* s, std::int32_t sLength) {
    if (sLength < 0 ? *s == 0 : sLength == 0) return Current(trie);
    const std::uint8_t* pos = trie->pos;
    if (!pos) return NoMatch;
    std::int32_t length = trie->remainingMatchLength;
    for (;;) {
        std::int32_t inByte;
        if (sLength < 0) {
            for (;;) {
                if ((inByte = *s++) == 0) {
                    trie->remainingMatchLength = length;
                    trie->pos = pos;
                    std::int32_t node;
                    return (length < 0 && (node = *pos) >= MinValueLead) ? ValueResult(node) : NoValue;
                }
                if (length < 0) {
                    trie->remainingMatchLength = length;
                    break;
                }
                if (inByte != *pos) {
                    trie->pos = nullptr;
                    return NoMatch;
                }
                ++pos;
                --length;
            }
        } else {
            for (;;) {
                if (sLength == 0) {
                    trie->remainingMatchLength = length;
                    trie->pos = pos;
                    std::int32_t node;
                    return (length < 0 && (node = *pos) >= MinValueLead) ? ValueResult(node) : NoValue;
                }
                inByte = *s++;
                --sLength;
                if (length < 0) {
                    trie->remainingMatchLength = length;
                    break;
                }
                if (inByte != *pos) {
                    trie->pos = nullptr;
                    return NoMatch;
                }
                ++pos;
                --length;
            }
        }
        for (;;) {
            const std::int32_t node = *pos++;
            if (node < MinLinearMatch) {
                const std::int32_t result = BranchNext(trie, pos, node, inByte);
                if (result == NoMatch) return NoMatch;
                if (sLength < 0) {
                    if ((inByte = *s++) == 0) return result;
                } else {
                    if (sLength == 0) return result;
                    inByte = *s++;
                    --sLength;
                }
                if (result == FinalValue) {
                    trie->pos = nullptr;
                    return NoMatch;
                }
                pos = trie->pos;
            } else if (node < MinValueLead) {
                length = node - MinLinearMatch;
                if (inByte != *pos) {
                    trie->pos = nullptr;
                    return NoMatch;
                }
                ++pos;
                --length;
                break;
            } else if (node & ValueIsFinal) {
                trie->pos = nullptr;
                return NoMatch;
            } else {
                pos = SkipValue(pos, node);
            }
        }
    }
}

bool FindUniqueValue(const std::uint8_t* pos, bool haveUniqueValue, std::int32_t& uniqueValue);

const std::uint8_t* FindUniqueValueFromBranch(const std::uint8_t* pos, std::int32_t length, bool haveUniqueValue, std::int32_t& uniqueValue) {
    while (length > MaxBranchLinearSubNodeLength) {
        ++pos;
        if (!FindUniqueValueFromBranch(JumpByDelta(pos), length >> 1, haveUniqueValue, uniqueValue)) return nullptr;
        length = length - (length >> 1);
        pos = SkipDelta(pos);
    }
    do {
        ++pos;
        const std::int32_t node = *pos++;
        const bool isFinal = (node & ValueIsFinal) != 0;
        const std::int32_t value = ReadValue(pos, node >> 1);
        pos = SkipValue(pos, node);
        if (isFinal) {
            if (haveUniqueValue) {
                if (value != uniqueValue) return nullptr;
            } else {
                uniqueValue = value;
                haveUniqueValue = true;
            }
        } else {
            if (!FindUniqueValue(pos + value, haveUniqueValue, uniqueValue)) return nullptr;
            haveUniqueValue = true;
        }
    } while (--length > 1);
    return pos + 1;
}

bool FindUniqueValue(const std::uint8_t* pos, bool haveUniqueValue, std::int32_t& uniqueValue) {
    for (;;) {
        std::int32_t node = *pos++;
        if (node < MinLinearMatch) {
            if (node == 0) node = *pos++;
            pos = FindUniqueValueFromBranch(pos, node + 1, haveUniqueValue, uniqueValue);
            if (!pos) return false;
            haveUniqueValue = true;
        } else if (node < MinValueLead) {
            pos += node - MinLinearMatch + 1;
        } else {
            const bool isFinal = (node & ValueIsFinal) != 0;
            const std::int32_t value = ReadValue(pos, node >> 1);
            if (haveUniqueValue) {
                if (value != uniqueValue) return false;
            } else {
                uniqueValue = value;
                haveUniqueValue = true;
            }
            if (isFinal) return true;
            pos = SkipValue(pos, node);
        }
    }
}

} // namespace Bytes

namespace Units {

using Icu::UCharsTrie;

constexpr std::int32_t MaxBranchLinearSubNodeLength = 5;
constexpr std::int32_t MinLinearMatch = 0x30;
constexpr std::int32_t MinValueLead = 0x40;
constexpr std::int32_t NodeTypeMask = 0x3f;
constexpr std::int32_t ValueIsFinal = 0x8000;
constexpr std::int32_t MinTwoUnitValueLead = 0x4000;
constexpr std::int32_t ThreeUnitValueLead = 0x7fff;
constexpr std::int32_t MinTwoUnitNodeValueLead = 0x4040;
constexpr std::int32_t ThreeUnitNodeValueLead = 0x7fc0;
constexpr std::int32_t MinTwoUnitDeltaLead = 0xfc00;
constexpr std::int32_t ThreeUnitDeltaLead = 0xffff;

std::int32_t ValueResult(std::int32_t node) {
    return IntermediateValue - (node >> 15);
}

std::int32_t ReadValue(const char16_t* pos, std::int32_t leadUnit) {
    if (leadUnit < MinTwoUnitValueLead) return leadUnit;
    if (leadUnit < ThreeUnitValueLead) return ((leadUnit - MinTwoUnitValueLead) << 16) | pos[0];
    return (pos[0] << 16) | pos[1];
}

const char16_t* SkipValue(const char16_t* pos, std::int32_t leadUnit) {
    if (leadUnit >= MinTwoUnitValueLead) {
        if (leadUnit < ThreeUnitValueLead) ++pos;
        else pos += 2;
    }
    return pos;
}

const char16_t* SkipValue(const char16_t* pos) {
    const std::int32_t leadUnit = *pos++;
    return SkipValue(pos, leadUnit & 0x7fff);
}

std::int32_t ReadNodeValue(const char16_t* pos, std::int32_t leadUnit) {
    if (leadUnit < MinTwoUnitNodeValueLead) return (leadUnit >> 6) - 1;
    if (leadUnit < ThreeUnitNodeValueLead) return (((leadUnit & 0x7fc0) - MinTwoUnitNodeValueLead) << 10) | *pos;
    return (pos[0] << 16) | pos[1];
}

const char16_t* SkipNodeValue(const char16_t* pos, std::int32_t leadUnit) {
    if (leadUnit >= MinTwoUnitNodeValueLead) {
        if (leadUnit < ThreeUnitNodeValueLead) ++pos;
        else pos += 2;
    }
    return pos;
}

const char16_t* JumpByDelta(const char16_t* pos) {
    std::int32_t delta = *pos++;
    if (delta >= MinTwoUnitDeltaLead) {
        if (delta == ThreeUnitDeltaLead) {
            delta = (pos[0] << 16) | pos[1];
            pos += 2;
        } else {
            delta = ((delta - MinTwoUnitDeltaLead) << 16) | *pos++;
        }
    }
    return pos + delta;
}

const char16_t* SkipDelta(const char16_t* pos) {
    const std::int32_t delta = *pos++;
    if (delta >= MinTwoUnitDeltaLead) {
        if (delta == ThreeUnitDeltaLead) pos += 2;
        else ++pos;
    }
    return pos;
}

std::int32_t Current(const UCharsTrie* trie) {
    const char16_t* pos = trie->pos;
    if (!pos) return NoMatch;
    std::int32_t node;
    return (trie->remainingMatchLength < 0 && (node = *pos) >= MinValueLead) ? ValueResult(node) : NoValue;
}

std::int32_t BranchNext(UCharsTrie* trie, const char16_t* pos, std::int32_t length, std::int32_t uchar) {
    if (length == 0) length = *pos++;
    ++length;
    while (length > MaxBranchLinearSubNodeLength) {
        if (uchar < *pos++) {
            length >>= 1;
            pos = JumpByDelta(pos);
        } else {
            length = length - (length >> 1);
            pos = SkipDelta(pos);
        }
    }
    do {
        if (uchar == *pos++) {
            std::int32_t result;
            std::int32_t node = *pos;
            if (node & ValueIsFinal) {
                result = FinalValue;
            } else {
                ++pos;
                std::int32_t delta;
                if (node < MinTwoUnitValueLead) {
                    delta = node;
                } else if (node < ThreeUnitValueLead) {
                    delta = ((node - MinTwoUnitValueLead) << 16) | *pos++;
                } else {
                    delta = (pos[0] << 16) | pos[1];
                    pos += 2;
                }
                pos += delta;
                node = *pos;
                result = node >= MinValueLead ? ValueResult(node) : NoValue;
            }
            trie->pos = pos;
            return result;
        }
        --length;
        pos = SkipValue(pos);
    } while (length > 1);
    if (uchar == *pos++) {
        trie->pos = pos;
        const std::int32_t node = *pos;
        return node >= MinValueLead ? ValueResult(node) : NoValue;
    }
    trie->pos = nullptr;
    return NoMatch;
}

std::int32_t NextImpl(UCharsTrie* trie, const char16_t* pos, std::int32_t uchar) {
    std::int32_t node = *pos++;
    for (;;) {
        if (node < MinLinearMatch) {
            return BranchNext(trie, pos, node, uchar);
        } else if (node < MinValueLead) {
            std::int32_t length = node - MinLinearMatch;
            if (uchar == *pos++) {
                trie->remainingMatchLength = --length;
                trie->pos = pos;
                return (length < 0 && (node = *pos) >= MinValueLead) ? ValueResult(node) : NoValue;
            }
            break;
        } else if (node & ValueIsFinal) {
            break;
        } else {
            pos = SkipNodeValue(pos, node);
            node &= NodeTypeMask;
        }
    }
    trie->pos = nullptr;
    return NoMatch;
}

std::int32_t Next(UCharsTrie* trie, std::int32_t uchar) {
    const char16_t* pos = trie->pos;
    if (!pos) return NoMatch;
    std::int32_t length = trie->remainingMatchLength;
    if (length >= 0) {
        if (uchar == *pos++) {
            trie->remainingMatchLength = --length;
            trie->pos = pos;
            std::int32_t node;
            return (length < 0 && (node = *pos) >= MinValueLead) ? ValueResult(node) : NoValue;
        }
        trie->pos = nullptr;
        return NoMatch;
    }
    return NextImpl(trie, pos, uchar);
}

std::int32_t First(UCharsTrie* trie, std::int32_t uchar) {
    trie->remainingMatchLength = -1;
    if (!trie->uchars) APS5_INVALID_ARG_EX;
    return NextImpl(trie, trie->uchars, uchar);
}

std::int32_t NextString(UCharsTrie* trie, const char16_t* s, std::int32_t sLength) {
    if (sLength < 0 ? *s == 0 : sLength == 0) return Current(trie);
    const char16_t* pos = trie->pos;
    if (!pos) return NoMatch;
    std::int32_t length = trie->remainingMatchLength;
    for (;;) {
        std::int32_t uchar;
        if (sLength < 0) {
            for (;;) {
                if ((uchar = *s++) == 0) {
                    trie->remainingMatchLength = length;
                    trie->pos = pos;
                    std::int32_t node;
                    return (length < 0 && (node = *pos) >= MinValueLead) ? ValueResult(node) : NoValue;
                }
                if (length < 0) {
                    trie->remainingMatchLength = length;
                    break;
                }
                if (uchar != *pos) {
                    trie->pos = nullptr;
                    return NoMatch;
                }
                ++pos;
                --length;
            }
        } else {
            for (;;) {
                if (sLength == 0) {
                    trie->remainingMatchLength = length;
                    trie->pos = pos;
                    std::int32_t node;
                    return (length < 0 && (node = *pos) >= MinValueLead) ? ValueResult(node) : NoValue;
                }
                uchar = *s++;
                --sLength;
                if (length < 0) {
                    trie->remainingMatchLength = length;
                    break;
                }
                if (uchar != *pos) {
                    trie->pos = nullptr;
                    return NoMatch;
                }
                ++pos;
                --length;
            }
        }
        std::int32_t node = *pos++;
        for (;;) {
            if (node < MinLinearMatch) {
                const std::int32_t result = BranchNext(trie, pos, node, uchar);
                if (result == NoMatch) return NoMatch;
                if (sLength < 0) {
                    if ((uchar = *s++) == 0) return result;
                } else {
                    if (sLength == 0) return result;
                    uchar = *s++;
                    --sLength;
                }
                if (result == FinalValue) {
                    trie->pos = nullptr;
                    return NoMatch;
                }
                pos = trie->pos;
                node = *pos++;
            } else if (node < MinValueLead) {
                length = node - MinLinearMatch;
                if (uchar != *pos) {
                    trie->pos = nullptr;
                    return NoMatch;
                }
                ++pos;
                --length;
                break;
            } else if (node & ValueIsFinal) {
                trie->pos = nullptr;
                return NoMatch;
            } else {
                pos = SkipNodeValue(pos, node);
                node &= NodeTypeMask;
            }
        }
    }
}

bool FindUniqueValue(const char16_t* pos, bool haveUniqueValue, std::int32_t& uniqueValue);

const char16_t* FindUniqueValueFromBranch(const char16_t* pos, std::int32_t length, bool haveUniqueValue, std::int32_t& uniqueValue) {
    while (length > MaxBranchLinearSubNodeLength) {
        ++pos;
        if (!FindUniqueValueFromBranch(JumpByDelta(pos), length >> 1, haveUniqueValue, uniqueValue)) return nullptr;
        length = length - (length >> 1);
        pos = SkipDelta(pos);
    }
    do {
        ++pos;
        std::int32_t node = *pos++;
        const bool isFinal = (node >> 15) != 0;
        node &= 0x7fff;
        const std::int32_t value = ReadValue(pos, node);
        pos = SkipValue(pos, node);
        if (isFinal) {
            if (haveUniqueValue) {
                if (value != uniqueValue) return nullptr;
            } else {
                uniqueValue = value;
                haveUniqueValue = true;
            }
        } else {
            if (!FindUniqueValue(pos + value, haveUniqueValue, uniqueValue)) return nullptr;
            haveUniqueValue = true;
        }
    } while (--length > 1);
    return pos + 1;
}

bool FindUniqueValue(const char16_t* pos, bool haveUniqueValue, std::int32_t& uniqueValue) {
    std::int32_t node = *pos++;
    for (;;) {
        if (node < MinLinearMatch) {
            if (node == 0) node = *pos++;
            pos = FindUniqueValueFromBranch(pos, node + 1, haveUniqueValue, uniqueValue);
            if (!pos) return false;
            haveUniqueValue = true;
            node = *pos++;
        } else if (node < MinValueLead) {
            pos += node - MinLinearMatch + 1;
            node = *pos++;
        } else {
            const bool isFinal = (node >> 15) != 0;
            const std::int32_t value = isFinal ? ReadValue(pos, node & 0x7fff) : ReadNodeValue(pos, node);
            if (haveUniqueValue) {
                if (value != uniqueValue) return false;
            } else {
                uniqueValue = value;
                haveUniqueValue = true;
            }
            if (isFinal) return true;
            pos = SkipNodeValue(pos, node);
            node &= NodeTypeMask;
        }
    }
}

std::int32_t Lead(std::int32_t cp) {
    return (cp >> 10) + 0xd7c0;
}

std::int32_t Trail(std::int32_t cp) {
    return (cp & 0x3ff) | 0xdc00;
}

} // namespace Units

template<typename TPointer>
TPointer Data(TPointer pointer) {
    if (!pointer) APS5_INVALID_ARG_EX;
    return pointer;
}

} // namespace

extern "C" {

void APS5_VABI _ZN6icu_689BytesTrieD1Ev_nid_postfix(Icu::BytesTrie* self) {
    if (Object(self)->ownedArray) IcuReleaseMemory_nid_no_patch(self->ownedArray);
}

void APS5_VABI _ZN6icu_689BytesTrieD2Ev_nid_postfix(Icu::BytesTrie* self) {
    _ZN6icu_689BytesTrieD1Ev_nid_postfix(self);
}

std::int32_t APS5_VABI _ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(const std::uint8_t* pos, std::int32_t leadByte) {
    if (leadByte >= Bytes::MinTwoByteValueLead) Data(pos);
    return Bytes::ReadValue(pos, leadByte);
}

const std::uint8_t* APS5_VABI _ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(const std::uint8_t* pos) {
    return Bytes::JumpByDelta(Data(pos));
}

std::int32_t APS5_VABI _ZNK6icu_689BytesTrie7currentEv_nid_postfix(const Icu::BytesTrie* self) {
    return Bytes::Current(Object(self));
}

std::int32_t APS5_VABI _ZN6icu_689BytesTrie10branchNextEPKhii_nid_postfix(Icu::BytesTrie* self, const std::uint8_t* pos, std::int32_t length,
                                                                       std::int32_t inByte) {
    return Bytes::BranchNext(Object(self), Data(pos), length, inByte);
}

std::int32_t APS5_VABI _ZN6icu_689BytesTrie8nextImplEPKhi_nid_postfix(Icu::BytesTrie* self, const std::uint8_t* pos, std::int32_t inByte) {
    return Bytes::NextImpl(Object(self), Data(pos), inByte);
}

std::int32_t APS5_VABI _ZN6icu_689BytesTrie4nextEi_nid_postfix(Icu::BytesTrie* self, std::int32_t inByte) {
    return Bytes::Next(Object(self), inByte);
}

std::int32_t APS5_VABI _ZN6icu_689BytesTrie4nextEPKci_nid_postfix(Icu::BytesTrie* self, const char* s, std::int32_t sLength) {
    Object(self);
    if (sLength != 0) Data(s);
    return Bytes::NextString(self, s, sLength);
}

const std::uint8_t* APS5_VABI _ZN6icu_689BytesTrie25findUniqueValueFromBranchEPKhiaRi_nid_postfix(const std::uint8_t* pos, std::int32_t length,
                                                                                                std::int8_t haveUniqueValue, std::int32_t* uniqueValue) {
    return Bytes::FindUniqueValueFromBranch(Data(pos), length, haveUniqueValue != 0, *Object(uniqueValue));
}

std::int8_t APS5_VABI _ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(const std::uint8_t* pos, std::int8_t haveUniqueValue, std::int32_t* uniqueValue) {
    return Bytes::FindUniqueValue(Data(pos), haveUniqueValue != 0, *Object(uniqueValue));
}

void APS5_VABI _ZN6icu_6810UCharsTrieD1Ev_nid_postfix(Icu::UCharsTrie* self) {
    if (Object(self)->ownedArray) IcuReleaseMemory_nid_no_patch(self->ownedArray);
}

void APS5_VABI _ZN6icu_6810UCharsTrieD2Ev_nid_postfix(Icu::UCharsTrie* self) {
    _ZN6icu_6810UCharsTrieD1Ev_nid_postfix(self);
}

std::int32_t APS5_VABI _ZNK6icu_6810UCharsTrie7currentEv_nid_postfix(const Icu::UCharsTrie* self) {
    return Units::Current(Object(self));
}

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(Icu::UCharsTrie* self, std::int32_t cp) {
    Object(self);
    if (cp <= 0xffff) return Units::First(self, cp);
    return (Units::First(self, Units::Lead(cp)) & 1) ? Units::Next(self, Units::Trail(cp)) : NoMatch;
}

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie16nextForCodePointEi_nid_postfix(Icu::UCharsTrie* self, std::int32_t cp) {
    Object(self);
    if (cp <= 0xffff) return Units::Next(self, cp);
    return (Units::Next(self, Units::Lead(cp)) & 1) ? Units::Next(self, Units::Trail(cp)) : NoMatch;
}

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie10branchNextEPKDsii_nid_postfix(Icu::UCharsTrie* self, const char16_t* pos, std::int32_t length,
                                                                          std::int32_t uchar) {
    return Units::BranchNext(Object(self), Data(pos), length, uchar);
}

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie8nextImplEPKDsi_nid_postfix(Icu::UCharsTrie* self, const char16_t* pos, std::int32_t uchar) {
    return Units::NextImpl(Object(self), Data(pos), uchar);
}

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie4nextEi_nid_postfix(Icu::UCharsTrie* self, std::int32_t uchar) {
    return Units::Next(Object(self), uchar);
}

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(Icu::UCharsTrie* self, const char16_t* const* text, std::int32_t sLength) {
    Object(self);
    const char16_t* s = *Object(text);
    if (sLength != 0) Data(s);
    return Units::NextString(self, s, sLength);
}

const char16_t* APS5_VABI _ZN6icu_6810UCharsTrie25findUniqueValueFromBranchEPKDsiaRi_nid_postfix(const char16_t* pos, std::int32_t length,
                                                                                               std::int8_t haveUniqueValue, std::int32_t* uniqueValue) {
    return Units::FindUniqueValueFromBranch(Data(pos), length, haveUniqueValue != 0, *Object(uniqueValue));
}

std::int8_t APS5_VABI _ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(const char16_t* pos, std::int8_t haveUniqueValue, std::int32_t* uniqueValue) {
    return Units::FindUniqueValue(Data(pos), haveUniqueValue != 0, *Object(uniqueValue));
}

}
