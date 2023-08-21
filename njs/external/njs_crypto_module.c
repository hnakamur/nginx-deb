
/*
 * Copyright (C) Dmitry Volyntsev
 * Copyright (C) NGINX, Inc.
 */


#include <njs.h>
#include <njs_md5.h>
#include <njs_sha1.h>
#include <njs_sha2.h>
#include <njs_string.h>
#include <njs_buffer.h>


typedef void (*njs_hash_init)(void *ctx);
typedef void (*njs_hash_update)(void *ctx, const void *data, size_t size);
typedef void (*njs_hash_final)(u_char *result, void *ctx);

typedef njs_int_t (*njs_digest_encode)(njs_vm_t *vm, njs_value_t *value,
    const njs_str_t *src);


typedef struct {
    njs_str_t           name;

    size_t              size;
    njs_hash_init       init;
    njs_hash_update     update;
    njs_hash_final      final;
} njs_hash_alg_t;

typedef struct {
    union {
        njs_md5_t       md5;
        njs_sha1_t      sha1;
        njs_sha2_t      sha2;
    } u;

    njs_hash_alg_t      *alg;
} njs_digest_t;

typedef struct {
    u_char              opad[64];

    union {
        njs_md5_t       md5;
        njs_sha1_t      sha1;
        njs_sha2_t      sha2;
    } u;

    njs_hash_alg_t      *alg;
} njs_hmac_t;


typedef struct {
    njs_str_t             name;

    njs_digest_encode     encode;
} njs_crypto_enc_t;


static njs_hash_alg_t *njs_crypto_algorithm(njs_vm_t *vm,
    njs_value_t *value);
static njs_crypto_enc_t *njs_crypto_encoding(njs_vm_t *vm,
    njs_value_t *value);
static njs_int_t njs_buffer_digest(njs_vm_t *vm, njs_value_t *value,
    const njs_str_t *src);
static njs_int_t njs_crypto_create_hash(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t unused, njs_value_t *retval);
static njs_int_t njs_hash_prototype_update(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t hmac, njs_value_t *retval);
static njs_int_t njs_hash_prototype_digest(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t hmac, njs_value_t *retval);
static njs_int_t njs_hash_prototype_copy(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t hmac, njs_value_t *retval);
static njs_int_t njs_crypto_create_hmac(njs_vm_t *vm, njs_value_t *args,
    njs_uint_t nargs, njs_index_t unused, njs_value_t *retval);

static njs_int_t njs_crypto_init(njs_vm_t *vm);


static njs_hash_alg_t njs_hash_algorithms[] = {

   {
     njs_str("md5"),
     16,
     (njs_hash_init) njs_md5_init,
     (njs_hash_update) njs_md5_update,
     (njs_hash_final) njs_md5_final
   },

   {
     njs_str("sha1"),
     20,
     (njs_hash_init) njs_sha1_init,
     (njs_hash_update) njs_sha1_update,
     (njs_hash_final) njs_sha1_final
   },

   {
     njs_str("sha256"),
     32,
     (njs_hash_init) njs_sha2_init,
     (njs_hash_update) njs_sha2_update,
     (njs_hash_final) njs_sha2_final
   },

   {
    njs_null_str,
    0,
    NULL,
    NULL,
    NULL
   }

};


static njs_crypto_enc_t njs_encodings[] = {

   {
     njs_str("buffer"),
     njs_buffer_digest
   },

   {
     njs_str("hex"),
     njs_string_hex
   },

   {
     njs_str("base64"),
     njs_string_base64
   },

   {
     njs_str("base64url"),
     njs_string_base64url
   },

   {
    njs_null_str,
    NULL
   }
};


static njs_external_t  njs_ext_crypto_hash[] = {

    {
        .flags = NJS_EXTERN_PROPERTY | NJS_EXTERN_SYMBOL,
        .name.symbol = NJS_SYMBOL_TO_STRING_TAG,
        .u.property = {
            .value = "Hash",
        }
    },

    {
        .flags = NJS_EXTERN_METHOD,
        .name.string = njs_str("update"),
        .writable = 1,
        .configurable = 1,
        .u.method = {
            .native = njs_hash_prototype_update,
            .magic8 = 0,
        }
    },

    {
        .flags = NJS_EXTERN_METHOD,
        .name.string = njs_str("digest"),
        .writable = 1,
        .configurable = 1,
        .u.method = {
            .native = njs_hash_prototype_digest,
            .magic8 = 0,
        }
    },

    {
        .flags = NJS_EXTERN_METHOD,
        .name.string = njs_str("copy"),
        .writable = 1,
        .configurable = 1,
        .u.method = {
            .native = njs_hash_prototype_copy,
        }
    },

    {
        .flags = NJS_EXTERN_METHOD,
        .name.string = njs_str("constructor"),
        .writable = 1,
        .configurable = 1,
        .u.method = {
            .native = njs_crypto_create_hash,
        }
    },
};


