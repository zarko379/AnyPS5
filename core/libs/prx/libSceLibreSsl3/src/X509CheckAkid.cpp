/* $OpenBSD: x509_purp.c,v 1.43 2024/07/12 18:15:10 beck Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2001.
 */
/* ====================================================================
 * Copyright (c) 1999-2004 The OpenSSL Project.  All rights reserved.
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

#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <stdexcept>
#include <string>

namespace {

constexpr int VerifyOk = 0;
constexpr int AkidSkidMismatch = 30;
constexpr int AkidIssuerSerialMismatch = 31;
constexpr int DirectoryName = 4;

}

extern "C" {

int APS5_VABI X509_check_akid_nid_postfix(const LibreSsl3PurposeX509* issuer, const LibreSsl3AuthorityKeyId* akid) {
    if (!akid) return VerifyOk;
    if (akid->keyid) {
        if (!issuer) throw std::invalid_argument(std::string(__func__) + ": null issuer");
        if (issuer->skid && ASN1_OCTET_STRING_cmp_nid_postfix(akid->keyid, issuer->skid) != 0) return AkidSkidMismatch;
    }
    if (akid->serial) {
        if (!issuer) throw std::invalid_argument(std::string(__func__) + ": null issuer");
        if (!issuer->certInfo) throw std::invalid_argument(std::string(__func__) + ": null certificate info");
        const auto* serial = static_cast<const Asn1String*>(issuer->certInfo->serialNumber);
        if (!serial) throw std::invalid_argument(std::string(__func__) + ": null serial number");
        if (ASN1_OCTET_STRING_cmp_nid_postfix(serial, akid->serial) != 0) return AkidIssuerSerialMismatch;
    }
    if (const auto* names = akid->issuer) {
        for (int index = 0; index < names->num; ++index) {
            if (!names->data) throw std::invalid_argument(std::string(__func__) + ": null name array");
            const auto* name = reinterpret_cast<const LibreSslGeneralName*>(names->data[index]);
            if (!name) throw std::invalid_argument(std::string(__func__) + ": null general name");
            if (name->type == DirectoryName)
                NotImplemented_nid_no_patch("X509_check_akid: comparing an authority directory name with the issuer name");
        }
    }
    return VerifyOk;
}

}
