/**
TLS Peer Verification Helper

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_BYBIT_TLS_VERIFY_H
#define INCLUDE_STONKY_BYBIT_TLS_VERIFY_H

#include <boost/asio/ssl/context.hpp>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#endif

namespace stonky::bybit {

/**
 * Turn on TLS peer-certificate verification for a client context. Without this
 * asio defaults to verify_none: the connection is encrypted but the peer is
 * unauthenticated, so any MITM can impersonate the venue. Pair with a
 * boost::asio::ssl::host_name_verification callback on the stream/context so
 * the verified certificate is also bound to the expected hostname.
 *
 * On Windows OpenSSL cannot see the system certificate store, so the ROOT
 * store is imported into the context explicitly; on POSIX the default CA
 * paths (ca-certificates) are used.
 */
inline void enableTlsPeerVerification(boost::asio::ssl::context &ctx) {
    ctx.set_default_verify_paths();

#ifdef _WIN32
    if (const HCERTSTORE store = CertOpenSystemStoreW(0, L"ROOT")) {
        X509_STORE *x509Store = SSL_CTX_get_cert_store(ctx.native_handle());
        PCCERT_CONTEXT cert = nullptr;

        while ((cert = CertEnumCertificatesInStore(store, cert)) != nullptr) {
            const unsigned char *der = cert->pbCertEncoded;
            if (X509 *x509 = d2i_X509(nullptr, &der, static_cast<long>(cert->cbCertEncoded))) {
                X509_STORE_add_cert(x509Store, x509); // duplicate adds are harmless
                X509_free(x509);
            }
        }

        CertCloseStore(store, 0);
    }
#endif

    ctx.set_verify_mode(boost::asio::ssl::verify_peer);
}

} // namespace stonky::bybit

#endif // INCLUDE_STONKY_BYBIT_TLS_VERIFY_H