static njs_external_t  njs_ext_crypto_hmac[] = {

    {
        .flags = NJS_EXTERN_PROPERTY | NJS_EXTERN_SYMBOL,
        .name.symbol = NJS_SYMBOL_TO_STRING_TAG,
        .u.property = {
            .value = "Hmac",
        }
    },

    {
        .flags = NJS_EXTERN_METHOD,
        .name.string = njs_str("update"),
        .writable = 1,
        .configurable = 1,
        .u.method = {
            .native = njs_hash_prototype_update,
            .magic8 = 1,
        }
    },

    {
        .flags = NJS_EXTERN_METHOD,
        .name.string = njs_str("digest"),
        .writable = 1,
        .configurable = 1,
        .u.method = {
            .native = njs_hash_prototype_digest,
            .magic8 = 1,
        }
    },

    {
        .flags = NJS_EXTERN_METHOD,
        .name.string = njs_str("constructor"),
        .writable = 1,
        .configurable = 1,
        .u.method = {
            .native = njs_crypto_create_hmac,
            .magic8 = 0,
        }
    },
};


static njs_external_t  njs_ext_crypto_crypto[] = {

    {
        .flags = NJS_EXTERN_PROPERTY | NJS_EXTERN_SYMBOL,
        .name.symbol = NJS_SYMBOL_TO_STRING_TAG,
        .u.property = {
            .value = "crypto",
        }
    },

    {
        .flags = NJS_EXTERN_METHOD,
        .name.string = njs_str("createHash"),
        .writable = 1,
        .configurable = 1,
        .enumerable = 1,
        .u.method = {
            .native = njs_crypto_create_hash,
            .magic8 = 0,
        }
    },

    {
        .flags = NJS_EXTERN_METHOD,
        .name.string = njs_str("createHmac"),
        .writable = 1,
        .configurable = 1,
        .enumerable = 1,
        .u.method = {
            .native = njs_crypto_create_hmac,
            .magic8 = 0,
        }
    },
};


static njs_int_t    njs_crypto_hash_proto_id;
static njs_int_t    njs_crypto_hmac_proto_id;


njs_module_t  njs_crypto_module = {
    .name = njs_str("crypto"),
    .preinit = NULL,
    .init = njs_crypto_init,
};


static njs_int_t
njs_crypto_create_hash(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused, njs_value_t *retval)
{
    njs_digest_t    *dgst;
    njs_hash_alg_t  *alg;

    alg = njs_crypto_algorithm(vm, njs_arg(args, nargs, 1));
    if (njs_slow_path(alg == NULL)) {
        return NJS_ERROR;
    }

    dgst = njs_mp_alloc(njs_vm_memory_pool(vm), sizeof(njs_digest_t));
    if (njs_slow_path(dgst == NULL)) {
        njs_vm_memory_error(vm);
        return NJS_ERROR;
    }

    dgst->alg = alg;

    alg->init(&dgst->u);

    return njs_vm_external_create(vm, retval, njs_crypto_hash_proto_id,
                                  dgst, 0);
}


static njs_int_t
njs_hash_prototype_update(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t hmac, njs_value_t *retval)
{
    njs_str_t                    data;
    njs_int_t                    ret;
    njs_hmac_t                   *ctx;
    njs_value_t                  *this, *value;
    njs_digest_t                 *dgst;
    njs_opaque_value_t           result;
    const njs_buffer_encoding_t  *enc;

    this = njs_argument(args, 0);

    if (!hmac) {
        dgst = njs_vm_external(vm, njs_crypto_hash_proto_id, this);
        if (njs_slow_path(dgst == NULL)) {
            njs_vm_type_error(vm, "\"this\" is not a hash object");
            return NJS_ERROR;
        }

        if (njs_slow_path(dgst->alg == NULL)) {
            njs_vm_error(vm, "Digest already called");
            return NJS_ERROR;
        }

        ctx = NULL;

    } else {
        ctx = njs_vm_external(vm, njs_crypto_hmac_proto_id, this);
        if (njs_slow_path(ctx == NULL)) {
            njs_vm_type_error(vm, "\"this\" is not a hmac object");
            return NJS_ERROR;
        }

        if (njs_slow_path(ctx->alg == NULL)) {
            njs_vm_error(vm, "Digest already called");
            return NJS_ERROR;
        }

        dgst = NULL;
    }

    value = njs_arg(args, nargs, 1);

    if (njs_value_is_string(value)) {
        enc = njs_buffer_encoding(vm, njs_arg(args, nargs, 2), 1);
        if (njs_slow_path(enc == NULL)) {
            return NJS_ERROR;
        }

        ret = njs_buffer_decode_string(vm, value, njs_value_arg(&result), enc);
        if (njs_slow_path(ret != NJS_OK)) {
            return NJS_ERROR;
        }

        njs_value_string_get(njs_value_arg(&result), &data);

    } else if (njs_value_is_buffer(value)) {
        ret = njs_value_buffer_get(vm, value, &data);
        if (njs_slow_path(ret != NJS_OK)) {
            return NJS_ERROR;
        }

    } else {
        njs_vm_type_error(vm, "data is not a string or Buffer-like object");

        return NJS_ERROR;
    }

    if (!hmac) {
        dgst->alg->update(&dgst->u, data.start, data.length);

    } else {
        ctx->alg->update(&ctx->u, data.start, data.length);
    }

    njs_value_assign(retval, this);

    return NJS_OK;
}


