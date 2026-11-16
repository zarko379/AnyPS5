// Derived from icu4c/source/common/edits.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/edits.cpp
//
// The upstream file's notice, verbatim:
//   © 2017 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html

#include "Edits.hpp"
#include "Configuration.hpp"
#include "prx/libc/include/General.hpp"
#include <climits>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

using Icu::Edits;
using Icu::EditsIterator;

constexpr std::int32_t StackCapacity = 100;
constexpr std::int32_t MaxUnchangedLength = 0x1000;
constexpr std::int32_t MaxUnchanged = MaxUnchangedLength - 1;
constexpr std::int32_t MaxShortChangeOldLength = 6;
constexpr std::int32_t MaxShortChangeNewLength = 7;
constexpr std::int32_t ShortChangeNumMask = 0x1ff;
constexpr std::int32_t MaxShortChange = 0x6fff;
constexpr std::int32_t LengthIn1Trail = 61;
constexpr std::int32_t LengthIn2Trail = 62;
constexpr std::int32_t IllegalArgumentError = 1;
constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t IndexOutOfBoundsError = 8;

template<typename TObject>
TObject* Object(TObject* object) {
    if (!object) APS5_INVALID_ARG_EX;
    return object;
}

bool Failure(std::int32_t code) {
    return code > 0;
}

std::uint16_t Unit(const std::uint16_t* array, std::int32_t index) {
    if (!array) APS5_INVALID_ARG_EX;
    return array[index];
}

std::int32_t LastUnit(const Edits* edits) {
    return edits->length > 0 ? Unit(edits->array, edits->length - 1) : 0xffff;
}

void SetLastUnit(Edits* edits, std::int32_t last) {
    edits->array[edits->length - 1] = static_cast<std::uint16_t>(last);
}

void ReleaseArray(Edits* edits) {
    if (edits->array != edits->stackArray) IcuReleaseMemory_nid_no_patch(edits->array);
}

Edits* CopyArray(Edits* edits, const Edits* other) {
    if (Failure(edits->errorCode)) {
        edits->length = edits->delta = edits->numChanges = 0;
        return edits;
    }
    if (edits->length > edits->capacity) {
        auto* newArray = static_cast<std::uint16_t*>(uprv_malloc_nid_postfix(static_cast<std::size_t>(edits->length) * 2));
        if (!newArray) {
            edits->length = edits->delta = edits->numChanges = 0;
            edits->errorCode = MemoryAllocationError;
            return edits;
        }
        ReleaseArray(edits);
        edits->array = newArray;
        edits->capacity = edits->length;
    }
    if (edits->length > 0) {
        if (!other->array) APS5_INVALID_ARG_EX;
        std::memcpy(edits->array, other->array, static_cast<std::size_t>(edits->length) * 2);
    }
    return edits;
}

void Reset(Edits* edits) {
    edits->length = edits->delta = edits->numChanges = 0;
    edits->errorCode = 0;
}

Edits* MoveArray(Edits* edits, Edits* source) {
    if (Failure(edits->errorCode)) {
        edits->length = edits->delta = edits->numChanges = 0;
        return edits;
    }
    ReleaseArray(edits);
    if (edits->length > StackCapacity) {
        edits->array = source->array;
        edits->capacity = source->capacity;
        source->array = source->stackArray;
        source->capacity = StackCapacity;
        Reset(source);
        return edits;
    }
    edits->array = edits->stackArray;
    edits->capacity = StackCapacity;
    if (edits->length > 0) {
        if (!source->array) APS5_INVALID_ARG_EX;
        std::memmove(edits->array, source->array, static_cast<std::size_t>(edits->length) * 2);
    }
    return edits;
}

bool GrowArray(Edits* edits) {
    std::int32_t newCapacity;
    if (edits->array == edits->stackArray) {
        newCapacity = 2000;
    } else if (edits->capacity == INT32_MAX) {
        edits->errorCode = IndexOutOfBoundsError;
        return false;
    } else if (edits->capacity >= (INT32_MAX / 2)) {
        newCapacity = INT32_MAX;
    } else {
        newCapacity = 2 * edits->capacity;
    }
    if ((newCapacity - edits->capacity) < 5) {
        edits->errorCode = IndexOutOfBoundsError;
        return false;
    }
    auto* newArray = static_cast<std::uint16_t*>(uprv_malloc_nid_postfix(static_cast<std::size_t>(newCapacity) * 2));
    if (!newArray) {
        edits->errorCode = MemoryAllocationError;
        return false;
    }
    if (edits->length > 0) std::memcpy(newArray, edits->array, static_cast<std::size_t>(edits->length) * 2);
    ReleaseArray(edits);
    edits->array = newArray;
    edits->capacity = newCapacity;
    return true;
}

