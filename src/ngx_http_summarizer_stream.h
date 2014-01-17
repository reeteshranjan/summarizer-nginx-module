/*
 * Stream abstraction for ngx_buf_t
 */

#ifndef NGX_HTTP_SUMMARIZER_STREAM_H
#define NGX_HTTP_SUMMARIZER_STREAM_H

/* TYPES */

typedef struct smrzr_stream_s smrzr_stream_t;

/* PROTOTYPES */

/* create stream */
smrzr_stream_t*
smrzr_stream_create(ngx_pool_t*);

/* allocate buffer (for writes) */
ngx_int_t
smrzr_stream_alloc(smrzr_stream_t * strm, size_t len);

/* set given buffer (for reads) */
ngx_int_t
smrzr_stream_set_buf(smrzr_stream_t * strm, ngx_buf_t * b);

/* get the buffer */
ngx_buf_t*
smrzr_stream_get_buf(smrzr_stream_t * strm);

/* get current offset in stream */
ngx_uint_t
smrzr_stream_offset(smrzr_stream_t * strm);

/* get max size of stream */
ngx_uint_t
smrzr_stream_maxsize(smrzr_stream_t * strm);

/* writes */
ngx_int_t
smrzr_stream_write_int16(smrzr_stream_t * strm, uint16_t val);

ngx_int_t
smrzr_stream_write_int32(smrzr_stream_t * strm, uint32_t val);

ngx_int_t
smrzr_stream_write_float(smrzr_stream_t * strm, float val);

ngx_int_t
smrzr_stream_write_string(smrzr_stream_t * strm, ngx_str_t * val);

/* reads */
ngx_int_t
smrzr_stream_read_int16(smrzr_stream_t * strm, uint16_t * val);

ngx_int_t
smrzr_stream_read_int32(smrzr_stream_t * strm, uint32_t * val);

ngx_int_t
smrzr_stream_read_float(smrzr_stream_t * strm, float * val);

ngx_int_t
smrzr_stream_read_string(smrzr_stream_t * strm, ngx_str_t ** val);

#endif /* NGX_HTTP_SUMMARIZER_STREAM_H */