static njs_int_t
njs_hash_prototype_digest(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t hmac, njs_value_t *retval)
{
    njs_str_t         str;
    njs_hmac_t        *ctx;
    njs_value_t       *this;
    njs_digest_t      *dgst;
    njs_hash_alg_t    *alg;
    njs_crypto_enc_t  *enc;
    u_char            hash1[32], digest[32];

    this = njs_argument(args, 0);

    if (!hmac) {
        dgst = njs_vm_external(vm, njs_crypto_hash_proto_id, this);
        if (njs_slow_path(dgst == NULL)) {
            njs_vm_type_error(vm, "\"this\" is not a hash object");
            return NJS_ERROR;
        }

        if (njs_slow_path(dgst->alg == NULL)) {
            goto exception;
        }

        ctx = NULL;

    } else {
        ctx = njs_vm_external(vm, njs_crypto_hmac_proto_id, this);
        if (njs_slow_path(ctx == NULL)) {
            njs_vm_type_error(vm, "\"this\" is not a hmac object");
            return NJS_ERROR;
        }

        if (njs_slow_path(ctx->alg == NULL)) {
            goto exception;
        }

        dgst = NULL;
    }

    enc = njs_crypto_encoding(vm, njs_arg(args, nargs, 1));
    if (njs_slow_path(enc == NULL)) {
        return NJS_ERROR;
    }

    if (!hmac) {
        alg = dgst->alg;
        alg->final(digest, &dgst->u);
        dgst->alg = NULL;

    } else {
        alg = ctx->alg;
        alg->final(hash1, &ctx->u);

        alg->init(&ctx->u);
        alg->update(&ctx->u, ctx->opad, 64);
        alg->update(&ctx->u, hash1, alg->size);
        alg->final(digest, &ctx->u);
        ctx->alg = NULL;
    }

    str.start = digest;
    str.length = alg->size;

    return enc->encode(vm, retval, &str);

exception:

    njs_vm_error(vm, "Digest already called");

    return NJS_ERROR;
}


static njs_int_t
njs_hash_prototype_copy(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused, njs_value_t *retval)
{
    njs_digest_t  *dgst, *copy;

    dgst = njs_vm_external(vm, njs_crypto_hash_proto_id, njs_argument(args, 0));
    if (njs_slow_path(dgst == NULL)) {
        njs_vm_type_error(vm, "\"this\" is not a hash object");
        return NJS_ERROR;
    }

    if (njs_slow_path(dgst->alg == NULL)) {
        njs_vm_error(vm, "Digest already called");
        return NJS_ERROR;
    }

    copy = njs_mp_alloc(njs_vm_memory_pool(vm), sizeof(njs_digest_t));
    if (njs_slow_path(copy == NULL)) {
        njs_vm_memory_error(vm);
        return NJS_ERROR;
    }

    memcpy(copy, dgst, sizeof(njs_digest_t));

    return njs_vm_external_create(vm, retval,
                                  njs_crypto_hash_proto_id, copy, 0);
}


