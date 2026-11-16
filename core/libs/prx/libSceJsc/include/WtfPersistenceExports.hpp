#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/WtfSha1Exports.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Jsc::WtfAbi {

struct PersistenceDecoder {
    const std::uint8_t* buffer;
    const std::uint8_t* bufferPosition;
    const std::uint8_t* bufferEnd;
    SHA1 sha1;
};

inline constexpr std::size_t PersistenceEncoderInlineCapacity = 4096;

struct PersistenceEncoder {
    std::uint8_t* buffer;
    unsigned capacity;
    unsigned size;
    std::uint8_t inlineBuffer[PersistenceEncoderInlineCapacity];
    SHA1 sha1;
};

struct OptionalSHA1Digest {
    bool hasValue;
    SHA1Digest value;
};

static_assert(sizeof(PersistenceDecoder) == 128);
static_assert(offsetof(PersistenceDecoder, bufferPosition) == 8);
static_assert(offsetof(PersistenceDecoder, bufferEnd) == 16);
static_assert(offsetof(PersistenceDecoder, sha1) == 24);
static_assert(offsetof(PersistenceEncoder, capacity) == 8);
static_assert(offsetof(PersistenceEncoder, size) == 12);
static_assert(offsetof(PersistenceEncoder, inlineBuffer) == 16);
static_assert(offsetof(PersistenceEncoder, sha1) == 4112);
static_assert(sizeof(PersistenceEncoder) == 4216);
static_assert(sizeof(OptionalSHA1Digest) == 21);
static_assert(offsetof(OptionalSHA1Digest, value) == 1);
static_assert(std::is_trivially_copyable_v<OptionalSHA1Digest>);

} // namespace Jsc::WtfAbi

extern "C" {

Jsc::WtfAbi::OptionalSHA1Digest APS5_VABI _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(Jsc::WtfAbi::PersistenceDecoder* decoder);
void APS5_VABI _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(Jsc::WtfAbi::PersistenceEncoder* encoder,
                                                                                       const Jsc::WtfAbi::SHA1Digest* digest);

}
