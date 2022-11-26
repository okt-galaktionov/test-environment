/* SPDX-License-Identifier: Apache-2.0 */
/** @file
 * @brief Testing Results Comparator: common
 *
 * Routines to work with strings.
 *
 *
 * Copyright (C) 2004-2022 OKTET Labs Ltd. All rights reserved.
 */

#include "te_config.h"

#ifdef STDC_HEADERS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "logger_api.h"

#include "te_string.h"
#include "te_defs.h"

void
te_string_free_heap(te_string *str)
{
    str->len = str->size = 0;
    free(str->ptr);
    str->ptr = NULL;
}

void
te_string_free(te_string *str)
{
    if (str == NULL)
        return;

    if (str->free_func == NULL)
    {
        ERROR("%s(): free_func must not be NULL. te_string_free_heap() will "
              "be used but you must fix your code initializing te_string "
              "properly!", __FUNCTION__);
        te_string_free_heap(str);
        return;
    }

    str->free_func(str);
}

te_errno
te_string_append(te_string *str, const char *fmt, ...)
{
    va_list  ap;
    te_errno rc;

    va_start(ap, fmt);
    rc = te_string_append_va(str, fmt, ap);
    va_end(ap);
    return rc;
}

te_errno
te_string_append_chk(te_string *str, const char *fmt, ...)
{
    va_list  ap;
    te_errno rc;

    va_start(ap, fmt);
    rc = te_string_append_va_chk(str, fmt, ap);
    va_end(ap);
    return rc;
}


/* See description in te_string.h */
te_errno
te_string_reserve(te_string *str, size_t size)
{
    size_t          pagesize = getpagesize();
    size_t          adj_size;
    size_t          malloc_header_size = 4 * sizeof(void *);
    char           *ptr;
    size_t          grow_factor = 1;
    size_t          grow = 0;

    /*
     * Here we don't reinvent the wheel, we comply to the GCC C++ library
     * https://github.com/gcc-mirror/gcc/blob/master/libstdc++-v3/include/
     *  bits/basic_string.tcc
     * with the exception of grow factor exponent used.
     */
    if (size <= str->size)
        return 0;

    if (str->ext_buf)
        TE_FATAL_ERROR("cannot resize external buffer");

    /*
     * Apply grow factor ^ exp until predefined limit, and if size < newsize <
     * (factor ^ exp) * size, then use (factor ^ exp) * size as a resulting
     * size.
     *
     * Using factor ^ exp might be costly in terms of RAM used, so we fall back
     * to a regular addend-based expansion if we can't find it even after
     * applying @c TE_STRING_GROW_FACTOR_EXP_LIMIT exponent
     */
    for (grow = 0; grow < TE_STRING_GROW_FACTOR_EXP_LIMIT; ++grow)
    {
        grow_factor *= TE_STRING_GROW_FACTOR;
        if (size < grow_factor * str->size)
        {
            size = grow_factor * str->size;
            break;
        }
    }

    /*
     * Apply correction taking malloc overhead into account, it works for
     * allocations over page size. Based on GCC C++ basic_string implementation.
     */
    adj_size = size + malloc_header_size;
    if (adj_size > pagesize)
    {
        size_t extra;

        extra = pagesize - adj_size % pagesize;
        size += extra;
    }

    /* Actually reallocate data */
    ptr = realloc(str->ptr, size);
    if (ptr == NULL)
        TE_FATAL_ERROR("Memory allocation failure");

    str->ptr = ptr;
    str->size = size;
    return 0;
}