void Append(Edits* edits, std::int32_t r) {
    if (edits->length < edits->capacity || GrowArray(edits)) edits->array[edits->length++] = static_cast<std::uint16_t>(r);
}

void AddUnchanged(Edits* edits, std::int32_t unchangedLength) {
    if (Failure(edits->errorCode) || unchangedLength == 0) return;
    if (unchangedLength < 0) {
        edits->errorCode = IllegalArgumentError;
        return;
    }
    const std::int32_t last = LastUnit(edits);
    if (last < MaxUnchanged) {
        const std::int32_t remaining = MaxUnchanged - last;
        if (remaining >= unchangedLength) {
            SetLastUnit(edits, last + unchangedLength);
            return;
        }
        SetLastUnit(edits, MaxUnchanged);
        unchangedLength -= remaining;
    }
    while (unchangedLength >= MaxUnchangedLength) {
        Append(edits, MaxUnchanged);
        unchangedLength -= MaxUnchangedLength;
    }
    if (unchangedLength > 0) Append(edits, unchangedLength - 1);
}

void AddReplace(Edits* edits, std::int32_t oldLength, std::int32_t newLength) {
    if (Failure(edits->errorCode)) return;
    if (oldLength < 0 || newLength < 0) {
        edits->errorCode = IllegalArgumentError;
        return;
    }
    if (oldLength == 0 && newLength == 0) return;
    ++edits->numChanges;
    const std::int32_t newDelta = newLength - oldLength;
    if (newDelta != 0) {
        if ((newDelta > 0 && edits->delta >= 0 && newDelta > (INT32_MAX - edits->delta)) ||
            (newDelta < 0 && edits->delta < 0 && newDelta < (INT32_MIN - edits->delta))) {
            edits->errorCode = IndexOutOfBoundsError;
            return;
        }
        edits->delta += newDelta;
    }
    if (0 < oldLength && oldLength <= MaxShortChangeOldLength && newLength <= MaxShortChangeNewLength) {
        const std::int32_t u = (oldLength << 12) | (newLength << 9);
        const std::int32_t last = LastUnit(edits);
        if (MaxUnchanged < last && last < MaxShortChange && (last & ~ShortChangeNumMask) == u && (last & ShortChangeNumMask) < ShortChangeNumMask) {
            SetLastUnit(edits, last + 1);
            return;
        }
        Append(edits, u);
        return;
    }
    std::int32_t head = 0x7000;
    if (oldLength < LengthIn1Trail && newLength < LengthIn1Trail) {
        head |= oldLength << 6;
        head |= newLength;
        Append(edits, head);
    } else if ((edits->capacity - edits->length) >= 5 || GrowArray(edits)) {
        std::int32_t limit = edits->length + 1;
        if (oldLength < LengthIn1Trail) {
            head |= oldLength << 6;
        } else if (oldLength <= 0x7fff) {
            head |= LengthIn1Trail << 6;
            edits->array[limit++] = static_cast<std::uint16_t>(0x8000 | oldLength);
        } else {
            head |= (LengthIn2Trail + (oldLength >> 30)) << 6;
            edits->array[limit++] = static_cast<std::uint16_t>(0x8000 | (oldLength >> 15));
            edits->array[limit++] = static_cast<std::uint16_t>(0x8000 | oldLength);
        }
        if (newLength < LengthIn1Trail) {
            head |= newLength;
        } else if (newLength <= 0x7fff) {
            head |= LengthIn1Trail;
            edits->array[limit++] = static_cast<std::uint16_t>(0x8000 | newLength);
        } else {
            head |= LengthIn2Trail + (newLength >> 30);
            edits->array[limit++] = static_cast<std::uint16_t>(0x8000 | (newLength >> 15));
            edits->array[limit++] = static_cast<std::uint16_t>(0x8000 | newLength);
        }
        edits->array[edits->length] = static_cast<std::uint16_t>(head);
        edits->length = limit;
    }
}