static njs_int_t
njs_crypto_create_hmac(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused, njs_value_t *retval)
{
    njs_int_t                    ret;
    njs_str_t                    key;
    njs_uint_t                   i;
    njs_hmac_t                   *ctx;
    njs_value_t                  *value;
    njs_hash_alg_t               *alg;
    njs_opaque_value_t           result;
    const njs_buffer_encoding_t  *enc;
    u_char                       digest[32], key_buf[64];

    alg = njs_crypto_algorithm(vm, njs_arg(args, nargs, 1));
    if (njs_slow_path(alg == NULL)) {
        return NJS_ERROR;
    }

    value = njs_arg(args, nargs, 2);

    if (njs_value_is_string(value)) {
        enc = njs_buffer_encoding(vm, njs_value_arg(&njs_value_undefined), 1);
        if (njs_slow_path(enc == NULL)) {
            return NJS_ERROR;
        }

        ret = njs_buffer_decode_string(vm, value, njs_value_arg(&result), enc);
        if (njs_slow_path(ret != NJS_OK)) {
            return NJS_ERROR;
        }

        njs_value_string_get(njs_value_arg(&result), &key);

    } else if (njs_value_is_buffer(value)) {
        ret = njs_value_buffer_get(vm, value, &key);
        if (njs_slow_path(ret != NJS_OK)) {
            return NJS_ERROR;
        }

    } else {
        njs_vm_type_error(vm, "key is not a string or Buffer-like object");

        return NJS_ERROR;
    }

    ctx = njs_mp_alloc(njs_vm_memory_pool(vm), sizeof(njs_hmac_t));
    if (njs_slow_path(ctx == NULL)) {
        njs_vm_memory_error(vm);
        return NJS_ERROR;
    }

    ctx->alg = alg;

    if (key.length > sizeof(key_buf)) {
        alg->init(&ctx->u);
        alg->update(&ctx->u, key.start, key.length);
        alg->final(digest, &ctx->u);

        memcpy(key_buf, digest, alg->size);
        njs_explicit_memzero(key_buf + alg->size, sizeof(key_buf) - alg->size);

    } else {
        memcpy(key_buf, key.start, key.length);
        njs_explicit_memzero(key_buf + key.length,
                             sizeof(key_buf) - key.length);
    }

    for (i = 0; i < 64; i++) {
        ctx->opad[i] = key_buf[i] ^ 0x5c;
    }

    for (i = 0; i < 64; i++) {
         key_buf[i] ^= 0x36;
    }

    alg->init(&ctx->u);
    alg->update(&ctx->u, key_buf, 64);

    return njs_vm_external_create(vm, retval, njs_crypto_hmac_proto_id,
                                  ctx, 0);
}


static njs_hash_alg_t *
njs_crypto_algorithm(njs_vm_t *vm, njs_value_t *value)
{
    njs_str_t       name;
    njs_hash_alg_t  *e;

    if (njs_slow_path(!njs_value_is_string(value))) {
        njs_vm_type_error(vm, "algorithm must be a string");
        return NULL;
    }

    njs_value_string_get(value, &name);

    for (e = &njs_hash_algorithms[0]; e->name.length != 0; e++) {
        if (njs_strstr_eq(&name, &e->name)) {
            return e;
        }
    }

    njs_vm_type_error(vm, "not supported algorithm: \"%V\"", &name);

    return NULL;
}


static njs_crypto_enc_t *
njs_crypto_encoding(njs_vm_t *vm, njs_value_t *value)
{
    njs_str_t         name;
    njs_crypto_enc_t  *e;

    if (njs_slow_path(!njs_value_is_string(value))) {
        if (!njs_value_is_undefined(value)) {
            njs_vm_type_error(vm, "encoding must be a string");
            return NULL;
        }

        return &njs_encodings[0];
    }

    njs_value_string_get(value, &name);

    for (e = &njs_encodings[1]; e->name.length != 0; e++) {
        if (njs_strstr_eq(&name, &e->name)) {
            return e;
        }
    }

    njs_vm_type_error(vm, "Unknown digest encoding: \"%V\"", &name);

    return NULL;
}


static njs_int_t
njs_buffer_digest(njs_vm_t *vm, njs_value_t *value, const njs_str_t *src)
{
    return njs_buffer_new(vm, value, src->start, src->length);
}


static njs_int_t
njs_crypto_init(njs_vm_t *vm)
{
    njs_int_t           ret, proto_id;
    njs_mod_t           *module;
    njs_opaque_value_t  value;

    njs_crypto_hash_proto_id =
                     njs_vm_external_prototype(vm, njs_ext_crypto_hash,
                                               njs_nitems(njs_ext_crypto_hash));
    if (njs_slow_path(njs_crypto_hash_proto_id < 0)) {
        return NJS_ERROR;
    }

    njs_crypto_hmac_proto_id =
                     njs_vm_external_prototype(vm, njs_ext_crypto_hmac,
                                               njs_nitems(njs_ext_crypto_hmac));
    if (njs_slow_path(njs_crypto_hmac_proto_id < 0)) {
        return NJS_ERROR;
    }

    proto_id = njs_vm_external_prototype(vm, njs_ext_crypto_crypto,
                                         njs_nitems(njs_ext_crypto_crypto));
    if (njs_slow_path(proto_id < 0)) {
        return NJS_ERROR;
    }

    ret = njs_vm_external_create(vm, njs_value_arg(&value), proto_id, NULL, 1);
    if (njs_slow_path(ret != NJS_OK)) {
        return NJS_ERROR;
    }

    module = njs_vm_add_module(vm, &njs_str_value("crypto"),
                               njs_value_arg(&value));
    if (njs_slow_path(module == NULL)) {
        return NJS_ERROR;
    }

    return NJS_OK;
}
