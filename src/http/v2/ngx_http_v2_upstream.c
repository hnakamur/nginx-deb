
/*
 * Copyright (C) Nginx, Inc.
 * Copyright (C) Google Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


ngx_int_t
ngx_http_v2_upstream_output_filter(void *data, ngx_chain_t *in)
{
    ngx_http_request_t  *r = data;

    ngx_buf_t                 *b;
    ngx_uint_t                 sid;
    ngx_http_v2_node_t        *node;
    ngx_http_upstream_t       *u;
    ngx_http_v2_stream_t      *stream;
    ngx_http_v2_out_frame_t   *frame;
    ngx_http_v2_connection_t  *h2c;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http2 upstream output filter");

    u = r->upstream;

    if (!u->stream->node) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http2 upstream output header");

        stream = u->stream;
        h2c = stream->connection;

        sid = h2c->last_sid ? h2c->last_sid + 2 : 1;

        node = ngx_http_v2_get_node_by_id(h2c, sid, 1);
        if (node == NULL) {
            return NGX_ERROR;
        }

        if (node->parent) {
            ngx_queue_remove(&node->reuse);
            h2c->closed_nodes--;
        }

        node->stream = stream;
        stream->node = node;

        h2c->last_sid = sid;

        b = in->buf;

        frame = ngx_http_v2_create_headers_frame(stream, b->pos, b->last,
                                                 b->last_buf);
        if (frame == NULL) {
            return NGX_ERROR;
        }

        /* HEADERS frame handler doesn't update original buffer */
        b->pos = b->last;

        ngx_http_v2_queue_blocked_frame(h2c, frame);
        stream->queued = 1;

        in = in->next;

        if (in == NULL) {
            return ngx_http_v2_filter_send(stream->fake_connection, stream);
        }
    }

    return ngx_chain_writer(&r->upstream->writer, in);
}


ngx_int_t
ngx_http_v2_stream_buffer_init(ngx_http_v2_stream_t *stream)
{
    off_t                 size;
    ngx_event_t          *rev;
    ngx_connection_t     *fc;
    ngx_http_upstream_t  *u;

    u = stream->request->upstream;
    fc = stream->fake_connection;
    rev = fc->read;

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, fc->log, 0,
                   "http2 stream buffer init s:%ui l:%O eof:%ud",
                   u->headers_in.status_n, u->headers_in.content_length_n,
                   stream->in_closed);

    if (stream->in_closed) {
        rev->eof = 1;
        rev->ready = 1;
        rev->active = 0;

        return NGX_OK;
    }

    if (u->headers_in.status_n == NGX_HTTP_NO_CONTENT
        || u->headers_in.status_n == NGX_HTTP_NOT_MODIFIED
        || u->headers_in.content_length_n == 0)
    {
        rev->ready = 0;
        rev->active = 1;

        return NGX_OK;
    }

    if (u->headers_in.content_length_n == -1
        || u->headers_in.content_length_n > NGX_HTTP_V2_PREREAD_WINDOW)
    {
        size = NGX_HTTP_V2_PREREAD_WINDOW;

    } else {
        size = u->headers_in.content_length_n;
    }

    fc->buffer = ngx_create_temp_buf(fc->pool, size);
    if (fc->buffer == NULL) {
        return NGX_ERROR;
    }

    rev->ready = 0;
    rev->active = 1;

    return NGX_OK;
}


ngx_int_t
ngx_http_v2_stream_buffer_save(ngx_http_v2_stream_t *stream, u_char *pos,
    size_t size)
{
    size_t             free_chunk, free_total;
    ngx_buf_t         *b;
    ngx_event_t       *rev;
    ngx_connection_t  *fc;

    fc = stream->fake_connection;
    rev = fc->read;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, fc->log, 0,
                   "http2 stream buffer save s:%uz eof:%ud",
                   size, stream->in_closed);

    if (stream->in_closed) {
        if (rev->available || size) {
            rev->pending_eof = 1;

        } else {
            rev->pending_eof = 0;
            rev->eof = 1;
            rev->ready = 1;
            rev->active = 0;
        }

        if (size == 0) {
            return NGX_OK;
        }

    } else if (rev->pending_eof || rev->eof || size == 0) {
        rev->ready = 0;
        rev->active = 1;
        return NGX_ERROR;
    }

    b = fc->buffer;

    if (b == NULL || b->start == NULL) {
        rev->ready = 0;
        rev->active = 1;
        return NGX_ERROR;
    }

    if (b->last == b->end) {
        b->last = b->start;
    }

    if (b->pos <= b->last) {
        free_chunk = b->end - b->last;
        free_total = free_chunk + (b->pos - b->start);

    } else {
        free_chunk = b->pos - b->last;
        free_total = free_chunk;
    }

    if (size > free_total) {
        rev->ready = 0;
        rev->active = 1;
        return NGX_ERROR;
    }

    if (free_chunk > size) {
        free_chunk = size;
    }

    b->last = ngx_cpymem(b->last, pos, free_chunk);
    pos += free_chunk;
    size -= free_chunk;

    if (size) {
        b->last = ngx_cpymem(b->start, pos, size);
        pos += size;
    }

    rev->ready = 1;
    rev->active = 0;

#if (NGX_HAVE_KQUEUE)
    rev->available += free_chunk + size;
#else
    rev->available = 1;
#endif

    return NGX_OK;
}


