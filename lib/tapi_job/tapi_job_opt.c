/* SPDX-License-Identifier: Apache-2.0 */
/** @file
 * @brief generic tool options TAPI
 *
 * TAPI to handle generic tool options (implementation).
 *
 * Copyright (C) 2019-2022 OKTET Labs Ltd. All rights reserved.
 */

#include "te_config.h"
#include <inttypes.h>

#include "te_defs.h"
#include "te_string.h"
#include "logger_api.h"
#include "tapi_job_opt.h"
#include "te_sockaddr.h"
#include "te_enum.h"

te_errno
tapi_job_opt_create_uint_t(const void *value, const void *priv, te_vec *args)
{
    tapi_job_opt_uint_t *p = (tapi_job_opt_uint_t *)value;

    UNUSED(priv);

    if (!p->defined)
        return TE_ENOENT;

    return te_vec_append_str_fmt(args, "%u", p->value);
}

te_errno
tapi_job_opt_create_uint_t_hex(const void *value, const void *priv,
                               te_vec *args)
{
    tapi_job_opt_uint_t *p = (tapi_job_opt_uint_t *)value;

    UNUSED(priv);

    if (!p->defined)
        return TE_ENOENT;

    return te_vec_append_str_fmt(args, "0x%x", p->value);
}

te_errno
tapi_job_opt_create_uint_t_octal(const void *value, const void *priv,
                                 te_vec *args)
{
    tapi_job_opt_uint_t *p = (tapi_job_opt_uint_t *)value;

    UNUSED(priv);

    if (!p->defined)
        return TE_ENOENT;

    return te_vec_append_str_fmt(args, "%o", p->value);
}

te_errno
tapi_job_opt_create_uintmax_t(const void *value, const void *priv, te_vec *args)
{
    tapi_job_opt_uintmax_t *p = (tapi_job_opt_uintmax_t *)value;

    UNUSED(priv);

    if (!p->defined)
        return TE_ENOENT;

    return te_vec_append_str_fmt(args, "%ju", p->value);
}

te_errno
tapi_job_opt_create_uint(const void *value, const void *priv, te_vec *args)
{
    unsigned int uint = *(const unsigned int *)value;

    UNUSED(priv);

    return te_vec_append_str_fmt(args, "%u", uint);
}

te_errno
tapi_job_opt_create_uint_omittable(const void *value, const void *priv,
                                   te_vec *args)
{
    if (*(const unsigned int *)value == TAPI_JOB_OPT_OMIT_UINT)
        return TE_ENOENT;

    return tapi_job_opt_create_uint(value, priv, args);
}

te_errno
tapi_job_opt_create_double_t(const void *value, const void *priv, te_vec *args)
{
    tapi_job_opt_double_t *p = (tapi_job_opt_double_t *)value;

    UNUSED(priv);

    if (!p->defined)
        return TE_ENOENT;

    return te_vec_append_str_fmt(args, "%f", p->value);
}

te_errno
tapi_job_opt_create_string(const void *value, const void *priv, te_vec *args)
{
    const char *str = *(const char *const *)value;

    UNUSED(priv);

    if (str == NULL)
        return TE_ENOENT;

    return te_vec_append_str_fmt(args, "%s", str);
}

te_errno
tapi_job_opt_create_bool(const void *value, const void *priv, te_vec *args)
{
    UNUSED(args);
    UNUSED(priv);

    return (*(const te_bool *)value) ? 0 : TE_ENOENT;
}

te_errno
tapi_job_opt_create_dummy(const void *value, const void *priv, te_vec *args)
{
    UNUSED(value);
    UNUSED(args);
    UNUSED(priv);

    /*
     * Function is just a dummy stuff which is required to
     * handle options without arguments in tapi_job_opt_build_args().
     */
    return 0;
}

te_errno
tapi_job_opt_create_sockaddr_ptr(const void *value, const void *priv,
                                 te_vec *args)
{
    const struct sockaddr **sa = (const struct sockaddr **)value;

    UNUSED(priv);

    if (sa == NULL || *sa == NULL)
        return TE_ENOENT;

    return te_vec_append_str_fmt(args, "%s", te_sockaddr_get_ipstr(*sa));
}

te_errno
tapi_job_opt_create_addr_port_ptr(const void *value, const void *priv,
                                  te_vec *args)
{
    const struct sockaddr **sa = (const struct sockaddr **)value;

    UNUSED(priv);

    if (sa == NULL || *sa == NULL)
        return TE_ENOENT;

    return te_vec_append_str_fmt(args, "%s:%" PRIu16,
                                 te_sockaddr_get_ipstr(*sa),
                                 ntohs(te_sockaddr_get_port(*sa)));
}

