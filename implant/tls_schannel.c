/*
 * tls_schannel.c - see tls_schannel.h for the contract.
 *
 * Build links: -lsecur32 -lbcrypt
 */

#include "tls_schannel.h"

#include <wincrypt.h>
#include <bcrypt.h>
#include <stdlib.h>
#include <string.h>

#define CIPHER_CHUNK 16384

static bool grow(unsigned char **buf, size_t *cap, size_t need)
{
    if (need <= *cap)
        return true;
    unsigned char *p = realloc(*buf, need);
    if (!p)
        return false;
    *buf = p;
    *cap = need;
    return true;
}

/* Pull more ciphertext from the socket into the staging buffer (blocking).
   Returns false when the connection is dead. */
static bool fill_cipher(tls_conn *t)
{
    if (!grow(&t->cipher_buf, &t->cipher_cap, t->cipher_len + CIPHER_CHUNK))
        return false;
    int n = recv(t->sock, (char *)t->cipher_buf + t->cipher_len,
                 (int)(t->cipher_cap - t->cipher_len), 0);
    if (n <= 0)
        return false;
    t->cipher_len += (size_t)n;
    return true;
}

static bool send_all(SOCKET sock, const unsigned char *buf, size_t len)
{
    while (len > 0) {
        int n = send(sock, (const char *)buf, (int)len, 0);
        if (n <= 0)
            return false;
        buf += n;
        len -= (size_t)n;
    }
    return true;
}

static bool acquire_client_credentials(tls_conn *t)
{
    SCHANNEL_CRED sc = { 0 };
    sc.dwVersion = SCHANNEL_CRED_VERSION;
    /* We verify the certificate ourselves via the SHA-256 pin. */
    sc.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;

    return AcquireCredentialsHandleA(NULL, (SEC_CHAR *)UNISP_NAME_A,
                                     SECPKG_CRED_OUTBOUND, NULL, &sc,
                                     NULL, NULL, &t->cred, NULL) == SEC_E_OK;
}

/* Keep the SECBUFFER_EXTRA remainder, if any, at the front of cipher_buf. */
static void keep_extra(tls_conn *t, const SecBuffer *extra)
{
    if (extra && extra->BufferType == SECBUFFER_EXTRA) {
        memmove(t->cipher_buf,
                t->cipher_buf + (t->cipher_len - extra->cbBuffer),
                extra->cbBuffer);
        t->cipher_len = extra->cbBuffer;
    } else {
        t->cipher_len = 0;
    }
}

static bool handshake(tls_conn *t)
{
    const DWORD flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT |
                        ISC_REQ_CONFIDENTIALITY | ISC_REQ_ALLOCATE_MEMORY |
                        ISC_REQ_STREAM | ISC_REQ_USE_SUPPLIED_CREDS;
    bool started = false;

    for (;;) {
        SecBuffer in[2] = {
            { (unsigned long)t->cipher_len, SECBUFFER_TOKEN, t->cipher_buf },
            { 0, SECBUFFER_EMPTY, NULL },
        };
        SecBufferDesc in_desc = { SECBUFFER_VERSION, 2, in };
        SecBuffer out[1] = { { 0, SECBUFFER_TOKEN, NULL } };
        SecBufferDesc out_desc = { SECBUFFER_VERSION, 1, out };
        DWORD ret_flags = 0;

        SECURITY_STATUS ss = InitializeSecurityContextA(
            &t->cred, started ? &t->ctx : NULL, NULL, flags, 0, 0,
            started ? &in_desc : NULL, 0,
            started ? NULL : &t->ctx, &out_desc, &ret_flags, NULL);

        if (out[0].cbBuffer > 0 && out[0].pvBuffer) {
            bool sent = send_all(t->sock, out[0].pvBuffer, out[0].cbBuffer);
            FreeContextBuffer(out[0].pvBuffer);
            if (!sent)
                return false;
        }

        if (ss == SEC_E_OK) {
            keep_extra(t, &in[1]);
            return true;
        }
        if (ss == SEC_I_CONTINUE_NEEDED || ss == SEC_E_INCOMPLETE_MESSAGE) {
            keep_extra(t, &in[1]);
            if (!fill_cipher(t))
                return false;
            started = true;
            continue;
        }
        return false; /* SEC_E_* hard failure */
    }
}