bool CopyErrorTo(const Edits* edits, std::int32_t* outErrorCode) {
    if (Failure(*outErrorCode)) return true;
    if (!Failure(edits->errorCode)) return false;
    *outErrorCode = edits->errorCode;
    return true;
}

void Construct(EditsIterator* iterator, const std::uint16_t* array, std::int32_t length, std::int8_t onlyChanges, std::int8_t coarse) {
    iterator->array = array;
    iterator->index = 0;
    iterator->length = length;
    iterator->remaining = 0;
    iterator->onlyChanges = onlyChanges;
    iterator->coarse = coarse;
    iterator->dir = 0;
    iterator->changed = 0;
    iterator->oldLength = 0;
    iterator->newLength = 0;
    iterator->srcIndex = 0;
    iterator->replIndex = 0;
    iterator->destIndex = 0;
}

std::int32_t ReadLength(EditsIterator* iterator, std::int32_t head) {
    if (head < LengthIn1Trail) return head;
    if (head < LengthIn2Trail) return Unit(iterator->array, iterator->index++) & 0x7fff;
    const std::int32_t length = ((head & 1) << 30) | (static_cast<std::int32_t>(Unit(iterator->array, iterator->index) & 0x7fff) << 15) |
                                (Unit(iterator->array, iterator->index + 1) & 0x7fff);
    iterator->index += 2;
    return length;
}

void UpdateNextIndexes(EditsIterator* iterator) {
    iterator->srcIndex += iterator->oldLength;
    if (iterator->changed) iterator->replIndex += iterator->newLength;
    iterator->destIndex += iterator->newLength;
}

void UpdatePreviousIndexes(EditsIterator* iterator) {
    iterator->srcIndex -= iterator->oldLength;
    if (iterator->changed) iterator->replIndex -= iterator->newLength;
    iterator->destIndex -= iterator->newLength;
}

bool NoNext(EditsIterator* iterator) {
    iterator->dir = 0;
    iterator->changed = 0;
    iterator->oldLength = iterator->newLength = 0;
    return false;
}

bool Next(EditsIterator* it, bool onlyChanges, std::int32_t* errorCode) {
    if (Failure(*errorCode)) return false;
    if (it->dir > 0) {
        UpdateNextIndexes(it);
    } else {
        if (it->dir < 0) {
            if (it->remaining > 0) {
                ++it->index;
                it->dir = 1;
                return true;
            }
        }
        it->dir = 1;
    }
    if (it->remaining >= 1) {
        if (it->remaining > 1) {
            --it->remaining;
            return true;
        }
        it->remaining = 0;
    }
    if (it->index >= it->length) return NoNext(it);
    std::int32_t u = Unit(it->array, it->index++);
    if (u <= MaxUnchanged) {
        it->changed = 0;
        it->oldLength = u + 1;
        while (it->index < it->length && (u = Unit(it->array, it->index)) <= MaxUnchanged) {
            ++it->index;
            it->oldLength += u + 1;
        }
        it->newLength = it->oldLength;
        if (onlyChanges) {
            UpdateNextIndexes(it);
            if (it->index >= it->length) return NoNext(it);
            ++it->index;
        } else {
            return true;
        }
    }
    it->changed = 1;
    if (u <= MaxShortChange) {
        const std::int32_t oldLen = u >> 12;
        const std::int32_t newLen = (u >> 9) & MaxShortChangeNewLength;
        const std::int32_t num = (u & ShortChangeNumMask) + 1;
        if (it->coarse) {
            it->oldLength = num * oldLen;
            it->newLength = num * newLen;
        } else {
            it->oldLength = oldLen;
            it->newLength = newLen;
            if (num > 1) it->remaining = num;
            return true;
        }
    } else {
        it->oldLength = ReadLength(it, (u >> 6) & 0x3f);
        it->newLength = ReadLength(it, u & 0x3f);
        if (!it->coarse) return true;
    }
    while (it->index < it->length && (u = Unit(it->array, it->index)) > MaxUnchanged) {
        ++it->index;
        if (u <= MaxShortChange) {
            const std::int32_t num = (u & ShortChangeNumMask) + 1;
            it->oldLength += (u >> 12) * num;
            it->newLength += ((u >> 9) & MaxShortChangeNewLength) * num;
        } else {
            it->oldLength += ReadLength(it, (u >> 6) & 0x3f);
            it->newLength += ReadLength(it, u & 0x3f);
        }
    }
    return true;
}

