#ifndef BTPD_HTTP_CLIENT_H
#define BTPD_HTTP_CLIENT_H

#include "pstdint.h"
#include "winsock2.h"

#include "iobuf.h"

struct http_url {
    char *host;
    char *uri;
    uint16_t port;
};

struct http_url *http_url_parse(const char *url);
void http_url_free(struct http_url *url);

struct http_response {
    enum {
        HTTP_T_ERR, HTTP_T_CODE, HTTP_T_HEADER, HTTP_T_DATA, HTTP_T_DONE
    } type;
    union {
        int error;
        int code;
        struct {
            char *n;
            char *v;
        } header;
        struct {
            size_t l;
            char *p;
        } data;
    } v;
};

typedef void (*http_cb_t)(struct http_req *, struct http_response *, void *);

struct http_req {
    enum {
        PS_HEAD, PS_CHUNK_SIZE, PS_CHUNK_DATA, PS_CHUNK_CRLF, PS_ID_DATA
    } pstate;

    int parsing;
    int cancel;
    int chunked;
    long length;

    http_cb_t cb;
    void *arg;

    struct http_url *url;
    struct iobuf rbuf;
    struct iobuf wbuf;
};

int http_uri_encode(const wchar_t *uri,size_t inwords,char *enc_uri);
int http_get(struct http_req **out, const char *url, const char *hdrs,
    http_cb_t cb, void *arg);
void http_free(struct http_req *req);
void http_cancel(struct http_req *req);
struct http_url *http_url_get(struct http_req *req);
int http_want_read(struct http_req *req);
int http_want_write(struct http_req *req);
int http_read(struct http_req *req, SOCKET sd);
int http_write(struct http_req *req, SOCKET sd);

#endif
