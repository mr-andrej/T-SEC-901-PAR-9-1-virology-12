/*
 * tls_schannel.h - minimal TLS client over a connected socket (SChannel)
 *
 * Wraps the Windows SSPI/SChannel API behind three blocking calls:
 * tls_client_open, tls_read, tls_write. The server certificate is checked
 * against a pinned SHA-256 fingerprint instead of the system CA store, so
 * the implant trusts exactly one certificate and nothing else.
 */

#ifndef TLS_SCHANNEL_H
#define TLS_SCHANNEL_H

#define SECURITY_WIN32

#include <winsock2.h>
#include <windows.h>
#include <sspi.h>
#include <schannel.h>
#include <stdbool.h>

typedef struct {
    CredHandle cred;
    CtxtHandle ctx;
    SecPkgContext_StreamSizes sizes;
    SOCKET sock;
    CRITICAL_SECTION lock;   /* serializes EncryptMessage/DecryptMessage:
                                SSPI contexts are not thread-safe */
    unsigned char *cipher_buf; /* ciphertext received but not yet decrypted
                                  (reader thread only) */
    size_t cipher_len;
    size_t cipher_cap;
    unsigned char *plain_buf;  /* plaintext decrypted but not yet consumed */
    size_t plain_len;
    size_t plain_cap;
} tls_conn;

/* TLS handshake on an already-connected socket, then pin check.
   Returns true only if TLS is up AND the server cert matches pin_sha256. */
bool tls_client_open(tls_conn *t, SOCKET sock, const unsigned char pin_sha256[32]);

/* Copy up to len bytes of plaintext into buf.
   Returns >0 bytes read, 0 on clean shutdown, -1 on error. */
int tls_read(tls_conn *t, void *buf, int len);

/* Encrypt and send all of buf. Returns false on error. */
bool tls_write(tls_conn *t, const void *buf, int len);

/* Shutdown and release everything. Safe on a half-initialized struct. */
void tls_close(tls_conn *t);

#endif
