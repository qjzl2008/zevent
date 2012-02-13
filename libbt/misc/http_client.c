#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include "pstdint.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iobuf.h"
#include "subr.h"
#include "http_client.h"
#include "msvc_c99.h"

struct http_url *
http_url_parse(const char *url)
{
    const char *cur, *at, *uri = NULL,*uri_e = NULL;
    const char *host = NULL,*host_e = NULL;
    const char *port = NULL, *port_e = NULL;

    struct http_url *res = NULL;
    size_t ulen = strlen(url);
    if (strncmp(url, "http://", 7) != 0)
        return NULL;
    at = strchr(url + 7, '@');
    uri = strchr(url + 7, '/');
    cur = strchr(url + 7, '?');
    if (cur != NULL && (uri == NULL || cur < uri))
        uri = cur;
    if (uri == NULL)
        uri = url + ulen;
    if (at != NULL && at < uri)
        host = at + 1;
    else
        host = url + 7;
    cur = host;
    while (cur < uri && *cur != ':')
        cur++;
    host_e = cur;
    if (host_e == host)
        return NULL;
    if (*cur == ':') {
        cur++;
        port = cur;
        while (cur < uri && *cur >= '0' && *cur <= '9')
            cur++;
        if (cur == port || cur != uri)
            return NULL;
        port_e = cur;
    }
    while (*cur != '\0')
        cur++;
    uri_e = cur;
    res =
        malloc(sizeof(*res) + host_e - host + 1 + uri_e - uri + 2);
    if (res == NULL)
        return NULL;
    if (port != NULL)
        sscanf(port, "%hu", &res->port);
    else
        res->port = 80;
    res->host = (char *)(res + 1);
    res->uri = res->host + (host_e - host + 1);
    memcpy(res->host, host, host_e - host);
    res->host[host_e - host] = '\0';
    if (*uri != '/') {
        res->uri[0] = '/';
        memcpy(res->uri + 1, uri, uri_e - uri);
        res->uri[uri_e - uri + 1] = '\0';
    } else {
        memcpy(res->uri, uri, uri_e - uri);
        res->uri[uri_e - uri] = '\0';
    }
    return res;
}

void
http_url_free(struct http_url *url)
{
    free(url);
}

void
http_free(struct http_req *req)
{
    if (req->url != NULL)
        http_url_free(req->url);
    iobuf_free(&req->rbuf);
    iobuf_free(&req->wbuf);
    free(req);
}

static void
http_error(struct http_req *req)
{
    struct http_response res;
    res.type = HTTP_T_ERR;
    res.v.error = 1;
    req->cb(req, &res, req->arg);
    http_free(req);
}

static char *
strnl(char *str, int *nlsize)
{
    char *nl = strchr(str, '\n');
    if (nl != NULL && nl > str && *(nl - 1) == '\r') {
        *nlsize = 2;
        return nl - 1;
    } else {
        *nlsize = 1;
        return nl;
    }
}

