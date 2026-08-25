#ifndef _CCAM3_CRYPTO_H_
#define _CCAM3_CRYPTO_H_

// ============================================================
// CCcam 3 crypto (pura C, sem OpenSSL - build musl estatica)
// Implementa o minimo do wire format do CCcam 3:
//   SHA1 (handshake legado), PBKDF2-HMAC-SHA256 (handshake RSA_AES),
//   RC4 (transporte legado), AES-256 (ECB + GCM para AES_GCM)
// ============================================================

// SHA1 - out[20]
void ccam3_sha1(const uint8_t *data, size_t len, uint8_t out[20]);

// SHA256 - out[32]
void ccam3_sha256(const uint8_t *data, size_t len, uint8_t out[32]);

// HMAC-SHA256 - out[32]
void ccam3_hmac_sha256(const uint8_t *key, size_t keylen,
                       const uint8_t *data, size_t datalen, uint8_t out[32]);

// PBKDF2-HMAC-SHA256 - outlen bytes (usa-se 32)
void ccam3_pbkdf2_sha256(const char *pass, size_t passlen,
                         const uint8_t *salt, size_t saltlen,
                         int iterations, uint8_t *out, size_t outlen);

// RC4 in-place
void ccam3_rc4(const uint8_t *key, size_t keylen, uint8_t *data, size_t len);

// AES-256 ECB (len multiplo de 16); encrypt=1 cifra, 0 decifra
void ccam3_aes256_ecb(const uint8_t key[32], uint8_t *data, size_t len, int encrypt);

// AES-256-GCM (sem AAD), IV de 12 bytes, tag de 16 bytes
// encrypt: out_ct tem o mesmo tamanho que pt
void ccam3_aes256_gcm_encrypt(const uint8_t key[32], const uint8_t iv[12],
                              const uint8_t *pt, size_t ptlen,
                              uint8_t *ct, uint8_t tag[16]);
// decrypt: devolve 0 ok, -1 tag invalido
int  ccam3_aes256_gcm_decrypt(const uint8_t key[32], const uint8_t iv[12],
                              const uint8_t *ct, size_t ctlen,
                              const uint8_t tag[16], uint8_t *pt);

// random seguro (seeds/IV) - /dev/urandom
void ccam3_randbytes(uint8_t *buf, size_t len);

#endif
