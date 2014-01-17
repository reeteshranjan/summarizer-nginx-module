/*
 * Summarizer upstream module
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_http_summarizer_proto.h"

/* TYPES */

typedef enum {
    SMRZR_ARG_FILENAME = 0,
    SMRZR_ARG_RATIO,
    SMRZR_ARG_COUNT
} smrzr_args_t;

typedef struct {
    ngx_http_upstream_conf_t       upstream;
    ngx_int_t                      arg_idx[SMRZR_ARG_COUNT];
} ngx_http_summarizer_loc_conf_t;

typedef struct {
    ngx_http_request_t           * request;
    smrzr_status_t                 status;
    size_t                         len;
} ngx_http_summarizer_ctx_t;


/* PROTOTYPES */

static void      * ngx_http_summarizer_create_loc_conf(ngx_conf_t *cf);
static char      * ngx_http_summarizer_merge_loc_conf(ngx_conf_t *cf, void 
                       *parent, void *child);

static ngx_int_t   ngx_http_summarizer_handler(ngx_http_request_t *r);
static ngx_int_t   ngx_http_summarizer_create_request(ngx_http_request_t *r);
static ngx_int_t   ngx_http_summarizer_reinit_request(ngx_http_request_t *r);
static ngx_int_t   ngx_http_summarizer_process_header(ngx_http_request_t *r);
#if 0
static ngx_int_t   ngx_http_summarizer_filter_init(void *data);
static ngx_int_t   ngx_http_summarizer_filter(void *data, ssize_t bytes);
#endif
static void        ngx_http_summarizer_abort_request(ngx_http_request_t *r);
static void        ngx_http_summarizer_finalize_request(ngx_http_request_t *r, 
ngx_int_t rc);

static char      * ngx_http_summarizer_pass(ngx_conf_t *cf, ngx_command_t *cmd, 
                       void *conf);

/* LOCALS */

static ngx_str_t ngx_http_summarizer_args[] = {
    ngx_string("smrzr_filename"),    /* SMRZR_ARG_FILENAME */
    ngx_string("smrzr_ratio")        /* SMRZR_ARG_RATIO */
};

/* MODULE GLOBALS */

static ngx_conf_bitmask_t ngx_http_summarizer_next_upstream_masks[] = {
    { ngx_string("error"),            NGX_HTTP_UPSTREAM_FT_ERROR },
    { ngx_string("timeout"),          NGX_HTTP_UPSTREAM_FT_TIMEOUT },
    { ngx_string("invalid_response"), NGX_HTTP_UPSTREAM_FT_INVALID_HEADER },
    { ngx_string("not_found"),        NGX_HTTP_UPSTREAM_FT_HTTP_404 },
    { ngx_string("off"),              NGX_HTTP_UPSTREAM_FT_OFF },
    { ngx_null_string, 0 }
};