bool Previous(EditsIterator* it, std::int32_t* errorCode) {
    if (Failure(*errorCode)) return false;
    if (it->dir >= 0) {
        if (it->dir > 0) {
            if (it->remaining > 0) {
                --it->index;
                it->dir = -1;
                return true;
            }
            UpdateNextIndexes(it);
        }
        it->dir = -1;
    }
    if (it->remaining > 0) {
        const std::int32_t u = Unit(it->array, it->index);
        if (it->remaining <= (u & ShortChangeNumMask)) {
            ++it->remaining;
            UpdatePreviousIndexes(it);
            return true;
        }
        it->remaining = 0;
    }
    if (it->index <= 0) return NoNext(it);
    std::int32_t u = Unit(it->array, --it->index);
    if (u <= MaxUnchanged) {
        it->changed = 0;
        it->oldLength = u + 1;
        while (it->index > 0 && (u = Unit(it->array, it->index - 1)) <= MaxUnchanged) {
            --it->index;
            it->oldLength += u + 1;
        }
        it->newLength = it->oldLength;
        UpdatePreviousIndexes(it);
        return true;
    }
    it->changed = 1;
    if (u <= MaxShortChange) {
        const std::int32_t oldLen = u >> 12;
        const std::int32_t newLen = (u >> 9) & MaxShortChangeNewLength;
        const std::int32_t num = (u & ShortChangeNumMask) + 1;
        if (it->coarse) {
            it->oldLength = num * oldLen;
            it->newLength = num * newLen;
        } else {
            it->oldLength = oldLen;
            it->newLength = newLen;
            if (num > 1) it->remaining = 1;
            UpdatePreviousIndexes(it);
            return true;
        }
    } else {
        if (u <= 0x7fff) {
            it->oldLength = ReadLength(it, (u >> 6) & 0x3f);
            it->newLength = ReadLength(it, u & 0x3f);
        } else {
            while ((u = Unit(it->array, --it->index)) > 0x7fff) {
            }
            const std::int32_t headIndex = it->index++;
            it->oldLength = ReadLength(it, (u >> 6) & 0x3f);
            it->newLength = ReadLength(it, u & 0x3f);
            it->index = headIndex;
        }
        if (!it->coarse) {
            UpdatePreviousIndexes(it);
            return true;
        }
    }
    while (it->index > 0 && (u = Unit(it->array, it->index - 1)) > MaxUnchanged) {
        --it->index;
        if (u <= MaxShortChange) {
            const std::int32_t num = (u & ShortChangeNumMask) + 1;
            it->oldLength += (u >> 12) * num;
            it->newLength += ((u >> 9) & MaxShortChangeNewLength) * num;
        } else if (u <= 0x7fff) {
            const std::int32_t headIndex = it->index++;
            it->oldLength += ReadLength(it, (u >> 6) & 0x3f);
            it->newLength += ReadLength(it, u & 0x3f);
            it->index = headIndex;
        }
    }
    UpdatePreviousIndexes(it);
    return true;
}

