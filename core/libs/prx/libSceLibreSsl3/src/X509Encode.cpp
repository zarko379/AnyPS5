/* $OpenBSD: a_bitstr.c,v 1.43 2024/07/08 14:52:31 beck Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Asn1Encoding {
    unsigned char* enc;
    std::int64_t length;
    std::int32_t modified;
};

static_assert(sizeof(Asn1Encoding) == 24);

constexpr int NullType = 5;
constexpr std::int64_t BitsLeftFlag = 0x08;

void AppendLength(std::vector<unsigned char>& output, std::size_t length) {
    if (length < 0x80) {
        output.push_back(static_cast<unsigned char>(length));
        return;
    }
    unsigned char bytes[sizeof(std::size_t)]{};
    std::size_t count = 0;
    for (std::size_t value = length; value != 0; value >>= 8) bytes[count++] = static_cast<unsigned char>(value);
    output.push_back(static_cast<unsigned char>(0x80 | count));
    while (count > 0) output.push_back(bytes[--count]);
}

void AppendTlv(std::vector<unsigned char>& output, unsigned char tag, std::span<const unsigned char> content) {
    output.push_back(tag);
    AppendLength(output, content.size());
    output.insert(output.end(), content.begin(), content.end());
}

[[noreturn]] void Unsupported(const char* what) {
    NotImplemented_nid_no_patch((std::string("i2d_X509: ") + what).c_str());
}

std::vector<unsigned char> EncodeAlgorithm(const LibreSsl3X509Algor& algorithm) {
    const auto* object = static_cast<const Asn1Object_nid_no_patch*>(algorithm.algorithm);
    if (!object) throw std::invalid_argument("i2d_X509: null signature algorithm object");
    if (!object->data || object->length <= 0) Unsupported("a signature algorithm object without encoded data");
    std::vector<unsigned char> content;
    AppendTlv(content, 0x06, std::span<const unsigned char>(object->data, static_cast<std::size_t>(object->length)));
    if (algorithm.parameter) {
        if (algorithm.parameter->type != NullType) Unsupported("signature algorithm parameters other than NULL");
        AppendTlv(content, 0x05, std::span<const unsigned char>());
    }
    std::vector<unsigned char> output;
    AppendTlv(output, 0x30, content);
    return output;
}

std::vector<unsigned char> EncodeSignature(const Asn1String& signature) {
    if (signature.length < 0 || signature.length == std::numeric_limits<int>::max())
        Unsupported("a signature with a negative or maximal length");
    const auto length = static_cast<std::size_t>(signature.length);
    if (length > 0 && !signature.data) throw std::invalid_argument("i2d_X509: null signature data");
    int bits = 0;
    if (length > 0) {
        if (signature.flags & BitsLeftFlag) {
            bits = static_cast<int>(signature.flags & 0x07);
        } else {
            const unsigned char last = signature.data[length - 1];
            if (last == 0) Unsupported("a signature with trailing zero bytes and no unused-bit count");
            while (((last >> bits) & 1) == 0) ++bits;
        }
    }
    std::vector<unsigned char> content;
    content.push_back(static_cast<unsigned char>(bits));
    if (length > 0) {
        content.insert(content.end(), signature.data, signature.data + length);
        content.back() &= static_cast<unsigned char>(0xff << bits);
    }
    std::vector<unsigned char> output;
    AppendTlv(output, 0x03, content);
    return output;
}

std::vector<unsigned char> EncodeCertificate(const LibreSsl3EncodingX509& certificate) {
    if (!certificate.certInfo) throw std::invalid_argument("i2d_X509: null certificate info");
    if (!certificate.sigAlg) throw std::invalid_argument("i2d_X509: null signature algorithm");
    if (!certificate.signature) throw std::invalid_argument("i2d_X509: null signature");
    Asn1Encoding encoding{};
    std::memcpy(&encoding, certificate.certInfo->encoding, sizeof(encoding));
    if (encoding.modified) Unsupported("certificate info without a cached unmodified encoding");
    if (encoding.length < 0 || (encoding.length > 0 && !encoding.enc))
        throw std::invalid_argument("i2d_X509: invalid cached certificate info encoding");
    std::vector<unsigned char> content(encoding.enc, encoding.enc + encoding.length);
    const auto algorithm = EncodeAlgorithm(*certificate.sigAlg);
    content.insert(content.end(), algorithm.begin(), algorithm.end());
    const auto signature = EncodeSignature(*certificate.signature);
    content.insert(content.end(), signature.begin(), signature.end());
    std::vector<unsigned char> output;
    AppendTlv(output, 0x30, content);
    return output;
}

}

extern "C" {

int APS5_VABI i2d_X509_nid_postfix(const LibreSsl3EncodingX509* certificate, unsigned char** output) {
    if (!certificate) return 0;
    const auto encoded = EncodeCertificate(*certificate);
    if (encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        Unsupported("an encoding longer than INT_MAX");
    const int length = static_cast<int>(encoded.size());
    if (!output) return length;
    if (*output) {
        std::memcpy(*output, encoded.data(), encoded.size());
        *output += encoded.size();
        return length;
    }
    auto* buffer = static_cast<unsigned char*>(calloc_nid_postfix(1, encoded.size()));
    if (!buffer) return -1;
    std::memcpy(buffer, encoded.data(), encoded.size());
    *output = buffer;
    return length;
}

}