static ngx_command_t ngx_http_summarizer_commands[] = {

    /* module specific commands */
    { ngx_string("summarizer_pass"),
      NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
      ngx_http_summarizer_pass,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /* standard ones for upstream module */
    { ngx_string("summarizer_bind"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_upstream_bind_set_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_summarizer_loc_conf_t, upstream.local),
      NULL },

    { ngx_string("summarizer_connect_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_summarizer_loc_conf_t, upstream.connect_timeout),
      NULL },

    { ngx_string("summarizer_send_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_summarizer_loc_conf_t, upstream.send_timeout),
      NULL },

    { ngx_string("summarizer_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_summarizer_loc_conf_t, upstream.buffer_size),
      NULL },

    { ngx_string("summarizer_read_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_summarizer_loc_conf_t, upstream.read_timeout),
      NULL },

    { ngx_string("summarizer_next_upstream"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_summarizer_loc_conf_t, upstream.upstream),
      &ngx_http_summarizer_next_upstream_masks },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_summarizer_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_summarizer_create_loc_conf,      /* create location configration */
    ngx_http_summarizer_merge_loc_conf        /* merge location configration */
};


ngx_module_t ngx_http_summarizer_module = {
    NGX_MODULE_V1,
    &ngx_http_summarizer_module_ctx,          /* module context */
    ngx_http_summarizer_commands,             /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


/* FUNCTION DEFINITIONS */

/* location conf creation */
static void*
ngx_http_summarizer_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_summarizer_loc_conf_t  *conf;
    size_t i;

    if(NULL == (conf = ngx_pcalloc(cf->pool, 
                           sizeof(ngx_http_summarizer_loc_conf_t))))
    {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->upstream.bufs.num = 0;
     *     conf->upstream.upstream = 0;
     *     conf->upstream.temp_path = NULL;
     *     conf->upstream.uri = { 0, NULL };
     *     conf->upstream.location = NULL;
     */

    conf->upstream.connect_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.send_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.read_timeout = NGX_CONF_UNSET_MSEC;

    conf->upstream.buffer_size = NGX_CONF_UNSET_SIZE;

    /* the hardcoded values */
    conf->upstream.cyclic_temp_file = 0;
    conf->upstream.buffering = 0;
    conf->upstream.ignore_client_abort = 0;
    conf->upstream.send_lowat = 0;
    conf->upstream.bufs.num = 0;
    conf->upstream.busy_buffers_size = 0;
    conf->upstream.max_temp_file_size = 0;
    conf->upstream.temp_file_write_size = 0;
    conf->upstream.intercept_errors = 1;
    conf->upstream.intercept_404 = 1;
    conf->upstream.pass_request_headers = 0;
    conf->upstream.pass_request_body = 0;

    /* initialize module specific elements of the context */
    for(i = 0; i < SMRZR_ARG_COUNT; ++i) {
        conf->arg_idx[i] = NGX_CONF_UNSET;
    }

    return conf;
}

/* location conf merge */
static char*
ngx_http_summarizer_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_summarizer_loc_conf_t *prev = parent;
    ngx_http_summarizer_loc_conf_t *conf = child;
    size_t i;

    ngx_conf_merge_msec_value(conf->upstream.connect_timeout, 
                              prev->upstream.connect_timeout, 60000);
    ngx_conf_merge_msec_value(conf->upstream.send_timeout, 
                              prev->upstream.send_timeout, 60000);
    ngx_conf_merge_msec_value(conf->upstream.read_timeout, 
                              prev->upstream.read_timeout, 60000);

    ngx_conf_merge_size_value(conf->upstream.buffer_size, 
                              prev->upstream.buffer_size, (size_t)ngx_pagesize);

    ngx_conf_merge_bitmask_value(conf->upstream.next_upstream,
                                 prev->upstream.next_upstream,
                                 (NGX_CONF_BITMASK_SET | 
                                      NGX_HTTP_UPSTREAM_FT_ERROR |
                                      NGX_HTTP_UPSTREAM_FT_TIMEOUT));

    if (conf->upstream.next_upstream & NGX_HTTP_UPSTREAM_FT_OFF) {
        conf->upstream.next_upstream =
            NGX_CONF_BITMASK_SET | NGX_HTTP_UPSTREAM_FT_OFF;
    }

    if (conf->upstream.upstream == NULL) {
        conf->upstream.upstream = prev->upstream.upstream;
    }

    for(i = 0; i < SMRZR_ARG_COUNT; ++i) {
        if(conf->arg_idx[i] == NGX_CONF_UNSET) {
            conf->arg_idx[i] = prev->arg_idx[i];
        }
    }

    return NGX_CONF_OK;
}

/* pass */
static char*
ngx_http_summarizer_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_summarizer_loc_conf_t *slcf = conf;
    ngx_str_t                      *value;
    ngx_url_t                       url;
    ngx_http_core_loc_conf_t       *clcf;
    size_t                          i;

    if (slcf->upstream.upstream) {
        return "is duplicate";
    }

    value = cf->args->elts;

    ngx_memzero(&url, sizeof(ngx_url_t));

    url.url = value[1];
    url.no_resolve = 1;

    slcf->upstream.upstream = ngx_http_upstream_add(cf, &url, 0);
    if (slcf->upstream.upstream == NULL) {
        return NGX_CONF_ERROR;
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    clcf->handler = ngx_http_summarizer_handler;

    if (clcf->name.data[clcf->name.len - 1] == '/') {
        clcf->auto_redirect = 1;
    }

    for(i = 0; i < SMRZR_ARG_COUNT; ++i) {
        if(NGX_ERROR == (slcf->arg_idx[i] = ngx_http_get_variable_index(
                                      cf, &ngx_http_summarizer_args[i])))
        {
            ngx_log_error(NGX_LOG_ERR, cf->log, 0,
                 "Can't get variable index for '%s' key",
                    ngx_http_summarizer_args[i].data);
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}

/* upstream handler to provide the callbacks */
ngx_int_t
ngx_http_summarizer_handler(ngx_http_request_t *r)
{
    ngx_int_t                           rc;
    ngx_http_upstream_t                *u;
    ngx_http_summarizer_ctx_t          *ctx;
    ngx_http_summarizer_loc_conf_t     *slcf;

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "summarizer_handler: http method is not GET or HEAD");
        return NGX_HTTP_NOT_ALLOWED;
    }

    if (ngx_http_set_content_type(r) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "summarizer_handler: failed in set_content_type");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_http_upstream_create(r) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "summarizer_handler: failed to create upstream");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    u = r->upstream;

    ngx_str_set(&u->schema, "summarizer://"); 
    /*ngx_str_set(&u->schema, "");*/
    u->output.tag = (ngx_buf_tag_t) &ngx_http_summarizer_module;

    slcf = ngx_http_get_module_loc_conf(r, ngx_http_summarizer_module);

    u->conf = &slcf->upstream;

    u->create_request = ngx_http_summarizer_create_request;
    u->reinit_request = ngx_http_summarizer_reinit_request;
    u->process_header = ngx_http_summarizer_process_header;
    u->abort_request = ngx_http_summarizer_abort_request;
    u->finalize_request = ngx_http_summarizer_finalize_request;

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_summarizer_ctx_t));
    if (ctx == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    /* TODO upstream context's sphinx specific members init here */

    ngx_http_set_ctx(r, ctx, ngx_http_summarizer_module);

    /*u->input_filter_init = ngx_http_summarizer_filter_init;
    u->input_filter = ngx_http_summarizer_filter;
    u->input_filter_ctx = ctx;*/

    rc = ngx_http_read_client_request_body(r, ngx_http_upstream_init);

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    return NGX_DONE;
}

/* parse search arguments */

#define GET_INDEXED_VARIABLE_VAL(r, slcf, arg_no) \
    ngx_int_t i = slcf->arg_idx[arg_no]; \
    ngx_http_variable_value_t * vv = ngx_http_get_indexed_variable(r, i); \
    if (vv == NULL || vv->not_found) { \
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, \
            "'%s' variable is not set", ngx_http_summarizer_args[arg_no].data); \
        return NGX_ERROR; \
    } \
    ngx_str_t* vvs = ngx_palloc(r->pool, sizeof(ngx_str_t)); \
    if(NULL == vvs) { return NGX_ERROR; } \
    if(vv->len != 0) { \
        if(NULL == (vvs->data = ngx_palloc(r->pool, vv->len))) { \
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, \
                "Failed to allocate while getting indexed variable"); \
            return NGX_ERROR; \
        } \
        memcpy(vvs->data, vv->data, vv->len); \
    } else { vvs->data = NULL; } \
    vvs->len = vv->len;

#define GET_ARG(arg_no, var) \
do { \
    GET_INDEXED_VARIABLE_VAL(r, slcf, arg_no); \
    input->var = vvs; \
} while(0)

#define PARSE_FLOAT_ARG(arg_no, var, dflt)   \
do { \
    GET_INDEXED_VARIABLE_VAL(r, slcf, arg_no); \
    if(vvs->len != 0) { \
        input->var = atoi((const char*)(vvs->data)); \
    } else { input->var = dflt; } \
} while(0)

static ngx_int_t
ngx_http_summarizer_parse_args(
    ngx_http_request_t                  * r,
    ngx_http_summarizer_loc_conf_t      * slcf,
    smrzr_input_t                       * input)
{
    /* file name */
    GET_ARG(SMRZR_ARG_FILENAME, file_name);

    /* ratio */
    PARSE_FLOAT_ARG(SMRZR_ARG_RATIO, ratio, smrzr_default_ratio);

    return(NGX_OK);
}

/* create request callback */
static ngx_int_t
ngx_http_summarizer_create_request(ngx_http_request_t *r)
{
    ngx_buf_t                      * b;
    ngx_chain_t                    * cl;
    ngx_http_summarizer_loc_conf_t * slcf;
    ngx_http_summarizer_ctx_t      * ctx;
    smrzr_input_t                    input;
    ngx_str_t                        dbg;
 
    slcf = ngx_http_get_module_loc_conf(r, ngx_http_summarizer_module);

    ctx = ngx_http_get_module_ctx(r, ngx_http_summarizer_module);

    memset(&input, 0, sizeof(input));
    if(NGX_OK != ngx_http_summarizer_parse_args( r, slcf, &input)) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "Summarizer query args parse error");
        return(NGX_ERROR);
    }

    if(NGX_ERROR == smrzr_create_summary_request(r->pool, &input, &b))
    {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "Summarizer upstream search req creation failed");
        return(NGX_ERROR);
    }

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    cl->buf = b;
    cl->next = NULL;

    r->upstream->request_bufs = cl;

    ctx->request = r;

    dbg.data = b->pos;
    dbg.len = b->last - b->pos;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
        "summarizer request: \"%V\"", &dbg);

    return NGX_OK;
}