std::int32_t FindIndex(EditsIterator* it, std::int32_t i, bool findSource, std::int32_t* errorCode) {
    if (Failure(*errorCode) || i < 0) return -1;
    std::int32_t spanStart;
    std::int32_t spanLength;
    if (findSource) {
        spanStart = it->srcIndex;
        spanLength = it->oldLength;
    } else {
        spanStart = it->destIndex;
        spanLength = it->newLength;
    }
    if (i < spanStart) {
        if (i >= (spanStart / 2)) {
            for (;;) {
                Previous(it, errorCode);
                spanStart = findSource ? it->srcIndex : it->destIndex;
                if (i >= spanStart) return 0;
                if (it->remaining > 0) {
                    spanLength = findSource ? it->oldLength : it->newLength;
                    const std::int32_t u = Unit(it->array, it->index);
                    const std::int32_t num = (u & ShortChangeNumMask) + 1 - it->remaining;
                    const std::int32_t len = num * spanLength;
                    if (i >= (spanStart - len)) {
                        const std::int32_t n = ((spanStart - i - 1) / spanLength) + 1;
                        it->srcIndex -= n * it->oldLength;
                        it->replIndex -= n * it->newLength;
                        it->destIndex -= n * it->newLength;
                        it->remaining += n;
                        return 0;
                    }
                    it->srcIndex -= num * it->oldLength;
                    it->replIndex -= num * it->newLength;
                    it->destIndex -= num * it->newLength;
                    it->remaining = 0;
                }
            }
        }
        it->dir = 0;
        it->index = it->remaining = it->oldLength = it->newLength = it->srcIndex = it->replIndex = it->destIndex = 0;
    } else if (i < (spanStart + spanLength)) {
        return 0;
    }
    while (Next(it, false, errorCode)) {
        if (findSource) {
            spanStart = it->srcIndex;
            spanLength = it->oldLength;
        } else {
            spanStart = it->destIndex;
            spanLength = it->newLength;
        }
        if (i < (spanStart + spanLength)) return 0;
        if (it->remaining > 1) {
            const std::int32_t len = it->remaining * spanLength;
            if (i < (spanStart + len)) {
                const std::int32_t n = (i - spanStart) / spanLength;
                it->srcIndex += n * it->oldLength;
                it->replIndex += n * it->newLength;
                it->destIndex += n * it->newLength;
                it->remaining -= n;
                return 0;
            }
            it->oldLength *= it->remaining;
            it->newLength *= it->remaining;
            it->remaining = 0;
        }
    }
    return 1;
}

Edits* MergeAndAppend(Edits* edits, const Edits* ab, const Edits* bc, std::int32_t* errorCode) {
    if (CopyErrorTo(edits, errorCode)) return edits;
    EditsIterator abIter;
    EditsIterator bcIter;
    Construct(&abIter, ab->array, ab->length, 0, 0);
    Construct(&bcIter, bc->array, bc->length, 0, 0);
    bool abHasNext = true;
    bool bcHasNext = true;
    std::int32_t aLength = 0;
    std::int32_t ab_bLength = 0;
    std::int32_t bc_bLength = 0;
    std::int32_t cLength = 0;
    std::int32_t pending_aLength = 0;
    std::int32_t pending_cLength = 0;
    for (;;) {
        if (bc_bLength == 0) {
            if (bcHasNext && (bcHasNext = Next(&bcIter, bcIter.onlyChanges != 0, errorCode))) {
                bc_bLength = bcIter.oldLength;
                cLength = bcIter.newLength;
                if (bc_bLength == 0) {
                    if (ab_bLength == 0 || !abIter.changed) {
                        AddReplace(edits, pending_aLength, pending_cLength + cLength);
                        pending_aLength = pending_cLength = 0;
                    } else {
                        pending_cLength += cLength;
                    }
                    continue;
                }
            }
        }
        if (ab_bLength == 0) {
            if (abHasNext && (abHasNext = Next(&abIter, abIter.onlyChanges != 0, errorCode))) {
                aLength = abIter.oldLength;
                ab_bLength = abIter.newLength;
                if (ab_bLength == 0) {
                    if (bc_bLength == bcIter.oldLength || !bcIter.changed) {
                        AddReplace(edits, pending_aLength + aLength, pending_cLength);
                        pending_aLength = pending_cLength = 0;
                    } else {
                        pending_aLength += aLength;
                    }
                    continue;
                }
            } else if (bc_bLength == 0) {
                break;
            } else {
                if (!CopyErrorTo(edits, errorCode)) *errorCode = IllegalArgumentError;
                return edits;
            }
        }
        if (bc_bLength == 0) {
            if (!CopyErrorTo(edits, errorCode)) *errorCode = IllegalArgumentError;
            return edits;
        }
        if (!abIter.changed && !bcIter.changed) {
            if (pending_aLength != 0 || pending_cLength != 0) {
                AddReplace(edits, pending_aLength, pending_cLength);
                pending_aLength = pending_cLength = 0;
            }
            const std::int32_t unchangedLength = aLength <= cLength ? aLength : cLength;
            AddUnchanged(edits, unchangedLength);
            ab_bLength = aLength -= unchangedLength;
            bc_bLength = cLength -= unchangedLength;
            continue;
        }
        if (!abIter.changed && bcIter.changed) {
            if (ab_bLength >= bc_bLength) {
                AddReplace(edits, pending_aLength + bc_bLength, pending_cLength + cLength);
                pending_aLength = pending_cLength = 0;
                aLength = ab_bLength -= bc_bLength;
                bc_bLength = 0;
                continue;
            }
        } else if (abIter.changed && !bcIter.changed) {
            if (ab_bLength <= bc_bLength) {
                AddReplace(edits, pending_aLength + aLength, pending_cLength + ab_bLength);
                pending_aLength = pending_cLength = 0;
                cLength = bc_bLength -= ab_bLength;
                ab_bLength = 0;
                continue;
            }
        } else {
            if (ab_bLength == bc_bLength) {
                AddReplace(edits, pending_aLength + aLength, pending_cLength + cLength);
                pending_aLength = pending_cLength = 0;
                ab_bLength = bc_bLength = 0;
                continue;
            }
        }
        pending_aLength += aLength;
        pending_cLength += cLength;
        if (ab_bLength < bc_bLength) {
            bc_bLength -= ab_bLength;
            cLength = ab_bLength = 0;
        } else {
            ab_bLength -= bc_bLength;
            aLength = bc_bLength = 0;
        }
    }
    if (pending_aLength != 0 || pending_cLength != 0) AddReplace(edits, pending_aLength, pending_cLength);
    CopyErrorTo(edits, errorCode);
    return edits;
}

} // namespace

