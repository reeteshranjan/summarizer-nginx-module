/*
 * Sphinx2 protocol functionality
 */

#include <assert.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_http_summarizer_proto.h"
#include "ngx_http_summarizer_stream.h"

/* GLOBALS */

float   smrzr_default_ratio = 30.0;

size_t  smrzr_min_header_len = 12; /* hs (4) + hdr(8) */

/* LOCAL GLOBALS */

static const size_t sz16 = sizeof(uint16_t),
                    sz32 = sizeof(uint32_t),
                    szf = sizeof(float);

/* FUNCTION DEFINITIONS */

#include <ctype.h>

static void s_dump_buffer(u_char* ptr, size_t len) {
    size_t i; char c;
    fprintf(stderr, "buf [%lu]: ", len);
    for(i = 0; i < len; ++i) {
        c = ptr[i];
        if(isprint(c)) fprintf(stderr, "%c", c);
        else if(c < 0x10) fprintf(stderr, "0%x", c);
        else fprintf(stderr, "%x", c);
    }
    fprintf(stderr, "\n");
}

/* Functions to handle summary request */

static size_t
s_smrzr_summary_request_len(smrzr_input_t * input)
{
    static const size_t header_len = sizeof(smrzr_request_header_t);

    return(header_len + input->file_name->len + 1);
}

ngx_int_t
smrzr_create_summary_request(
    ngx_pool_t      * pool,
    smrzr_input_t   * input,
    ngx_buf_t      ** b)
{
    /* data to send =
     *   header = proto [2] . version [2] . ratio [4]
     *            . filename_len  [4]
     * . filename [filename_len]
     */

    size_t buf_len = s_smrzr_summary_request_len(input);

    smrzr_stream_t* st = smrzr_stream_create(pool);

    ngx_int_t status;

    if(NGX_ERROR == smrzr_stream_alloc(st, buf_len)) {
        return(NGX_ERROR);
    }

    status =
           /* header - proto */
           smrzr_stream_write_int16(st, (uint16_t)SMRZR_DAEMON_PROTO)
           /* header - ver */
        || smrzr_stream_write_int16(st, (uint16_t)SMRZR_VERSION)
           /* ratio */
        || smrzr_stream_write_float(st, (float)input->ratio)
           /* file name */
        || smrzr_stream_write_string(st, input->file_name)
        ;

    *b = smrzr_stream_get_buf(st);

    s_dump_buffer((*b)->pos, buf_len);

    return status;
}

/* Functions to work with summarizerd response */

static ngx_int_t
s_smrzr_parse_response_header(
    ngx_pool_t      * pool,
    ngx_buf_t       * b,
    smrzr_status_t  * out_status,
    uint32_t        * len)
{
    ngx_int_t                status;
    smrzr_stream_t         * st;
    smrzr_response_header_t  rep_hdr;

    if(NULL == (st = smrzr_stream_create(pool))) {
        return(NGX_ERROR);
    }

    if(NGX_ERROR == smrzr_stream_set_buf(st, b)) {
        return(NGX_ERROR);
    }

    if(NGX_ERROR ==
        (status =
               smrzr_stream_read_int16(st, &rep_hdr.proto)
            || smrzr_stream_read_int16(st, &rep_hdr.ver)
            || smrzr_stream_read_int32(st, &rep_hdr.status)))
    {
        return(NGX_HTTP_UPSTREAM_INVALID_HEADER);
    }

    if(SMRZR_DAEMON_PROTO != rep_hdr.proto || SMRZR_VERSION != rep_hdr.ver) {
        return(NGX_HTTP_UPSTREAM_INVALID_HEADER);
    }

    switch(rep_hdr.status) {
        case SMRZR_STATUS_SUMMARY:
            if(NGX_ERROR == (status = smrzr_stream_read_int32(st, len)))
                return(NGX_HTTP_UPSTREAM_INVALID_HEADER);
        case SMRZR_STATUS_INVALID_REQ:
        case SMRZR_STATUS_INTERNAL_ERR:
            /* we are good here */
            *out_status = rep_hdr.status;
            break;
        default:
            return(NGX_HTTP_UPSTREAM_INVALID_HEADER);
    }

    return(NGX_OK);
}

ngx_int_t
smrzr_parse_summary_response_header(
    ngx_pool_t       * pool,
    ngx_buf_t        * b,
    smrzr_status_t   * status,
    uint32_t         * len)
{
    return(s_smrzr_parse_response_header(pool, b, status, len));
}