static ngx_int_t
ngx_http_summarizer_reinit_request(ngx_http_request_t *r)
{
    return NGX_OK;
}


static ngx_int_t
ngx_http_summarizer_process_header(ngx_http_request_t *r)
{
    ngx_http_upstream_t        * u;
    ngx_http_summarizer_ctx_t  * ctx;
    ngx_buf_t                  * b;
    ngx_int_t                    status;
    uint32_t                     len = 0;

    u = r->upstream;
    b = &u->buffer;

    /* not enough bytes read to parse status */
    if((b->last - b->pos) < (ssize_t)smrzr_min_header_len) {
        return(NGX_AGAIN);
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_summarizer_module);

    if(NGX_OK != (status = smrzr_parse_summary_response_header(r->pool, b,
                           &ctx->status, &len)))
    {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "Summarizer upstream error processing response header");
        return status;
    }

    switch(ctx->status) {
    case SMRZR_STATUS_SUMMARY:
        u->headers_in.content_length_n = len;
        u->headers_in.status_n = NGX_HTTP_OK;
        u->state->status = NGX_HTTP_OK;
        break;
    case SMRZR_STATUS_INVALID_REQ:
        u->headers_in.status_n = NGX_HTTP_BAD_REQUEST;
        u->state->status = NGX_HTTP_BAD_REQUEST;
        break;
    case SMRZR_STATUS_INTERNAL_ERR:
    default:
        u->headers_in.status_n = NGX_HTTP_INTERNAL_SERVER_ERROR;
        u->state->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        break;
    }

    return NGX_OK;
}

#if 0
static ngx_int_t
ngx_http_summarizer_filter_init(void *data)
{
    return NGX_OK;
}


static ngx_int_t
ngx_http_summarizer_filter(void *data, ssize_t bytes)
{
    /*ngx_http_summarizer_ctx_t  *ctx = data;*/

    /* TODO filter impl here */
    return 0; /*ctx->filter(ctx, bytes);*/
}
#endif

static void
ngx_http_summarizer_abort_request(ngx_http_request_t *r)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "abort http summarizer request");
    return;
}


static void
ngx_http_summarizer_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "finalize http summarizer request");
    return;
}