te_errno
te_string_append_va_chk(te_string *str, const char *fmt, va_list ap)
{
    te_errno rc;
    char    *s;
    size_t   rest;
    int      printed;
    te_bool  again;
    va_list  ap_start;

    if (str->ptr == NULL)
    {
        size_t new_size;

        assert(!str->ext_buf);

        new_size = str->size != 0 ? str->size : TE_STRING_INIT_LEN;
        str->ptr = malloc(new_size);
        if (str->ptr == NULL)
            TE_FATAL_ERROR("Memory allocation failure");

        str->size = new_size;
        str->len = 0;
    }

    assert(str->size > str->len);
    rest = str->size - str->len;
    do  {
        s = str->ptr + str->len;

        va_copy(ap_start, ap);
        printed = vsnprintf(s, rest, fmt, ap_start);
        va_end(ap_start);
        assert(printed >= 0);

        if ((size_t)printed >= rest)
        {
            if (str->ext_buf)
            {
                str->len = str->size - 1 /* '\0' */;
                return TE_ENOBUFS;
            }
            else
            {
                rc = te_string_reserve(str, str->len + printed + 1 /* '\0' */);
                if (rc != 0)
                    return rc;

                rest = str->size - str->len;
                /* Print again */
                again = TRUE;
            }
        }
        else
        {
            str->len += printed;
            again = FALSE;
        }
    } while (again);

    return 0;
}

te_errno
te_string_append_va(te_string *str, const char *fmt, va_list ap)
{
    te_errno rc = te_string_append_va_chk(str, fmt, ap);

    if (rc != 0)
        TE_FATAL_ERROR("Not enough space in supplied buffer");

    return 0;
}

te_errno
te_string_append_buf(te_string *str, const char *buf, size_t len)
{
    size_t req_len;
    te_errno rc;

    if (len == 0)
        return 0;

    req_len = str->len + len;
    if (buf[len - 1] != '\0')
        req_len++;

    rc = te_string_reserve(str, req_len);
    if (rc != 0)
        return rc;

    memcpy(str->ptr + str->len, buf, len);
    str->ptr[req_len - 1] = '\0';
    str->len = req_len - 1;
    return 0;
}

te_errno
te_string_append_shell_arg_as_is(te_string *str, const char *arg)
{
    te_errno rc = 0;
    te_string arg_escaped = TE_STRING_INIT;

    do {
        const char *p;
        int len;

        p = strchr(arg, '\'');
        if (p == NULL)
            len = strlen(arg);
        else
            len = p - arg;

        /* Print up to ' or end of string */
        rc = te_string_append(&arg_escaped, "'%.*s'", len, arg);
        if (rc != 0)
            break;
        arg += len;

        if (*arg == '\'')
        {
            rc = te_string_append(&arg_escaped, "\\\'");
            if (rc != 0)
                break;
            arg++;
        }
    } while (*arg != '\0');

    if (rc != 0)
    {
        te_string_free(&arg_escaped);
        return rc;
    }

    return te_string_append(str, "%s", arg_escaped.ptr);
}

te_errno
te_string_append_shell_args_as_is(te_string *str, ...)
{
    va_list args;
    const char *arg;
    te_errno rc = 0;

    va_start(args, str);
    while ((arg = va_arg(args, const char *)) != NULL)
    {
        if (str->len != 0)
        {
            rc = te_string_append(str, " ");
            if (rc != 0)
                break;
        }

        rc = te_string_append_shell_arg_as_is(str, arg);
        if (rc != 0)
            break;
    }
    va_end(args);

    return rc;
}

te_errno
te_string_join_vec(te_string *str, const te_vec *strvec,
                   const char *sep)
{
    te_bool need_sep = FALSE;
    const char * const *item;
    te_errno rc;

    TE_VEC_FOREACH(strvec, item)
    {
        /*
         * Seems meaningless to add "(null)" to TE string
         * for NULL pointers.
         */
        if (*item == NULL)
            continue;

        rc = te_string_append(str, "%s%s",
                              need_sep ? sep : "",
                              *item);
        if (rc != 0)
            return rc;
        need_sep = TRUE;
    }

    return 0;
}

char *
te_string_fmt_va(const char *fmt,
                 va_list     ap)
{
    te_string str = TE_STRING_INIT;

    if (te_string_append_va(&str, fmt, ap) != 0)
        te_string_free(&str);

    return str.ptr;
}

char *
te_string_fmt(const char *fmt, ...)
{
    va_list  ap;
    char *result;

    va_start(ap, fmt);
    result = te_string_fmt_va(fmt, ap);
    va_end(ap);

    return result;
}