static int
headers_parse(struct http_req *req, char *buf, char *end)
{
    int code, majv, minv, nlsize;
    char *cur, *nl;
    char name[128], value[872];
    struct http_response res;

    req->chunked = 0;
    req->length = -1;

    if (sscanf(buf, "HTTP/%d.%d %d", &majv, &minv, &code) != 3)
        return 0;
    res.type = HTTP_T_CODE;
    res.v.code = code;
    req->cb(req, &res, req->arg);
    if (req->cancel)
        return 1;

    cur = strchr(buf, '\n') + 1;
    nl = strnl(cur, &nlsize);
    while (cur < end) {
        int i;
        char *colon = strchr(cur, ':');
        if (colon == NULL || colon > nl)
            return 0;
        sprintf(name, "%.*s", (int)(colon - cur), cur);

        cur = colon + 1;
        i = 0;
    val_loop:
        while (isspace(*cur))
            cur++;
        while (cur < nl) {
            if (i < sizeof(value) - 1) {
                value[i] = *cur;
                i++;
            }
            cur++;
        }
        cur += nlsize;
        nl = strnl(cur, &nlsize);
        if (isspace(*cur)) {
            if (i < sizeof(value) - 1) {
                value[i] = ' ';
                i++;
            }
            cur++;
            goto val_loop;
        }
        value[i] = '\0';
        for (i--; i >= 0 && isspace(value[i]); i--)
            value[i] = '\0';

        res.type = HTTP_T_HEADER;
        res.v.header.n = name;
        res.v.header.v = value;
        req->cb(req, &res, req->arg);
        if (req->cancel)
            return 1;
        if ((!req->chunked
                && strcasecmp("Transfer-Encoding", name) == 0
                && strcasecmp("chunked", value) == 0))
            req->chunked = 1;
        if ((!req->chunked && req->length == -1
                && strcasecmp("Content-Length", name) == 0)) {
            errno = 0;
            req->length = strtol(value, NULL, 10);
            if (errno)
                req->length = -1;
        }
    }
    if (req->chunked)
        req->pstate = PS_CHUNK_SIZE;
    else
        req->pstate = PS_ID_DATA;
    return 1;
}

static int
http_parse(struct http_req *req, int len)
{
    char *end, *numend;
    size_t dlen;
    struct http_response res;
again:
    switch (req->pstate) {
    case PS_HEAD:
        if (len == 0)
            goto error;
        if ((end = iobuf_find(&req->rbuf, "\r\n\r\n", 4)) != NULL)
            dlen = 4;
        else if ((end = iobuf_find(&req->rbuf, "\n\n", 2)) != NULL)
            dlen = 2;
        else {
            if (req->rbuf.off < (1 << 15))
                return 1;
            else
                goto error;
        }

        if (!iobuf_write(&req->rbuf, "", 1))
            goto error;
        req->rbuf.off--;
        if (!headers_parse(req, req->rbuf.buf, end))
            goto error;
        if (req->cancel)
            goto cancel;
        iobuf_consumed(&req->rbuf, end - (char *)req->rbuf.buf + dlen);
        goto again;
    case PS_CHUNK_SIZE:
        assert(req->chunked);
        if (len == 0)
            goto error;
        if ((end = iobuf_find(&req->rbuf, "\n", 1)) == NULL) {
            if (req->rbuf.off < 20)
                return 1;
            else
                goto error;
        }
        errno = 0;
        req->length = strtol(req->rbuf.buf, &numend, 16);
        if (req->length < 0 || numend == (char *)req->rbuf.buf || errno)
            goto error;
        if (req->length == 0)
            goto done;
        iobuf_consumed(&req->rbuf, end - (char *)req->rbuf.buf + 1);
        req->pstate = PS_CHUNK_DATA;
        goto again;
    case PS_CHUNK_DATA:
        if (len == 0)
            goto error;
        assert(req->length > 0);
        dlen = min(req->rbuf.off, req->length);
        if (dlen > 0) {
            res.type = HTTP_T_DATA;
            res.v.data.l = dlen;
            res.v.data.p = req->rbuf.buf;
            req->cb(req, &res, req->arg);
            if (req->cancel)
                goto cancel;
            iobuf_consumed(&req->rbuf, dlen);
            req->length -= dlen;
            if (req->length == 0) {
                req->pstate = PS_CHUNK_CRLF;
                goto again;
            }
        }
        return 1;
    case PS_CHUNK_CRLF:
        if (len == 0)
            goto error;
        assert(req->length == 0);
        if (req->rbuf.off < 2)
            return 1;
        if (req->rbuf.buf[0] == '\r' && req->rbuf.buf[1] == '\n')
            dlen = 2;
        else if (req->rbuf.buf[0] == '\n')
            dlen = 1;
        else
            goto error;
        iobuf_consumed(&req->rbuf, dlen);
        req->pstate = PS_CHUNK_SIZE;
        goto again;
    case PS_ID_DATA:
        if (len == 0 && req->length < 0)
            goto done;
        else if (len == 0)
            goto error;
        if (req->length < 0)
            dlen = req->rbuf.off;
        else
            dlen = min(req->rbuf.off, req->length);
        if (dlen > 0) {
            res.type = HTTP_T_DATA;
            res.v.data.p = req->rbuf.buf;
            res.v.data.l = dlen;
            req->cb(req, &res, req->arg);
            if (req->cancel)
                goto cancel;
            iobuf_consumed(&req->rbuf, dlen);
            if (req->length > 0) {
                req->length -= dlen;
                if (req->length == 0)
                    goto done;
            }
        }
        return 1;
    default:
        abort();
    }
error:
    http_error(req);
    return 0;
done:
    res.type = HTTP_T_DONE;
    req->cb(req, &res, req->arg);
cancel:
    http_free(req);
    return 0;
}

