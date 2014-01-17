/*
 * Sphinx2 types to work with search daemon
 */

#ifndef NGX_HTTP_SUMMARIZER_PROTO_H
#define NGX_HTTP_SUMMARIZER_PROTO_H


/* TYPES */

#define SMRZR_VERSION          1
#define SMRZR_DAEMON_PROTO     0x1421

/* Return codes from summarizer daemon */
typedef enum {
    SMRZR_STATUS_SUMMARY =      0,
    SMRZR_STATUS_INVALID_REQ =  1,
    SMRZR_STATUS_INTERNAL_ERR = 2,
} smrzr_status_t;

/* Request header */
typedef struct {
    uint16_t           proto;
    uint16_t           ver;
    uint32_t           ratio;
    uint32_t           filename_len;
} smrzr_request_header_t;

/* Response header */
typedef struct {
    uint16_t           proto;
    uint16_t           ver;
    uint32_t           status;
} smrzr_response_header_t;

/* Summary response header */
typedef struct {
    uint16_t           proto;
    uint16_t           ver;
    uint32_t           status;
    uint32_t           summary_len;
} smrzr_summary_header_t;

/* Search input from URL */
typedef struct {
    ngx_str_t        * file_name;
    float              ratio;
} smrzr_input_t;


/* FUNCTION PROTOTYPES */

ngx_int_t  
smrzr_create_summary_request(ngx_pool_t*, smrzr_input_t*, ngx_buf_t**);

ngx_int_t
smrzr_parse_summary_response_header(ngx_pool_t*, ngx_buf_t*, smrzr_status_t*,
                                    uint32_t*);

/* GLOBALS */

extern float   smrzr_default_ratio;
extern size_t  smrzr_min_header_len;

#endif /* NGX_HTTP_SUMMARIZER_PROTO_H */
