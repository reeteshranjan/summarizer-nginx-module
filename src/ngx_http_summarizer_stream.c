/*
 * Sphinx2 request/response buffer stream using ngx_buf_t
 */

#include <assert.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_http_summarizer_stream.h"

/* TYPES */

struct smrzr_stream_s {
    ngx_pool_t           * pool;
    ngx_buf_t            * b;
};

/* FUNCTION DEFINITIONS */

/* create stream */
smrzr_stream_t*
smrzr_stream_create(ngx_pool_t * pool)
{
    smrzr_stream_t *s;

    if(NULL == (s = ngx_pcalloc(pool, sizeof(smrzr_stream_t))))
        return(NULL);

    s->pool = pool;

    return(s);
}

/* allocate buffer (for writes) */
ngx_int_t
smrzr_stream_alloc(
    smrzr_stream_t * strm,
    size_t           len)
{
    if(NULL == (strm->b = ngx_create_temp_buf(strm->pool, len))) {
        return(NGX_ERROR);
    }

    return(NGX_OK);
}

/* set given buffer (for reads) */
ngx_int_t
smrzr_stream_set_buf(
    smrzr_stream_t * strm,
    ngx_buf_t      * b)
{
    assert(NULL != b);
    strm->b = b;
    return (NGX_OK);
}

/* get the buffer */
ngx_buf_t*
smrzr_stream_get_buf(smrzr_stream_t * strm)
{
    return(strm->b);
}

/* get current offset in stream */
ngx_uint_t
smrzr_stream_offset(smrzr_stream_t * strm)
{
    assert(NULL != strm->b);
    return(strm->b->last - strm->b->pos);
}

/* get max size of stream */
ngx_uint_t
smrzr_stream_maxsize(smrzr_stream_t * strm)
{
    assert(NULL != strm->b);
    return(strm->b->end - strm->b->start);
}

/* writes */

#define CHECK_AND_APPEND(strm, type, val)     \
do { \
    if(sizeof(type) > (size_t)(strm->b->end - strm->b->last)) { \
        return (NGX_ERROR); \
    } \
    memcpy(strm->b->last, &val, sizeof(type)); \
    strm->b->last += sizeof(type); \
} while(0)

#define CHECK_AND_APPEND_STR(strm, val)     \
do { \
    uint32_t conv = htonl((uint32_t)val->len); \
    CHECK_AND_APPEND(strm, uint32_t, conv); \
    if(val->len > (size_t)(strm->b->end - strm->b->last)) { \
        return (NGX_ERROR); \
    } \
    memcpy(strm->b->last, val->data, val->len); \
    strm->b->last += val->len; \
} while(0)

ngx_int_t
smrzr_stream_write_int16(
    smrzr_stream_t * strm,
    uint16_t         val)
{
    assert(NULL != strm->b && NULL != strm->b->last);

    val = htons(val);

    CHECK_AND_APPEND(strm, uint16_t, val);

    return(NGX_OK);
}

ngx_int_t
smrzr_stream_write_int32(
    smrzr_stream_t * strm,
    uint32_t         val)
{
    assert(NULL != strm->b && NULL != strm->b->last);

    val = htonl(val);

    CHECK_AND_APPEND(strm, uint32_t, val);

    return(NGX_OK);
}

ngx_int_t
smrzr_stream_write_float(
    smrzr_stream_t * strm,
    float            val)
{
    uint32_t conv;

    assert(NULL != strm->b && NULL != strm->b->last);

    /* treat float as 32-bit dword */
    conv = htonl(*((uint32_t*)&val));

    CHECK_AND_APPEND(strm, uint32_t, conv);

    return(NGX_OK);
}

ngx_int_t
smrzr_stream_write_string(
    smrzr_stream_t * strm,
    ngx_str_t      * val)
{
    assert(NULL != strm->b && NULL != strm->b->last);

    CHECK_AND_APPEND_STR(strm, val); /* len + str (no null char) */

    return(NGX_OK);
}

/* reads */

#define CHECK_AND_READ(strm, type, val)     \
do { \
    if(sizeof(type) > (size_t)(strm->b->last - strm->b->pos)) { \
        return (NGX_ERROR); \
    } \
    memcpy(val, strm->b->pos, sizeof(type)); \
    strm->b->pos += sizeof(type); \
} while(0)

#define CHECK_AND_READ_STR(strm, val)     \
do { \
    CHECK_AND_READ(strm, uint32_t, &((val)->len)); \
    (val)->len = ntohl((val)->len); \
    if((val)->len > (size_t)(strm->b->last - strm->b->pos)) { \
        return (NGX_ERROR); \
    } \
    if(NULL == ((val)->data = ngx_palloc(strm->pool, (val)->len+1))) { \
        return (NGX_ERROR); \
    } \
    memcpy((val)->data, strm->b->pos, (val)->len); \
    (val)->data[(val)->len] = 0; \
    strm->b->pos += (val)->len; \
} while(0)

ngx_int_t
smrzr_stream_read_int16(
    smrzr_stream_t * strm,
    uint16_t       * val)
{
    assert(NULL != strm->b && NULL != strm->b->pos);

    CHECK_AND_READ(strm, uint16_t, val);

    *val = ntohs(*val);

    return(NGX_OK);
}

ngx_int_t
smrzr_stream_read_int32(
    smrzr_stream_t * strm,
    uint32_t       * val)
{
    assert(NULL != strm->b && NULL != strm->b->pos);

    CHECK_AND_READ(strm, uint32_t, val);

    *val = ntohl(*val);

    return(NGX_OK);
}

ngx_int_t
smrzr_stream_read_float(
    smrzr_stream_t * strm,
    float          * val)
{
    uint32_t v;

    assert(NULL != strm->b && NULL != strm->b->pos);

    CHECK_AND_READ(strm, uint32_t, &v);

    v = ntohl(v);

    memcpy(val, &v, sizeof(uint32_t));

    return(NGX_OK);
}

ngx_int_t
smrzr_stream_read_string(
    smrzr_stream_t * strm,
    ngx_str_t     ** val)
{
    assert(NULL != strm->b && NULL != strm->b->pos);

    if(NULL == (*val = ngx_palloc(strm->pool, sizeof(ngx_str_t)))) {
        return(NGX_ERROR);
    }

    CHECK_AND_READ_STR(strm, *val);

    return(NGX_OK);
}