struct http_url *
http_url_get(struct http_req *req)
{
    return req->url;
}

int
http_want_read(struct http_req *req)
{
    return 1;
}

int
http_want_write(struct http_req *req)
{
    return req->wbuf.off > 0;
}

int
http_read(struct http_req *req, SOCKET sd)
{
    ssize_t rv;
    WSABUF wsaData;
    DWORD dwBytes = 0;
    DWORD dwLeft = 0;
    DWORD flags = 0;
    size_t len = 4096;

    const char *tok = "Content-Length:";
    const char *start = NULL;
    const char *end = NULL;
    char length[64];
    int nlength = 0;
    int total = 0,hlen = 0;
    int total_len = 0;
    memset(length,0,sizeof(length));

    if (!iobuf_accommodate(&req->rbuf, len)) {
        http_error(req);
        return 0;
    }

    wsaData.len = (u_long)len;
    wsaData.buf = (char *)req->rbuf.buf;
    rv = WSARecv(sd, &wsaData, 1, &dwBytes, &flags, NULL, NULL);
    if(rv == SOCKET_ERROR) {
	http_error(req);
	return 0;
    }
    else{
	if(dwBytes == 0)
	{
	    http_error(req);
	    return 0;
	}
	start = strstr(req->rbuf.buf,tok);
	if(start)
	{
	    end = strstr(start,"\r\n");
	    if(!end)
	    {
		end = strstr(start,"\n");
	    }
	    if(end)
	    {
		memcpy(length,start + strlen(tok), end-start-strlen(tok));
		nlength = atoi(length);

		end = strstr(start,"\r\n\r\n");
		if(end)
		{
		    hlen = end - req->rbuf.buf + 4;
		}
		else
		{
		    end = strstr(start, "\n\n");
		    if(end)
		    {
			hlen = end - req->rbuf.buf + 2;
		    }
		    else
		    {
			http_error(req);
			return 0;
		    }
		}
		total_len = nlength + hlen;
		if(total_len > dwBytes && !iobuf_accommodate(&req->rbuf,total_len)) {
		    http_error(req);
		    return 0;
		}

		total = dwBytes;
		while(total < total_len)
		{
		    dwLeft = total_len -total;
		    wsaData.len = (u_long)dwLeft;
		    wsaData.buf = (char *)req->rbuf.buf + total;

		    rv = WSARecv(sd,&wsaData,1,&dwBytes,&flags,NULL,NULL);
		    if(rv == SOCKET_ERROR) {
			http_error(req);
			return 0;
		    }
		    total += dwBytes;
		}
	    }
	}
	req->rbuf.off += (size_t)total;
	req->parsing = 1;
	if(http_parse(req,(int)total)) {
	    req->parsing = 0;
	    return 1;
	} else
	    return 0;
    }
}


int
http_write(struct http_req *req, SOCKET sd)
{
    WSABUF wsaData;
    int rv;
    DWORD dwBytes = 0;
    assert(req->wbuf.off > 0);

    wsaData.len = (u_long)req->wbuf.off;
    wsaData.buf = (char *)req->wbuf.buf;

    rv = WSASend(sd, &wsaData, 1, &dwBytes, 0, NULL, NULL);
    if(rv == SOCKET_ERROR) {
	rv = WSAGetLastError();
	http_error(req);
	return 0;
    }
    else {
	iobuf_consumed(&req->wbuf,(size_t)dwBytes);
	return 1;
    }
}

