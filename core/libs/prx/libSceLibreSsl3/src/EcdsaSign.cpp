/* $OpenBSD: ecdsa.c,v 1.19 2024/04/15 15:49:37 tb Exp $ */
/* ====================================================================
 * Copyright (c) 2000-2002 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int EcLibrary = 16;
constexpr int AnyFunction = 0xfff;
constexpr int ReasonEcLibrary = 16;
constexpr int ReasonPassedNullParameter = 67;
constexpr int ReasonPointAtInfinity = 106;
constexpr int ReasonMissingParameters = 124;
constexpr int ReasonBadSignature = 166;

void PutEcError(int reason) {
    LibreSslPutError_nid_no_patch(EcLibrary, AnyFunction, reason);
}

void CheckDigest(const unsigned char* digest, int digestLength, const char* function) {
    if (digestLength < 0 || (!digest && digestLength > 0))
        throw std::invalid_argument(std::string(function) + ": invalid digest");
}

std::vector<unsigned char> Magnitude(const Bignum& value) {
    const int bits = BN_num_bits_nid_postfix(&value);
    std::vector<unsigned char> bytes(static_cast<std::size_t>((bits + 7) / 8));
    if (!bytes.empty()) BN_bn2bin_nid_postfix(&value, bytes.data());
    return bytes;
}

std::vector<unsigned char> OrderBytes(const EcGroup& group) {
    std::vector<unsigned char> bytes(mbedtls_mpi_size(&group.group.N));
    if (mbedtls_mpi_write_binary(&group.group.N, bytes.data(), bytes.size()) != 0)
        throw std::runtime_error("ECDSA_do_verify: group order encoding");
    return bytes;
}

bool InSignatureRange(const Bignum& value, const std::vector<unsigned char>& order) {
    const auto bytes = Magnitude(value);
    if (bytes.empty() || value.neg) return false;
    if (bytes.size() != order.size()) return bytes.size() < order.size();
    return std::memcmp(bytes.data(), order.data(), bytes.size()) < 0;
}

void AppendInteger(std::vector<unsigned char>& output, const std::vector<unsigned char>& magnitude) {
    std::vector<unsigned char> content;
    if (magnitude.front() & 0x80) content.push_back(0);
    content.insert(content.end(), magnitude.begin(), magnitude.end());
    output.push_back(0x02);
    if (content.size() < 0x80) {
        output.push_back(static_cast<unsigned char>(content.size()));
    } else {
        output.push_back(0x81);
        output.push_back(static_cast<unsigned char>(content.size()));
    }
    output.insert(output.end(), content.begin(), content.end());
}

bool ReadInteger(const std::vector<unsigned char>& der, std::size_t& offset, std::vector<unsigned char>& magnitude) {
    if (offset + 2 > der.size() || der[offset] != 0x02) return false;
    std::size_t length = der[offset + 1];
    offset += 2;
    if (length == 0x81) {
        if (offset >= der.size()) return false;
        length = der[offset++];
    } else if (length > 0x80) {
        return false;
    }
    if (length == 0 || offset + length > der.size()) return false;
    std::size_t start = offset;
    while (start + 1 < offset + length && der[start] == 0) ++start;
    magnitude.assign(der.begin() + static_cast<std::ptrdiff_t>(start),
        der.begin() + static_cast<std::ptrdiff_t>(offset + length));
    offset += length;
    return true;
}

bool SplitSignature(const std::vector<unsigned char>& der, std::vector<unsigned char>& r, std::vector<unsigned char>& s) {
    if (der.size() < 2 || der[0] != 0x30) return false;
    std::size_t offset = 2;
    if (der[1] == 0x81) offset = 3;
    else if (der[1] > 0x80) return false;
    return ReadInteger(der, offset, r) && ReadInteger(der, offset, s) && offset == der.size();
}

}

extern "C" {

LibreSsl3EcdsaSig* APS5_VABI ECDSA_do_sign_nid_postfix(const unsigned char* digest, int digestLength, EcKey* key) {
    if (!key) throw std::invalid_argument("ECDSA_do_sign: null key");
    CheckDigest(digest, digestLength, "ECDSA_do_sign");
    if (!key->group) return nullptr;
    if (!key->privateKey) {
        PutEcError(ReasonPassedNullParameter);
        return nullptr;
    }
    const int size = ECDSA_size_nid_postfix(key);
    if (size <= 0) throw std::runtime_error("ECDSA_do_sign: signature size");
    std::vector<unsigned char> der(static_cast<std::size_t>(size));
    unsigned int length = 0;
    if (ECDSA_sign_nid_postfix(0, digest, digestLength, der.data(), &length, key) != 1 || length > der.size())
        throw std::runtime_error("ECDSA_do_sign: libSceLibreSsl ECDSA_sign failed");
    der.resize(length);
    std::vector<unsigned char> rBytes;
    std::vector<unsigned char> sBytes;
    if (!SplitSignature(der, rBytes, sBytes)) throw std::runtime_error("ECDSA_do_sign: signature encoding");
    Bignum* r = BN_bin2bn_nid_postfix(rBytes.data(), static_cast<int>(rBytes.size()), nullptr);
    Bignum* s = r ? BN_bin2bn_nid_postfix(sBytes.data(), static_cast<int>(sBytes.size()), nullptr) : nullptr;
    LibreSsl3EcdsaSig* signature = s ? ECDSA_SIG_new_nid_postfix() : nullptr;
    if (!signature) {
        if (r && s) PutEcError(65);
        BN_free_nid_postfix(r);
        BN_free_nid_postfix(s);
        return nullptr;
    }
    ECDSA_SIG_set0_nid_postfix(signature, r, s);
    return signature;
}

int APS5_VABI ECDSA_do_verify_nid_postfix(const unsigned char* digest, int digestLength, const LibreSsl3EcdsaSig* signature,
                                          EcKey* key) {
    if (!key) throw std::invalid_argument("ECDSA_do_verify: null key");
    if (!signature || !key->group || !key->publicKey) {
        PutEcError(ReasonMissingParameters);
        return -1;
    }
    const auto* r = static_cast<const Bignum*>(signature->r);
    const auto* s = static_cast<const Bignum*>(signature->s);
    if (!r || !s) throw std::invalid_argument("ECDSA_do_verify: signature without r or s");
    const auto order = OrderBytes(*key->group);
    if (!InSignatureRange(*r, order) || !InSignatureRange(*s, order)) {
        PutEcError(ReasonBadSignature);
        return 0;
    }
    CheckDigest(digest, digestLength, "ECDSA_do_verify");
    std::vector<unsigned char> body;
    AppendInteger(body, Magnitude(*r));
    AppendInteger(body, Magnitude(*s));
    std::vector<unsigned char> der{0x30};
    if (body.size() >= 0x80) der.push_back(0x81);
    der.push_back(static_cast<unsigned char>(body.size()));
    der.insert(der.end(), body.begin(), body.end());
    const int result = ECDSA_verify_nid_postfix(0, digest, digestLength, der.data(), static_cast<int>(der.size()), key);
    if (result < 0) {
        PutEcError(ReasonPointAtInfinity);
        PutEcError(ReasonEcLibrary);
    }
    return result;
}

}