te_errno
tapi_job_opt_create_sockport_ptr(const void *value, const void *priv,
                                 te_vec *args)
{
    const struct sockaddr **sa = (const struct sockaddr **)value;

    UNUSED(priv);

    if (sa == NULL || *sa == NULL)
        return TE_ENOENT;

    return te_vec_append_str_fmt(args, "%" PRIu16,
                                 ntohs(te_sockaddr_get_port(*sa)));
}

te_errno
tapi_job_opt_create_sockaddr_subnet(const void *value, const void *priv,
                                    te_vec *args)
{
    const te_sockaddr_subnet *subnet = (const te_sockaddr_subnet *)value;

    UNUSED(priv);

    if (subnet->addr == NULL)
        return TE_ENOENT;

    return te_vec_append_str_fmt(args, "%s/%u",
                                 te_sockaddr_get_ipstr(subnet->addr),
                                 subnet->prefix_len);
}

te_errno
tapi_job_opt_create_enum(const void *value, const void *priv, te_vec *args)
{
    int ival = *(const int *)value;

    if (ival == TAPI_JOB_OPT_ENUM_UNDEF)
        return TE_ENOENT;

    return te_vec_append_str_fmt(args, "%s",
                                 te_enum_map_from_value(priv, ival));
}

te_errno
tapi_job_opt_create_enum_bool(const void *value, const void *priv, te_vec *args)
{
    te_bool bval = *(const te_bool *)value;

    return te_vec_append_str_fmt(args, "%s",
                                 te_enum_map_from_value(priv, bval));
}

/** See description in tapi_job_opt.h */
te_errno
tapi_job_opt_create_enum_bool3(const void *value, const void *priv,
                               te_vec *args)
{
    te_bool3 val = *(const te_bool3 *)value;

    if (val == TE_BOOL3_UNKNOWN)
        return TE_ENOENT;

    return te_vec_append_str_fmt(args, "%s",
                                 te_enum_map_from_value(priv, val));
}

/**
 * Append an argument @p arg that was processed by a formatting function
 * to arguments array @p args, with suffix/prefix if present.
 *
 * @param[in]  bind         Argument's prefix/suffix information.
 * @param[in]  arg          Argument vector to append, may be 0 size to append
 *                          only prefix/suffix. Prefix is concatenated with the
 *                          first element if the appropriate flag is set in
 *                          @p bind, suffix is always concatenated with the last
 *                          element.
 * @param[in,out] args      Vector to put resulting argument to.
 */
static te_errno
tapi_job_opt_append_arg_with_affixes(const tapi_job_opt_bind *bind,
                                      te_vec *arg, te_vec *args)
{
    te_bool do_concat_prefix = bind->concatenate_prefix && bind->prefix != NULL;
    size_t size;
    te_errno rc;
    size_t i;

    if (!do_concat_prefix && bind->prefix != NULL)
    {
        rc = te_vec_append_str_fmt(args, "%s", bind->prefix);
        if (rc != 0)
            return rc;
    }

    size = te_vec_size(arg);
    for (i = 0; i < size; i++)
    {
        const char *pfx = (do_concat_prefix && i == 0 ? bind->prefix : "");
        const char *suff = ((i + 1 == size && bind->suffix != NULL) ?
                            bind->suffix : "");

        rc = te_vec_append_str_fmt(args, "%s%s%s",
                                   pfx, TE_VEC_GET(const char *, arg, i), suff);
        if (rc != 0)
            return rc;
    }

    return 0;
}

static te_errno
tapi_job_opt_bind2str(const tapi_job_opt_bind *bind, const void *opt,
                       te_vec *args)
{
    te_vec arg_vec = TE_VEC_INIT_AUTOPTR(char *);
    const uint8_t *ptr = (const uint8_t *)opt + bind->opt_offset;
    te_errno rc;

    rc = bind->fmt_func(ptr, bind->priv, &arg_vec);

    if (rc != 0)
    {
        te_vec_free(&arg_vec);

        if (rc == TE_ENOENT)
            return 0;

        return rc;
    }

    rc = tapi_job_opt_append_arg_with_affixes(bind, &arg_vec, args);
    te_vec_free(&arg_vec);
    if (rc != 0)
        return rc;

    return 0;
}

static te_errno
tapi_job_opt_bind_args(const tapi_job_opt_bind *binds,
                       const void *opt, te_vec *tool_args)
{
    te_errno rc;
    const tapi_job_opt_bind *bind;

    for (bind = binds; bind->fmt_func != NULL; bind++)
    {
        rc = tapi_job_opt_bind2str(bind, opt, tool_args);
        if (rc != 0)
            return rc;
    }

    return TE_VEC_APPEND_RVALUE(tool_args, const char *, NULL);
}