static conv_ucs2_to_utf8(const wchar_t *in,
	size_t *inwords,
	char *out,
	size_t *outbytes)
{
    long newch,require;
    size_t need;
    char *invout;
    int ch;

    while(*inwords)
    {
	ch = (unsigned short)(*in++);
	if(ch < 0x80)
	{
	    --*inwords;
	    --*outbytes;
	    *(out++) = (unsigned char ) ch;
	}
	else
	{
	    if((ch & 0xFC00) == 0xDC00) {
		return -1;
	    }
	    if((ch & 0xFC00) == 0xD800) {
		if(*inwords < 2) {
		    return -1;
		}

		if(((unsigned short)(*in) & 0xFC00) != 0xDC00) {
		    return -1;
		}
		newch = (ch & 0x03FF) << 10 | ((unsigned short)(*in++) & 0x03FF);
		newch += 0x10000;
	    }
	    else {
		newch = ch;
	    }

	    require = newch >> 11;
	    need = 1;
	    while (require)
		require >>=5, ++need;
	    if(need >= *outbytes)
		break;
	    *inwords -= (need > 2) + 1;
	    *outbytes -= need + 1;

	    ch = 0200;
	    out += need + 1;
	    invout = out;
	    while(need--) {
		ch |= ch >> 1;
		*(--invout) = (unsigned char)(0200 | (newch & 0077));
		newch >>= 6;
	    }
	    *(--invout) = (unsigned char)(ch | newch);
	}
    }
    return 0;
}

int http_uri_encode(const wchar_t *uri,size_t inwords,char *enc_uri)
{
    char buf[64];
    unsigned char c;
    int i,ret;
    size_t outwords = 3*inwords + 1;
    char *utf8_uri = (char *)malloc(outwords);
    memset(utf8_uri,0,(outwords));
    memset(buf,0,sizeof(buf));

    ret = conv_ucs2_to_utf8(uri,&inwords,utf8_uri,&outwords);
    if(ret != 0)
	return -1;
    for(i = 0; i < strlen(utf8_uri); i++)
    {
	c = utf8_uri[i];
	if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c<= '9') || c== '.' || c == '-' ||
		c == '_' || c== '/')
	{
	    *enc_uri++ = c;
	    continue;
	}
	
	sprintf(buf,"%02x",c);
	*enc_uri++ = '%%';
	*enc_uri++ = buf[0];
	*enc_uri++ = buf[1];
    }

    free(utf8_uri);
    return 0;
}

int
http_get(struct http_req **out, const char *url, const char *hdrs,
    http_cb_t cb, void *arg)
{
    struct http_req *req = calloc(1, sizeof(*req));
    if (req == NULL)
        return 0;
    req->cb = cb;
    req->arg = arg;
    req->url = http_url_parse(url);
    if (req->url == NULL)
        goto error;
    req->rbuf = iobuf_init(4096);
    memset(req->rbuf.buf,0,req->rbuf.size);
    req->wbuf = iobuf_init(1024);
    memset(req->wbuf.buf,0,req->wbuf.size);
    if (!iobuf_print(&req->wbuf, "GET %s HTTP/1.1\r\n"
            "Host: %s:%hu\r\n"
            "Accept-Encoding:\r\n"
            "Connection: close\r\n"
            "%s"
            "\r\n", req->url->uri, req->url->host, req->url->port, hdrs))
        goto error;
    if (out != NULL)
        *out = req;
    return 1;
error:
    http_free(req);
    return 0;
}

void
http_cancel(struct http_req *req)
{
	if(!req)
		return;
    if (req->parsing)
        req->cancel = 1;
    else
        http_free(req);
}