ssize_t
ngx_http_v2_recv(ngx_connection_t *fc, u_char *buf, size_t size)
{
    size_t                     saved_chunk, saved_total;
    ssize_t                    bytes;
    ngx_buf_t                 *b;
    ngx_event_t               *rev;
    ngx_http_request_t        *r;
    ngx_http_v2_stream_t      *stream;
    ngx_http_v2_connection_t  *h2c;

    rev = fc->read;

    if (fc->error) {
        rev->ready = 0;
        rev->active = 1;
        return NGX_ERROR;
    }

    if (rev->eof) {
        rev->ready = 0;
        rev->active = 1;
        return 0;
    }

    b = fc->buffer;

    if (b == NULL || b->start == NULL) {
        rev->ready = 0;
        rev->active = 1;
        return NGX_ERROR;
    }

    if (!rev->available) {
        return NGX_AGAIN;
    }

    if (b->pos == b->end) {
        b->pos = b->start;
    }

    if (b->pos < b->last) {
        saved_chunk = b->last - b->pos;
        saved_total = saved_chunk;

    } else {
        saved_chunk = b->end - b->pos;
        saved_total = saved_chunk + (b->last - b->start);
    }

    if (size > saved_total) {
        size = saved_total;
    }

    if (saved_chunk > size) {
        saved_chunk = size;
    }

    buf = ngx_cpymem(buf, b->pos, saved_chunk);
    b->pos += saved_chunk;
    size -= saved_chunk;

    if (size) {
        buf = ngx_cpymem(buf, b->start, size);
        b->pos = b->start + size;
    }

    bytes = saved_chunk + size;

#if (NGX_HAVE_KQUEUE)
    rev->available -= bytes;
#endif

    if (b->last == b->pos) {

        if (rev->pending_eof) {
            rev->pending_eof = 0;
            rev->eof = 1;

            ngx_pfree(fc->pool, b->start);
            b->start = NULL;

        } else {
            rev->ready = 0;
            rev->active = 1;

            b->pos = b->start;
            b->last = b->start;
        }

#if !(NGX_HAVE_KQUEUE)
        rev->available = 0;
#endif
    }

    if (rev->pending_eof || rev->eof) {
        return bytes;
    }

    r = fc->data;
    stream = r->upstream->stream;
    h2c = stream->connection;

    if (ngx_http_v2_send_window_update(h2c, stream->node->id, bytes)
        == NGX_ERROR)
    {
        ngx_http_v2_finalize_connection(h2c, NGX_HTTP_V2_INTERNAL_ERROR);
        return NGX_ERROR;
    }

    stream->recv_window += bytes;

    if (!h2c->blocked) {
        if (ngx_http_v2_send_output_queue(h2c) == NGX_ERROR) {
            ngx_http_v2_finalize_connection(h2c, NGX_HTTP_V2_INTERNAL_ERROR);
            return NGX_ERROR;
        }
    }

    return bytes;
}


ssize_t
ngx_http_v2_recv_chain(ngx_connection_t *fc, ngx_chain_t *cl, off_t limit)
{
    u_char     *last;
    ssize_t     n, bytes, size;
    ngx_buf_t  *b;

    bytes = 0;

    b = cl->buf;
    last = b->last;

    for ( ;; ) {
        size = b->end - last;

        if (limit) {
            if (bytes >= limit) {
                return bytes;
            }

            if (bytes + size > limit) {
                size = (ssize_t) (limit - bytes);
            }
        }

        n = ngx_http_v2_recv(fc, last, size);

        if (n > 0) {
            last += n;
            bytes += n;

            if (last == b->end) {
                cl = cl->next;

                if (cl == NULL) {
                    return bytes;
                }

                b = cl->buf;
                last = b->last;
            }

            continue;
        }

        if (bytes) {

            if (n == 0 || n == NGX_ERROR) {
                fc->read->ready = 1;
                fc->read->active = 0;
            }

            return bytes;
        }

        return n;
    }
}


void
ngx_http_v2_upstream_free_stream(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ngx_http_v2_stream_t      *stream;
    ngx_http_v2_connection_t  *h2c;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http2 upstream free stream");

    stream = u->stream;
    u->stream = NULL;

    h2c = stream->connection;

    if (stream->queued) {
        ngx_http_v2_filter_cleanup(stream);

        if (stream->queued && !h2c->blocked) {
            if (ngx_http_v2_send_output_queue(h2c) == NGX_ERROR) {
                ngx_http_v2_finalize_connection(h2c, 0);
                goto error;
            }
        }
    }

    ngx_http_v2_close_stream(stream, 0);

    if (h2c->connection->error) {
        goto error;
    }

    if (stream->queued && !h2c->goaway) {
        h2c->goaway = 1;

        if (ngx_http_v2_send_goaway(h2c, NGX_HTTP_V2_NO_ERROR) == NGX_ERROR) {
            ngx_http_v2_finalize_connection(h2c, NGX_HTTP_V2_INTERNAL_ERROR);
            goto error;
        }
    }

    if (h2c->last_out && !h2c->blocked) {
        if (ngx_http_v2_send_output_queue(h2c) == NGX_ERROR) {
            ngx_http_v2_finalize_connection(h2c, 0);
            goto error;
        }
    }

    u->peer.connection = h2c->connection;
    u->keepalive = !h2c->goaway;

    return;

error:

    u->peer.connection = NULL;
    u->peer.sockaddr = NULL;
}
