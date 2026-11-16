#include "prx/libSceJsc/libSceJsc.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace {

using Jsc::WtfAbi::OptionalSHA1Digest;
using Jsc::WtfAbi::PersistenceDecoder;
using Jsc::WtfAbi::PersistenceEncoder;
using Jsc::WtfAbi::SHA1;
using Jsc::WtfAbi::SHA1Digest;

using DecodeFunction = OptionalSHA1Digest (APS5_VABI*)(PersistenceDecoder*);
using EncodeFunction = void (APS5_VABI*)(PersistenceEncoder*, const SHA1Digest*);

static_assert(std::is_same_v<decltype(&_ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE), DecodeFunction>);
static_assert(std::is_same_v<decltype(&_ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_), EncodeFunction>);

constexpr SHA1Digest OneDigestChecksum{0xba, 0xe1, 0x49, 0x7d, 0x7b, 0x46, 0x1c, 0x21, 0x58, 0x7f,
                                       0xec, 0xec, 0xdc, 0x68, 0x3c, 0x11, 0x55, 0x05, 0x61, 0x8f};
constexpr SHA1Digest FourDigestChecksum{0x0d, 0xd8, 0x42, 0x92, 0x70, 0xa8, 0xc6, 0x83, 0x1f, 0xc5,
                                        0xe7, 0xea, 0xfd, 0xa8, 0x60, 0x5d, 0xc6, 0x7e, 0xdc, 0x0e};

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF persistence check failed at line %d\n", line);
        std::abort();
    }
}

template <typename Exception, typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

void ResetSha1(SHA1& sha1) {
    std::memset(&sha1, 0, sizeof(sha1));
    sha1.hash[0] = 0x67452301;
    sha1.hash[1] = 0xefcdab89;
    sha1.hash[2] = 0x98badcfe;
    sha1.hash[3] = 0x10325476;
    sha1.hash[4] = 0xc3d2e1f0;
}

SHA1Digest Finish(SHA1& sha1) {
    SHA1Digest digest{};
    _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, &digest);
    return digest;
}

SHA1Digest Sequence(std::uint8_t first) {
    SHA1Digest digest{};
    for (std::size_t i = 0; i < digest.size(); ++i) digest[i] = static_cast<std::uint8_t>(first + i);
    return digest;
}

void TestDecode() {
    std::uint8_t data[80];
    for (std::size_t i = 0; i < sizeof(data); ++i) data[i] = static_cast<std::uint8_t>(i);

    PersistenceDecoder decoder{};
    decoder.buffer = data;
    decoder.bufferPosition = data;
    decoder.bufferEnd = data + 20;
    ResetSha1(decoder.sha1);
    OptionalSHA1Digest result = _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(&decoder);
    Check(result.hasValue, __LINE__);
    Check(result.value == Sequence(0), __LINE__);
    Check(decoder.bufferPosition == data + 20 && decoder.buffer == data && decoder.bufferEnd == data + 20, __LINE__);
    Check(decoder.sha1.totalBytes == 24 && decoder.sha1.cursor == 24, __LINE__);
    Check(Finish(decoder.sha1) == OneDigestChecksum, __LINE__);

    decoder.bufferPosition = data;
    decoder.bufferEnd = data + sizeof(data);
    ResetSha1(decoder.sha1);
    for (int i = 0; i < 4; ++i) {
        result = _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(&decoder);
        Check(result.hasValue && result.value == Sequence(static_cast<std::uint8_t>(i * 20)), __LINE__);
    }
    Check(decoder.bufferPosition == data + 80, __LINE__);
    Check(decoder.sha1.totalBytes == 96 && decoder.sha1.cursor == 32, __LINE__);
    result = _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(&decoder);
    Check(!result.hasValue, __LINE__);
    Check(decoder.bufferPosition == data + 80 && decoder.sha1.totalBytes == 96, __LINE__);
    Check(Finish(decoder.sha1) == FourDigestChecksum, __LINE__);

    decoder.bufferPosition = data;
    decoder.bufferEnd = data + 19;
    ResetSha1(decoder.sha1);
    result = _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(&decoder);
    Check(!result.hasValue && decoder.bufferPosition == data && decoder.sha1.totalBytes == 0, __LINE__);

    decoder.buffer = nullptr;
    decoder.bufferPosition = nullptr;
    decoder.bufferEnd = nullptr;
    result = _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(&decoder);
    Check(!result.hasValue && decoder.bufferPosition == nullptr, __LINE__);

    decoder.buffer = data;
    decoder.bufferPosition = data;
    decoder.bufferEnd = data + 20;
    decoder.sha1.cursor = 64;
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(&decoder); }), __LINE__);
    Check(Throws<std::invalid_argument>([] { _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(nullptr); }), __LINE__);
}

std::unique_ptr<PersistenceEncoder> MakeEncoder() {
    auto encoder = std::make_unique<PersistenceEncoder>();
    std::memset(encoder.get(), 0xA5, sizeof(PersistenceEncoder));
    encoder->buffer = encoder->inlineBuffer;
    encoder->capacity = Jsc::WtfAbi::PersistenceEncoderInlineCapacity;
    encoder->size = 0;
    ResetSha1(encoder->sha1);
    return encoder;
}

void TestEncode() {
    auto encoder = MakeEncoder();
    const SHA1Digest digest = Sequence(0);
    _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(encoder.get(), &digest);
    Check(encoder->size == 20 && encoder->capacity == 4096 && encoder->buffer == encoder->inlineBuffer, __LINE__);
    Check(std::memcmp(encoder->inlineBuffer, digest.data(), digest.size()) == 0, __LINE__);
    Check(encoder->inlineBuffer[20] == 0xA5, __LINE__);
    Check(Finish(encoder->sha1) == OneDigestChecksum, __LINE__);

    encoder = MakeEncoder();
    for (int i = 0; i < 4; ++i) {
        const SHA1Digest next = Sequence(static_cast<std::uint8_t>(i * 20));
        _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(encoder.get(), &next);
    }
    Check(encoder->size == 80, __LINE__);
    for (std::size_t i = 0; i < 80; ++i) Check(encoder->inlineBuffer[i] == i, __LINE__);
    Check(Finish(encoder->sha1) == FourDigestChecksum, __LINE__);

    encoder = MakeEncoder();
    encoder->size = 4076;
    _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(encoder.get(), &digest);
    Check(encoder->size == 4096, __LINE__);
    Check(std::memcmp(encoder->inlineBuffer + 4076, digest.data(), digest.size()) == 0, __LINE__);

    std::uint8_t external[64];
    std::memset(external, 0, sizeof(external));
    encoder = MakeEncoder();
    encoder->buffer = external;
    encoder->capacity = sizeof(external);
    encoder->size = 40;
    _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(encoder.get(), &digest);
    Check(encoder->size == 60 && std::memcmp(external + 40, digest.data(), digest.size()) == 0, __LINE__);
    Check(encoder->inlineBuffer[40] == 0xA5, __LINE__);

    encoder->size = 45;
    const std::uint64_t bytesBefore = encoder->sha1.totalBytes;
    Check(Throws<std::runtime_error>([&] { _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(encoder.get(), &digest); }), __LINE__);
    Check(encoder->size == 45 && encoder->sha1.totalBytes == bytesBefore, __LINE__);

    Check(Throws<std::invalid_argument>([&] { _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(encoder.get(), nullptr); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(nullptr, &digest); }), __LINE__);
}

}

int main() {
    TestDecode();
    TestEncode();
    std::puts("WTF persistence tests passed");
    return 0;
}