void
te_string_cut_beginning(te_string *str, size_t len)
{
    assert(str != NULL);
    if (len > str->len)
        len = str->len;

    str->len -= len;

    if (str->ptr != NULL)
    {
        memmove(str->ptr, str->ptr + len, str->len);
        str->ptr[str->len] = '\0';
    }
}

void
te_string_cut(te_string *str, size_t len)
{
    assert(str != NULL);
    if (len > str->len)
        len = str->len;
    assert(str->len >= len);
    str->len -= len;
    if (str->ptr != NULL)
        str->ptr[str->len] = '\0';
}

void
te_string_add_centered(te_string *str, const char *src,
                       size_t padlen, char padchar)
{
    char *dest;
    size_t src_len = strlen(src);

    te_string_reserve(str, str->len + padlen + 1);

    if (src_len > padlen)
        src_len = padlen;

    dest = str->ptr + str->len;
    /* If we cannot center exactly, prefer shifting to the right */
    memset(dest, padchar, (padlen - src_len + 1) / 2);
    dest += (padlen - src_len + 1) / 2;
    memcpy(dest, src, src_len);
    dest += src_len;
    memset(dest, padchar, (padlen - src_len) / 2);
    str->len += padlen;
    str->ptr[str->len] = '\0';
}

char *
raw2string(const uint8_t *data, size_t size)
{
    te_string  str = TE_STRING_INIT;
    size_t i = 0;

    te_string_append(&str, "[ ");

    for (i = 0; i < size; i++)
        te_string_append(&str, "%#02x ", data[i]);

    te_string_append(&str, "]");

    return str.ptr;
}

void
te_substring_find(te_substring_t *substr, const char *str)
{
    char *ch;

    if (!te_substring_is_valid(substr))
        return;

    ch = strstr(substr->base->ptr + substr->start, str);
    if (ch == NULL)
    {
        substr->start = SIZE_MAX;
        substr->len = 0;
    }
    else
    {
       substr->start = ch - substr->base->ptr;
       substr->len = strlen(str);
    }

    return;
}

te_errno
te_substring_replace(te_substring_t *substr, const char *str)
{
    te_errno rc;
    te_string tail = TE_STRING_INIT;

    if (substr->start + substr->len > substr->base->len)
    {
        ERROR("Substring position out of bounds");
        return TE_EINVAL;
    }

    // TODO: Rewrite this using te_string_reserve, memmove and memcpy
    rc = te_string_append(&tail, "%s",
                          substr->base->ptr + substr->start + substr->len);
    if (rc != 0)
        return rc;

    te_string_cut(substr->base, substr->base->len - substr->start);
    rc = te_string_append(substr->base, "%s%s", str, tail.ptr);
    if (rc != 0)
    {
        te_string_free(&tail);
        return rc;
    }

    substr->start += strlen(str);
    substr->len = 0;

    te_string_free(&tail);
    return 0;
}

void
te_substring_advance(te_substring_t *substr)
{
    substr->start += substr->len;
    substr->len = 0;
}

void
te_substring_limit(te_substring_t *substr, const te_substring_t *limit)
{
    substr->len = limit->start - substr->start;
}

static te_errno
replace_substring(te_substring_t *substr, const char *new,
                  const char *old)
{
    te_errno rc;

    te_substring_find(substr, old);

    if (!te_substring_is_valid(substr))
        return 0;

    rc = te_substring_replace(substr, new);
    if (rc != 0)
        ERROR("Failed to replace '%s' to '%s'", new, old);

    return rc;
}

te_errno
te_string_replace_substring(te_string *str, const char *new,
                            const char *old)
{
    te_substring_t iter = TE_SUBSTRING_INIT(str);

    return replace_substring(&iter, new, old);
}

te_errno
te_string_replace_all_substrings(te_string *str, const char *new,
                                 const char *old)
{
    te_substring_t iter = TE_SUBSTRING_INIT(str);
    te_errno rc = 0;

    while (1)
    {
        rc = replace_substring(&iter, new, old);
        if (rc != 0)
            break;

        if (!te_substring_is_valid(&iter))
            break;
    }

    return rc;
}