extern "C" {

void APS5_VABI _ZN6icu_685Edits12releaseArrayEv_nid_postfix(Icu::Edits* self) {
    ReleaseArray(Object(self));
}

Icu::Edits* APS5_VABI _ZN6icu_685Edits9copyArrayERKS0__nid_postfix(Icu::Edits* self, const Icu::Edits* other) {
    return CopyArray(Object(self), Object(other));
}

Icu::Edits* APS5_VABI _ZN6icu_685Edits9moveArrayERS0__nid_postfix(Icu::Edits* self, Icu::Edits* source) {
    return MoveArray(Object(self), Object(source));
}

Icu::Edits* APS5_VABI _ZN6icu_685EditsaSERKS0__nid_postfix(Icu::Edits* self, const Icu::Edits* other) {
    Object(self);
    Object(other);
    self->length = other->length;
    self->delta = other->delta;
    self->numChanges = other->numChanges;
    self->errorCode = other->errorCode;
    return CopyArray(self, other);
}

Icu::Edits* APS5_VABI _ZN6icu_685EditsaSEOS0__nid_postfix(Icu::Edits* self, Icu::Edits* source) {
    Object(self);
    Object(source);
    self->length = source->length;
    self->delta = source->delta;
    self->numChanges = source->numChanges;
    self->errorCode = source->errorCode;
    return MoveArray(self, source);
}

void APS5_VABI _ZN6icu_685EditsD1Ev_nid_postfix(Icu::Edits* self) {
    ReleaseArray(Object(self));
}

void APS5_VABI _ZN6icu_685EditsD2Ev_nid_postfix(Icu::Edits* self) {
    ReleaseArray(Object(self));
}

void APS5_VABI _ZN6icu_685Edits5resetEv_nid_postfix(Icu::Edits* self) {
    Reset(Object(self));
}

void APS5_VABI _ZN6icu_685Edits12addUnchangedEi_nid_postfix(Icu::Edits* self, std::int32_t unchangedLength) {
    AddUnchanged(Object(self), unchangedLength);
}

void APS5_VABI _ZN6icu_685Edits10addReplaceEii_nid_postfix(Icu::Edits* self, std::int32_t oldLength, std::int32_t newLength) {
    AddReplace(Object(self), oldLength, newLength);
}

void APS5_VABI _ZN6icu_685Edits6appendEi_nid_postfix(Icu::Edits* self, std::int32_t r) {
    Append(Object(self), r);
}

std::int8_t APS5_VABI _ZN6icu_685Edits9growArrayEv_nid_postfix(Icu::Edits* self) {
    return GrowArray(Object(self));
}

std::int8_t APS5_VABI _ZNK6icu_685Edits11copyErrorToER10UErrorCode_nid_postfix(const Icu::Edits* self, std::int32_t* outErrorCode) {
    Object(self);
    return CopyErrorTo(self, Object(outErrorCode));
}

Icu::Edits* APS5_VABI _ZN6icu_685Edits14mergeAndAppendERKS0_S2_R10UErrorCode_nid_postfix(Icu::Edits* self, const Icu::Edits* ab, const Icu::Edits* bc,
                                                                                       std::int32_t* errorCode) {
    Object(self);
    Object(ab);
    Object(bc);
    return MergeAndAppend(self, ab, bc, Object(errorCode));
}

void APS5_VABI _ZN6icu_685Edits8IteratorC1EPKtiaa_nid_postfix(Icu::EditsIterator* self, const std::uint16_t* array, std::int32_t length,
                                                             std::int8_t onlyChanges, std::int8_t coarse) {
    Construct(Object(self), array, length, onlyChanges, coarse);
}

void APS5_VABI _ZN6icu_685Edits8IteratorC2EPKtiaa_nid_postfix(Icu::EditsIterator* self, const std::uint16_t* array, std::int32_t length,
                                                             std::int8_t onlyChanges, std::int8_t coarse) {
    Construct(Object(self), array, length, onlyChanges, coarse);
}

std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator10readLengthEi_nid_postfix(Icu::EditsIterator* self, std::int32_t head) {
    return ReadLength(Object(self), head);
}

void APS5_VABI _ZN6icu_685Edits8Iterator17updateNextIndexesEv_nid_postfix(Icu::EditsIterator* self) {
    UpdateNextIndexes(Object(self));
}

void APS5_VABI _ZN6icu_685Edits8Iterator21updatePreviousIndexesEv_nid_postfix(Icu::EditsIterator* self) {
    UpdatePreviousIndexes(Object(self));
}

std::int8_t APS5_VABI _ZN6icu_685Edits8Iterator6noNextEv_nid_postfix(Icu::EditsIterator* self) {
    return NoNext(Object(self));
}

std::int8_t APS5_VABI _ZN6icu_685Edits8Iterator4nextEaR10UErrorCode_nid_postfix(Icu::EditsIterator* self, std::int8_t onlyChanges, std::int32_t* errorCode) {
    Object(self);
    return Next(self, onlyChanges != 0, Object(errorCode));
}

std::int8_t APS5_VABI _ZN6icu_685Edits8Iterator8previousER10UErrorCode_nid_postfix(Icu::EditsIterator* self, std::int32_t* errorCode) {
    Object(self);
    return Previous(self, Object(errorCode));
}

std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator9findIndexEiaR10UErrorCode_nid_postfix(Icu::EditsIterator* self, std::int32_t i, std::int8_t findSource,
                                                                                     std::int32_t* errorCode) {
    Object(self);
    return FindIndex(self, i, findSource != 0, Object(errorCode));
}

std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator31destinationIndexFromSourceIndexEiR10UErrorCode_nid_postfix(Icu::EditsIterator* self, std::int32_t i,
                                                                                                           std::int32_t* errorCode) {
    Object(self);
    const std::int32_t where = FindIndex(self, i, true, Object(errorCode));
    if (where < 0) return 0;
    if (where > 0 || i == self->srcIndex) return self->destIndex;
    if (self->changed) return self->destIndex + self->newLength;
    return self->destIndex + (i - self->srcIndex);
}

std::int32_t APS5_VABI _ZN6icu_685Edits8Iterator31sourceIndexFromDestinationIndexEiR10UErrorCode_nid_postfix(Icu::EditsIterator* self, std::int32_t i,
                                                                                                           std::int32_t* errorCode) {
    Object(self);
    const std::int32_t where = FindIndex(self, i, false, Object(errorCode));
    if (where < 0) return 0;
    if (where > 0 || i == self->destIndex) return self->srcIndex;
    if (self->changed) return self->srcIndex + self->oldLength;
    return self->srcIndex + (i - self->destIndex);
}

}
