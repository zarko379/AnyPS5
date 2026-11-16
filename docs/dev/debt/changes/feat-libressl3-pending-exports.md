# libSceLibreSsl3

## add Functional

- [BIO_vfree and EVP_CIPHER_CTX_set_flags](../../../core/libs/prx/libSceLibreSsl3/Export.cpp) (libSceLibreSsl3) - nothing is checked on the console. They follow [LibreSSL 4.0.0](https://github.com/libressl/openbsd/tree/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto) `bio_lib.c` and `evp_cipher.c`: `BIO_vfree` releases through libSceLibreSsl's `BIO_free`, which made the BIO, and `EVP_CIPHER_CTX_set_flags` ORs the sign-extended `int` into the `flags` of the libSceLibreSsl cipher context that this module's forwarded `EVP_CIPHER_CTX_new` returns. A null context throws where LibreSSL dereferences it.
- [ERR_load_X509_strings and ERR_load_PEM_strings](../../../core/libs/prx/libSceLibreSsl3/src/ErrorStrings.cpp) (libSceLibreSsl3) - return without an effect, deduced from the firmware 13.42 module run on a PC that the `ERR_lib_error_string` entry cites: its string loader init routines are a single `ret` and every LibreSSL code stays without a string after the loaders. Not measured on a console for these two exports.
- [OpenSSL_add_all_digests](../../../core/libs/prx/libSceLibreSsl3/src/DigestNames.cpp) (libSceLibreSsl3) - returns without an effect, as in [LibreSSL 4.0.0 crypto_init.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/crypto_init.c), where the digest name table is static. Not measured on a console.
- [ERR_print_errors_fp](../../../core/libs/prx/libSceLibreSsl3/src/ErrorReport.cpp) (libSceLibreSsl3) - nothing is checked on the console. It passes each `ERR_print_errors_cb` line to the guest `FILE` as [LibreSSL 4.0.0 err_prn.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/err/err_prn.c) `print_fp` does, writing with `fwrite` instead of `fprintf("%.*s")`; a short write returns -1 and stops the report, leaving the later errors queued. An empty queue never touches the stream.
- [SSL_is_quic, SSL_CTX_set_quic_method and SSL_set_quic_transport_params](../../../core/libs/prx/libSceLibreSsl3/src/Ssl.cpp) (libSceLibreSsl3) - nothing is checked on the console. They follow [LibreSSL 4.0.0 ssl_lib.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libssl/ssl_lib.c) on the `ssl_local.h` offsets this module models: a DTLS method refuses a QUIC method, and the old transport parameters are zeroed and freed before a copy from the guest heap replaces them, also for a length of 0. Null objects or methods and null parameters with a nonzero length throw.
- [EVP_PKEY_CTX_new_id](../../../core/libs/prx/libSceLibreSsl3/src/PkeyContext.cpp) (libSceLibreSsl3) - nothing is checked on the console. Like [LibreSSL 4.0.0 pmeth_lib.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/evp/pmeth_lib.c) it ignores the engine and makes a key-less context for the RSA, RSA-PSS and EC methods; other NIDs queue `EVP_R_UNSUPPORTED_ALGORITHM` and return null, except the CMAC, DH, DSA, Ed25519, HKDF, HMAC, TLS1-PRF and X25519 methods of the 4.0.0 table, which this module does not model and which throw.

- [i2d_PUBKEY](../../../core/libs/prx/libSceLibreSsl3/src/KeyDer.cpp) (libSceLibreSsl3) - nothing is checked on the console. It encodes a SubjectPublicKeyInfo as [LibreSSL 4.0.0](https://github.com/libressl/openbsd/tree/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto) `x_pubkey.c`, `rsa_ameth.c` and `ec_ameth.c` do (rsaEncryption with NULL parameters; id-ecPublicKey with the named-curve OID and the point in the key's conversion form), returning 0 for a null key and allocating the output when `*out` is null. EC keys are always written with a named curve, as `EVP_PKEY2PKCS8` here does, because libSceLibreSsl groups do not record whether the caller cleared `OPENSSL_EC_NAMED_CURVE`. Other key types, curves other than P-256 and P-384, RSA keys without a nonnegative modulus and exponent and EC keys without a group or public point throw.

- [ECDSA_do_sign and ECDSA_do_verify](../../../core/libs/prx/libSceLibreSsl3/src/EcdsaSign.cpp) (libSceLibreSsl3) - nothing is checked on the console. The checks and their errors follow [LibreSSL 4.0.0 ecdsa.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/ecdsa/ecdsa.c): signing without a group returns null quietly and without a private key queues `ERR_R_PASSED_NULL_PARAMETER`; verifying a null signature or a key without group or public point queues `EC_R_MISSING_PARAMETERS` and returns -1, and r or s outside [1, order) queues `EC_R_BAD_SIGNATURE` and returns 0. The arithmetic runs through libSceLibreSsl's `ECDSA_sign` and `ECDSA_verify` on the DER form, so their throws (keys off the curve, private keys that are a multiple of the order) apply. Null keys, null r or s and null digests with a length throw where LibreSSL dereferences them.

- [X509_check_akid](../../../core/libs/prx/libSceLibreSsl3/src/X509CheckAkid.cpp) (libSceLibreSsl3) - nothing is checked on the console. It follows [LibreSSL 4.0.0 x509_purp.c](https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/x509/x509_purp.c) on the 216-byte `X509` layout of `x509_local.h` (cached `skid` at offset 88): a key identifier differing from the cached one returns `X509_V_ERR_AKID_SKID_MISMATCH`, a serial differing from the issuer's returns `X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH`. Nothing fills `skid`, which LibreSSL caches when it parses extensions, so only certificates whose caller set it compare key identifiers. A directory name in the authority issuer names throws, because `X509_NAME_cmp` is not modelled; null objects throw where LibreSSL dereferences them.

## change libSceLibreSsl3 pending exports

- [libSceLibreSsl3 pending exports](../../../core/libs/prx/libSceLibreSsl3/Export.cpp) - 10 named callable exports of the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibreSsl3.c), cross-checked against the same module in PS5 firmware 13.60, remain unimplemented: `EVP_PKEY_new_mac_key`, `OCSP_cert_to_id`, `PEM_read_RSAPrivateKey`, `PKCS7_free`, `SSL_CTX_add0_chain_cert`, `SSL_CTX_clear_chain_certs`, `d2i_PKCS7`, `d2i_RSAPublicKey_bio`, `d2i_RSA_PUBKEY` and `d2i_X509_bio`. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.

## change Additional exports

- [Additional exports](../../../core/libs/prx/libSceLibreSsl3/AdditionalExports.cpp) declare 37 still-unimplemented callable exports: 36 named and 1 known only by NID. These entries throw. Parameterless `int` declarations are unknown-signature placeholders, not verified ABI contracts. Names/NIDs match the 13.60 export table. The fourteen libtls lifecycle/config exports are implemented in `LibTls.cpp`; title compatibility beyond preconnection behavior remains unverified.

## remove xpB+BNkO2Z0

## remove xf6anoZpWRM

## remove ZFeTGo4ybcw

## remove A0lZtFjhcTw

## remove M7ze0mz2jQM

## remove O9Thca3enw8

## remove 4pEl-5DmrrM

## remove J5ZLHxWkT6k

## remove A-Hs-8mau+8

## remove ba4o0Ih6kSs

## remove qGsc6uT6p1E

## remove C7eFtWPfLdk

## change formerly NID-only exports

- The formerly NID-only exports named in the inventory above and in [API declarations](../../../core/libs/prx/libSceLibreSsl3/include/Api.hpp) use LibreSSL names whose `tools/nid_names.py` hash equals the NID; the names were taken from the identifiers of the LibreSSL 3.8.4 and 4.0.0 `include`, `crypto`, `ssl` and `tls` sources. A name match does not show that the console function has LibreSSL's prototype or behavior. Unimplemented entries keep the no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`; the implemented contracts and remaining gaps are listed above. `AES_wrap_key`, `ECDSA_SIG_free`, `ECDSA_do_verify`, `ERR_load_ERR_strings`, `ERR_print_errors_fp` and `i2d_PUBKEY` also hash to the NIDs of six formerly `unknown` rows; `PBYz0kpXwg8` hashes to no identifier of those sources.