te_errno
tapi_job_opt_build_args(const char *path, const tapi_job_opt_bind *binds,
                         const void *opt, te_vec *tool_args)
{
    te_vec args = TE_VEC_INIT_AUTOPTR(char *);
    te_errno rc;

    rc = te_vec_append_str_fmt(&args, "%s", path);
    if (rc != 0)
        goto out;

    if (binds != NULL)
    {
        rc = tapi_job_opt_bind_args(binds, opt, &args);
        if (rc != 0)
            goto out;
    }

out:
    if (rc != 0)
        te_vec_free(&args);

    *tool_args = args;

    return rc;
}

static void
tapi_job_opt_maybe_remove_trail(te_vec *tool_args)
{
    size_t len = te_vec_size(tool_args);

    if (len > 0 && TE_VEC_GET(const char *, tool_args, len - 1) == NULL)
        te_vec_remove_index(tool_args, len - 1);
}

te_errno
tapi_job_opt_append_strings(const char **items, te_vec *tool_args)
{
    te_errno rc;

    tapi_job_opt_maybe_remove_trail(tool_args);
    rc = te_vec_append_strarray(tool_args, items);
    if (rc == 0)
        rc = TE_VEC_APPEND_RVALUE(tool_args, const char *, NULL);

    return rc;
}

te_errno
tapi_job_opt_append_args(const tapi_job_opt_bind *binds, const void *opt,
                         te_vec *tool_args)
{
    tapi_job_opt_maybe_remove_trail(tool_args);

    return tapi_job_opt_bind_args(binds, opt, tool_args);
}

te_errno
tapi_job_opt_create_array(const void *value, const void *priv, te_vec *args)
{
    const tapi_job_opt_array *array = priv;
    tapi_job_opt_bind bind = array->bind;
    te_errno rc;
    size_t i;
    size_t len = *(const size_t *)value;
    const void *array_ptr = NULL;

    if (array->is_ptr)
        array_ptr = *(const void * const *)(value + array->array_offset);
    else
        array_ptr = value + array->array_offset;

    if (len > 0 && array_ptr == NULL)
        return TE_EINVAL;

    bind.opt_offset = 0;

    for (i = 0; i < len; i++, bind.opt_offset += array->element_size)
    {
        rc = tapi_job_opt_bind2str(&bind, array_ptr, args);
        if (rc != 0)
            return rc;
    }

    return 0;
}

te_errno
tapi_job_opt_create_embed_array(const void *value, const void *priv,
                                te_vec *args)
{
    te_vec sub_args = TE_VEC_INIT_AUTOPTR(char *);
    te_string combined = TE_STRING_INIT;
    const tapi_job_opt_array *array = priv;
    te_errno rc;

    rc = tapi_job_opt_create_array(value, array, &sub_args);
    if (rc != 0)
    {
        te_vec_free(&sub_args);
        te_string_free(&combined);
        return rc;
    }

    if (te_vec_size(&sub_args) == 0)
        return TE_ENOENT;

    rc = te_string_join_vec(&combined, &sub_args, array->sep);
    te_vec_free(&sub_args);
    if (rc != 0)
    {
        te_string_free(&combined);
        return rc;
    }

    /*
     * Transfer ownership of the string buffer into the vector,
     * so combined is *not* freed in a successful case
     */
    rc = TE_VEC_APPEND(args, combined.ptr);
    if (rc != 0)
        te_string_free(&combined);

    return rc;
}

te_errno
tapi_job_opt_create_struct(const void *value, const void *priv, te_vec *args)
{
    te_vec sub_args = TE_VEC_INIT_AUTOPTR(char *);
    te_string combined = TE_STRING_INIT;

    const tapi_job_opt_struct *data = priv;
    tapi_job_opt_bind *bind;

    te_errno rc;

    for (bind = data->binds; bind->fmt_func != NULL; bind++)
    {
        rc = tapi_job_opt_bind2str(bind, value, &sub_args);
        if (rc != 0)
        {
            te_vec_free(&sub_args);
            te_string_free(&combined);
            return rc;
        }
    }

    rc = te_string_join_vec(&combined, &sub_args, data->sep);
    te_vec_free(&sub_args);
    if (rc != 0)
    {
        te_string_free(&combined);
        return rc;
    }

    if (combined.ptr == NULL)
        return TE_ENOENT;

    /*
     * Transfer ownership of the string buffer into the vector,
     * so combined is *not* freed in a successful case
     */
    rc = TE_VEC_APPEND(args, combined.ptr);
    if (rc != 0)
        te_string_free(&combined);

    return rc;
}
