// Derived from Source/WTF/wtf/persistence/PersistentCoders.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2011, 2014-2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Derived from Source/WTF/wtf/persistence/PersistentDecoder.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2010, 2011, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Derived from Source/WTF/wtf/persistence/PersistentEncoder.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceJsc/include/WtfPersistenceExports.hpp"
#include "prx/libSceJsc/src/WtfSha1Internal.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

using namespace Jsc::WtfAbi;

constexpr unsigned DataSalt = 101;

void UpdateChecksumForData(SHA1& sha1, const std::uint8_t* data, std::size_t size) {
    std::uint8_t salt[sizeof(DataSalt)];
    std::memcpy(salt, &DataSalt, sizeof(salt));
    Sha1AddBytes(sha1, salt, sizeof(salt));
    Sha1AddBytes(sha1, data, size);
}

bool BufferIsLargeEnoughToContain(const PersistenceDecoder& decoder, std::size_t size) {
    const auto available = reinterpret_cast<std::uintptr_t>(decoder.bufferEnd) - reinterpret_cast<std::uintptr_t>(decoder.bufferPosition);
    return size <= available;
}

}

extern "C" {

OptionalSHA1Digest APS5_VABI _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(PersistenceDecoder* decoder) {
    if (!decoder) throw std::invalid_argument("WTF::Persistence::Coder<SHA1::Digest>::decode requires a decoder");
    OptionalSHA1Digest result{};
    if (!BufferIsLargeEnoughToContain(*decoder, result.value.size())) return result;
    std::memcpy(result.value.data(), decoder->bufferPosition, result.value.size());
    decoder->bufferPosition += result.value.size();
    UpdateChecksumForData(decoder->sha1, result.value.data(), result.value.size());
    result.hasValue = true;
    return result;
}

void APS5_VABI _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(PersistenceEncoder* encoder, const SHA1Digest* digest) {
    if (!encoder || !digest) throw std::invalid_argument("WTF::Persistence::Coder<SHA1::Digest>::encode requires an encoder and a digest");
    const std::size_t newSize = std::size_t{encoder->size} + digest->size();
    if (newSize > encoder->capacity) NotImplemented_nid_no_patch(__func__);
    UpdateChecksumForData(encoder->sha1, digest->data(), digest->size());
    std::memcpy(encoder->buffer + encoder->size, digest->data(), digest->size());
    encoder->size = static_cast<unsigned>(newSize);
}

}