/* SHA-256 of the server certificate's DER bytes, via CNG. */
static bool cert_sha256(const unsigned char *der, DWORD der_len,
                        unsigned char out[32])
{
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE h = NULL;
    bool ok = false;

    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM,
                                    NULL, 0) != 0)
        return false;
    if (BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0) == 0 &&
        BCryptHashData(h, (PUCHAR)der, (ULONG)der_len, 0) == 0 &&
        BCryptFinishHash(h, out, 32, 0) == 0)
        ok = true;
    if (h)
        BCryptDestroyHash(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

static bool verify_pin(tls_conn *t, const unsigned char pin[32])
{
    PCCERT_CONTEXT cert = NULL;
    bool ok = false;

    if (QueryContextAttributesA(&t->ctx, SECPKG_ATTR_REMOTE_CERT_CONTEXT,
                                &cert) != SEC_E_OK || !cert)
        return false;

    unsigned char hash[32];
    if (cert_sha256(cert->pbCertEncoded, cert->cbCertEncoded, hash))
        ok = memcmp(hash, pin, 32) == 0;

    CertFreeCertificateContext(cert);
    return ok;
}

bool tls_client_open(tls_conn *t, SOCKET sock, const unsigned char pin[32])
{
    memset(t, 0, sizeof(*t));
    t->sock = sock;

    if (!acquire_client_credentials(t))
        return false;
    if (!handshake(t))
        return false;
    if (!verify_pin(t, pin))
        return false;
    if (QueryContextAttributesA(&t->ctx, SECPKG_ATTR_STREAM_SIZES,
                                &t->sizes) != SEC_E_OK)
        return false;
    return true;
}

bool tls_write(tls_conn *t, const void *data, int len)
{
    const unsigned char *p = data;

    while (len > 0) {
        int chunk = len < (int)t->sizes.cbMaximumMessage
                        ? len : (int)t->sizes.cbMaximumMessage;
        size_t total = t->sizes.cbHeader + (size_t)chunk + t->sizes.cbTrailer;
        unsigned char *msg = malloc(total);
        if (!msg)
            return false;
        memcpy(msg + t->sizes.cbHeader, p, (size_t)chunk);

        SecBuffer bufs[4] = {
            { t->sizes.cbHeader, SECBUFFER_STREAM_HEADER, msg },
            { (unsigned long)chunk, SECBUFFER_DATA, msg + t->sizes.cbHeader },
            { t->sizes.cbTrailer, SECBUFFER_STREAM_TRAILER,
              msg + t->sizes.cbHeader + chunk },
            { 0, SECBUFFER_EMPTY, NULL },
        };
        SecBufferDesc desc = { SECBUFFER_VERSION, 4, bufs };

        SECURITY_STATUS ss = EncryptMessage(&t->ctx, 0, &desc, 0);
        if (ss != SEC_E_OK) {
            free(msg);
            return false;
        }

        size_t wire = bufs[0].cbBuffer + bufs[1].cbBuffer + bufs[2].cbBuffer;
        bool sent = send_all(t->sock, msg, wire);
        free(msg);
        if (!sent)
            return false;

        p += chunk;
        len -= chunk;
    }
    return true;
}

/* Try to decrypt one record from the staged ciphertext into plain_buf.
   Returns: 1 = plaintext available, 0 = need more ciphertext,
   -1 = clean close, -2 = error. */
static int try_decrypt(tls_conn *t)
{
    SecBuffer bufs[4] = {
        { (unsigned long)t->cipher_len, SECBUFFER_DATA, t->cipher_buf },
        { 0, SECBUFFER_EMPTY, NULL },
        { 0, SECBUFFER_EMPTY, NULL },
        { 0, SECBUFFER_EMPTY, NULL },
    };
    SecBufferDesc desc = { SECBUFFER_VERSION, 4, bufs };

    SECURITY_STATUS ss = DecryptMessage(&t->ctx, &desc, 0, NULL);

    if (ss == SEC_E_OK || ss == SEC_I_CONTEXT_EXPIRED) {
        SecBuffer *data = NULL, *extra = NULL;
        for (int i = 0; i < 4; i++) {
            if (bufs[i].BufferType == SECBUFFER_DATA)
                data = &bufs[i];
            else if (bufs[i].BufferType == SECBUFFER_EXTRA)
                extra = &bufs[i];
        }
        if (data && data->cbBuffer > 0) {
            if (!grow(&t->plain_buf, &t->plain_cap,
                      t->plain_len + data->cbBuffer))
                return -2;
            memcpy(t->plain_buf + t->plain_len, data->pvBuffer,
                   data->cbBuffer);
            t->plain_len += data->cbBuffer;
        }
        size_t used = extra ? t->cipher_len - extra->cbBuffer : t->cipher_len;
        memmove(t->cipher_buf, t->cipher_buf + used, t->cipher_len - used);
        t->cipher_len -= used;
        return ss == SEC_E_OK ? 1 : -1;
    }
    if (ss == SEC_E_INCOMPLETE_MESSAGE)
        return 0;
    if (ss == SEC_I_RENEGOTIATE)
        return -2; /* post-handshake messages are refused by design */
    return -2;
}

static int serve_plaintext(tls_conn *t, void *buf, int len)
{
    size_t n = t->plain_len < (size_t)len ? t->plain_len : (size_t)len;
    memcpy(buf, t->plain_buf, n);
    memmove(t->plain_buf, t->plain_buf + n, t->plain_len - n);
    t->plain_len -= n;
    return (int)n;
}

/* Is ciphertext waiting on the socket right now? */
static int socket_state(tls_conn *t)
{
    fd_set rfds;
    struct timeval tv = { 0, 0 };

    FD_ZERO(&rfds);
    FD_SET(t->sock, &rfds);
    return select(0, &rfds, NULL, NULL, &tv); /* >0 readable, 0 idle, <0 dead */
}

int tls_read_nb(tls_conn *t, void *buf, int len)
{
    if (t->plain_len > 0)
        return serve_plaintext(t, buf, len);

    int r = try_decrypt(t);
    if (r == -1 || r == -2)
        return -1;
    if (t->plain_len > 0)
        return serve_plaintext(t, buf, len);

    if (r == 0) { /* need more ciphertext: check the socket without blocking */
        int s = socket_state(t);
        if (s < 0)
            return -1;
        if (s == 0)
            return 0;
        if (!fill_cipher(t))
            return -1;
        r = try_decrypt(t);
        if (r == -1 || r == -2)
            return -1;
    }
    if (t->plain_len > 0)
        return serve_plaintext(t, buf, len);
    return 0; /* partial record: nothing to give yet */
}

void tls_close(tls_conn *t)
{
    if (t->ctx.dwLower || t->ctx.dwUpper) {
        SecBuffer buf = { 0, SECBUFFER_TOKEN, NULL };
        SecBufferDesc desc = { SECBUFFER_VERSION, 1, &buf };

        ApplyControlToken(&t->ctx, &desc);
        buf.pvBuffer = NULL;
        buf.cbBuffer = 0;
        DWORD ret_flags;
        if (InitializeSecurityContextA(&t->cred, &t->ctx, NULL,
                                       ISC_REQ_ALLOCATE_MEMORY, 0, 0,
                                       NULL, 0, &t->ctx, &desc,
                                       &ret_flags, NULL) == SEC_E_OK &&
            buf.pvBuffer && buf.cbBuffer > 0) {
            send_all(t->sock, buf.pvBuffer, buf.cbBuffer);
            FreeContextBuffer(buf.pvBuffer);
        }
        DeleteSecurityContext(&t->ctx);
        memset(&t->ctx, 0, sizeof(t->ctx));
    }
    if (t->cred.dwLower || t->cred.dwUpper) {
        FreeCredentialsHandle(&t->cred);
        memset(&t->cred, 0, sizeof(t->cred));
    }
    free(t->cipher_buf);
    free(t->plain_buf);
    t->cipher_buf = t->plain_buf = NULL;
    t->cipher_len = t->cipher_cap = t->plain_len = t->plain_cap = 0;
}
