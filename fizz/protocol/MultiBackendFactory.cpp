/*
 *  Copyright (c) 2018-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree.
 */

#include <fizz/protocol/MultiBackendFactory.h>

#include <fizz/backend/openssl/OpenSSL.h>
#include <fizz/backend/openssl/certificate/CertUtils.h>
#include <fizz/crypto/Hkdf.h>

#include <fizz/fizz-config.h>

#if FIZZ_HAVE_OQS
#include <fizz/crypto/exchange/HybridKeyExchange.h>
#include <fizz/experimental/crypto/exchange/OQSKeyExchange.h>
#endif

#if FIZZ_BUILD_AEGIS
#include <fizz/crypto/aead/AEGISCipher.h>
#endif

namespace fizz {

std::unique_ptr<KeyExchange> MultiBackendFactory::makeKeyExchange(
    NamedGroup group,
    KeyExchangeMode mode) const {
  (void)mode;
  switch (group) {
    case NamedGroup::secp256r1:
      return fizz::openssl::makeKeyExchange<fizz::P256>();
    case NamedGroup::secp384r1:
      return fizz::openssl::makeKeyExchange<fizz::P384>();
    case NamedGroup::secp521r1:
      return fizz::openssl::makeKeyExchange<fizz::P521>();
    case NamedGroup::x25519:
      return std::make_unique<X25519KeyExchange>();
#if FIZZ_HAVE_OQS
    case NamedGroup::x25519_kyber512:
    case NamedGroup::x25519_kyber512_experimental:
      return std::make_unique<HybridKeyExchange>(
          std::make_unique<X25519KeyExchange>(),
          OQSKeyExchange::createOQSKeyExchange(mode, OQS_KEM_alg_kyber_512));
    case NamedGroup::secp256r1_kyber512:
      return std::make_unique<HybridKeyExchange>(
          fizz::openssl::makeKeyExchange<fizz::P256>(),
          OQSKeyExchange::createOQSKeyExchange(mode, OQS_KEM_alg_kyber_512));
    case NamedGroup::kyber512:
      return OQSKeyExchange::createOQSKeyExchange(mode, OQS_KEM_alg_kyber_512);
    case NamedGroup::x25519_kyber768_draft00:
    case NamedGroup::x25519_kyber768_experimental:
      return std::make_unique<HybridKeyExchange>(
          std::make_unique<X25519KeyExchange>(),
          OQSKeyExchange::createOQSKeyExchange(mode, OQS_KEM_alg_kyber_768));
    case NamedGroup::secp256r1_kyber768_draft00:
      return std::make_unique<HybridKeyExchange>(
          fizz::openssl::makeKeyExchange<fizz::P256>(),
          OQSKeyExchange::createOQSKeyExchange(mode, OQS_KEM_alg_kyber_768));
    case NamedGroup::secp384r1_kyber768:
      return std::make_unique<HybridKeyExchange>(
          fizz::openssl::makeKeyExchange<fizz::P384>(),
          OQSKeyExchange::createOQSKeyExchange(mode, OQS_KEM_alg_kyber_768));
#endif
    default:
      throw std::runtime_error("ke: not implemented");
  }
}

std::unique_ptr<Aead> MultiBackendFactory::makeAead(CipherSuite cipher) const {
  switch (cipher) {
    case CipherSuite::TLS_CHACHA20_POLY1305_SHA256:
      return openssl::OpenSSLEVPCipher::makeCipher<fizz::ChaCha20Poly1305>();
    case CipherSuite::TLS_AES_128_GCM_SHA256:
      return openssl::OpenSSLEVPCipher::makeCipher<fizz::AESGCM128>();
    case CipherSuite::TLS_AES_256_GCM_SHA384:
      return openssl::OpenSSLEVPCipher::makeCipher<fizz::AESGCM256>();
    case CipherSuite::TLS_AES_128_OCB_SHA256_EXPERIMENTAL:
      return openssl::OpenSSLEVPCipher::makeCipher<fizz::AESOCB128>();
#if FIZZ_BUILD_AEGIS
    case CipherSuite::TLS_AEGIS_256_SHA512:
      return AEGIS::make256();
    case CipherSuite::TLS_AEGIS_128L_SHA256:
      return AEGIS::make128L();
#endif
    default:
      throw std::runtime_error("aead: not implemented");
  }
}

namespace detail {
template <typename Hash>
inline std::unique_ptr<KeyDerivation> makeKeyDerivationPtr() {
  return std::unique_ptr<KeyDerivationImpl>(new KeyDerivationImpl(
      Hash::HashLen,
      &openssl::Hasher<Hash>::hash,
      &openssl::Hasher<Hash>::hmac,
      HkdfImpl(Hash::HashLen, &openssl::Hasher<Hash>::hmac),
      Hash::BlankHash));
}
} // namespace detail

std::unique_ptr<KeyDerivation> MultiBackendFactory::makeKeyDeriver(
    CipherSuite cipher) const {
  switch (cipher) {
    case CipherSuite::TLS_CHACHA20_POLY1305_SHA256:
    case CipherSuite::TLS_AES_128_GCM_SHA256:
    case CipherSuite::TLS_AES_128_OCB_SHA256_EXPERIMENTAL:
    case CipherSuite::TLS_AEGIS_128L_SHA256:
      return detail::makeKeyDerivationPtr<Sha256>();
    case CipherSuite::TLS_AES_256_GCM_SHA384:
      return detail::makeKeyDerivationPtr<Sha384>();
    case CipherSuite::TLS_AEGIS_256_SHA512:
      return detail::makeKeyDerivationPtr<Sha512>();
    default:
      throw std::runtime_error("ks: not implemented");
  }
}

std::unique_ptr<HandshakeContext> MultiBackendFactory::makeHandshakeContext(
    CipherSuite cipher) const {
  switch (cipher) {
    case CipherSuite::TLS_CHACHA20_POLY1305_SHA256:
    case CipherSuite::TLS_AES_128_GCM_SHA256:
    case CipherSuite::TLS_AES_128_OCB_SHA256_EXPERIMENTAL:
    case CipherSuite::TLS_AEGIS_128L_SHA256:
      return std::make_unique<HandshakeContextImpl<fizz::Sha256>>();
    case CipherSuite::TLS_AES_256_GCM_SHA384:
    case CipherSuite::TLS_AEGIS_256_SHA512:
      return std::make_unique<HandshakeContextImpl<fizz::Sha384>>();
    default:
      throw std::runtime_error("hs: not implemented");
  }
}

std::unique_ptr<PeerCert> MultiBackendFactory::makePeerCert(
    CertificateEntry certEntry,
    bool /*leaf*/) const {
  return openssl::CertUtils::makePeerCert(std::move(certEntry.cert_data));
}

} // namespace fizz
