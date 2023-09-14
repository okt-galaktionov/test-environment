/* SPDX-License-Identifier: Apache-2.0 */
/** @file
 * @brief API to deal with buffers
 *
 * Allocation of buffers, fill in by random numbers, etc.
 *
 *
 * Copyright (C) 2004-2022 OKTET Labs Ltd. All rights reserved.
 */

#define TE_LGR_USER     "TE Buffers"

#include "te_config.h"

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <stdio.h>
#endif

#if HAVE_STDARG_H
#include <stdarg.h>
#endif

#include "te_defs.h"
#include "logger_api.h"
#include "te_alloc.h"
#include "te_intset.h"
#include "te_bufs.h"
#include "te_hex_diff_dump.h"

#define FILL_SPEC_ESC_CHAR '`'

static te_errno
parse_byte_set(const char **spec, te_charset *set)
{
    const char *iter = *spec;

    te_charset_clear(set);

    if (*iter == FILL_SPEC_ESC_CHAR)
    {
        iter++;
        te_charset_add_range(set, (uint8_t)*iter, (uint8_t)*iter);
    }
    else if (*iter != '[')
    {
        te_charset_add_range(set, (uint8_t)*iter, (uint8_t)*iter);
    }
    else
    {
        te_bool except = FALSE;
        te_bool empty_range = TRUE;

        for (iter++; *iter != ']'; iter++)
        {
            uint8_t minch = 0;
            uint8_t maxch = 0;

            switch (*iter)
            {
                case '^':
                    if (empty_range)
                    {
                        te_charset_add_range(set, 0, UINT8_MAX);
                        empty_range = FALSE;
                    }
                    except = !except;
                    continue;
                case FILL_SPEC_ESC_CHAR:
                    iter++;
                    minch = maxch = *iter;
                    break;
                case '\0':
                    ERROR("Unterminated ']'");
                    return TE_EILSEQ;
                default:
                    if (iter[1] == '-' && iter[2] != ']' && iter[2] != '\0')
                    {
                        minch = (uint8_t)*iter;
                        maxch = (uint8_t)iter[2];
                        iter += 2;
                    }
                    else
                    {
                        minch = maxch = (uint8_t)*iter;
                    }
                    break;
            }

            if (except)
                te_charset_remove_range(set, minch, maxch);
            else
                te_charset_add_range(set, minch, maxch);
            empty_range = FALSE;
        }

        if (empty_range)
            te_charset_add_range(set, 0, UINT8_MAX);
    }

    *spec = iter + 1;
    return 0;
}

te_errno
te_compile_buf_pattern(const char *spec, uint8_t *storage,
                       size_t max_size,
                       te_buf_pattern *pattern)
{
    size_t remaining = max_size;

    pattern->start = storage;
    pattern->suffix_len = 0;
    pattern->repeat = pattern->suffix = pattern->end = NULL;

    while (*spec != '\0')
    {
        te_errno rc = 0;
        te_charset cset;
        size_t need_space;

        if (*spec == '(')
        {
            spec++;

            if (pattern->repeat != NULL)
            {
                ERROR("Multiple repeat sections");
                return TE_EINVAL;
            }
            pattern->repeat = storage;
        }
        else if (*spec == ')' &&
                 pattern->repeat != NULL &&
                 pattern->suffix == NULL)
        {
            spec++;
            pattern->suffix = storage;
            continue;
        }

        rc = parse_byte_set(&spec, &cset);
        if (rc != 0)
            return rc;

        need_space = cset.n_items;
        if (need_space == UINT8_MAX + 1)
            need_space = 1;
        else
            need_space++;
        if (remaining < need_space)
        {
            ERROR("Not enough space for compiled pattern, needed %zu",
                  need_space);
            return TE_ENOBUFS;
        }

        *storage++ = (uint8_t)cset.n_items;
        if (cset.n_items != UINT8_MAX + 1)
        {
            te_charset_get_bytes(&cset, storage);
            storage += cset.n_items;
        }
        remaining -= need_space;
        if (pattern->suffix != NULL)
            pattern->suffix_len++;
    }

    pattern->end = storage;
    if (pattern->repeat == NULL)
    {
        pattern->repeat = pattern->start;
        pattern->suffix = pattern->end;
    }
    else if (pattern->suffix == NULL)
    {
        ERROR("Unterminated '('");
        return TE_EILSEQ;
    }

    if (pattern->end == pattern->start)
    {
        ERROR("Empty pattern");
        return TE_ENODATA;
    }
    return 0;
}

static uint8_t
fill_pattern_byte(uint8_t **pattern)
{
    uint8_t byte;

    if (**pattern == 0)
    {
        byte = rand_range(0, UINT8_MAX);
        (*pattern)++;
    }
    else
    {
        byte = rand_range(0, **pattern - 1);
        byte = (*pattern)[byte + 1];
        (*pattern) += **pattern + 1;
    }

    return byte;
}

void
te_fill_pattern_buf(void *buf, size_t len, const te_buf_pattern *pattern)
{
    static uint8_t any_byte[1] = {0};
    static const te_buf_pattern any_byte_pattern = {
        .start = any_byte,
        .end = any_byte + 1,
        .repeat = any_byte,
        .suffix = any_byte + 1,
        .suffix_len = 0,
    };
    uint8_t *buf_ptr;
    uint8_t *pat_ptr;
    size_t remain;

    if (len == 0)
        return;

    assert(buf != NULL);

    if (pattern == NULL)
        pattern = &any_byte_pattern;

    pat_ptr = pattern->start;
    buf_ptr = buf;
    for (remain = len; remain > pattern->suffix_len; remain--)
    {
        assert(pat_ptr < pattern->end);

        *buf_ptr++ = fill_pattern_byte(&pat_ptr);

        if (pat_ptr == pattern->suffix)
            pat_ptr = pattern->repeat;
    }
    pat_ptr = pattern->suffix;
    for (; remain > 0; remain--)
    {
        assert(pat_ptr < pattern->end);

        *buf_ptr++ = fill_pattern_byte(&pat_ptr);
    }
}

void *
te_make_pattern_buf(size_t min, size_t max, size_t *p_len,
                    const te_buf_pattern *pattern)
{
    size_t len;
    void *buf;

    assert(min <= max);
    len = rand_range(min, max);

    /*
     * There is nothing wrong with asking for a zero-length buffer.
     * However, TE_ALLOC intentionally fails on zero length requests,
     * so we allocate a single-byte buffer instead.
     */
    buf = TE_ALLOC(len == 0 ? 1 : len);

    te_fill_pattern_buf(buf, len, pattern);

    if (p_len != NULL)
        *p_len = len;

    return buf;
}

te_errno
te_fill_spec_buf(void *buf, size_t len, const char *spec)
{
    uint8_t pat_storage[1024];
    te_buf_pattern pattern;
    te_errno rc;

    rc = te_compile_buf_pattern(spec, pat_storage, sizeof(pat_storage),
                                &pattern);
    if (rc != 0)
        return rc;

    te_fill_pattern_buf(buf, len, &pattern);

    return 0;
}

void *
te_make_spec_buf(size_t min, size_t max, size_t *p_len, const char *spec)
{
    uint8_t pat_storage[1024];
    te_buf_pattern pattern;
    te_errno rc;

    rc = te_compile_buf_pattern(spec, pat_storage, sizeof(pat_storage),
                                &pattern);
    if (rc != 0)
    {
        ERROR("Invalid pattern spec: %r", rc);
        return NULL;
    }

    return te_make_pattern_buf(min, max, p_len, &pattern);
}

te_bool
te_compare_bufs(const void *exp_buf, size_t exp_len,
                unsigned int n_copies,
                const void *actual_buf, size_t actual_len,
                unsigned int log_level)
{
    te_bool result = TRUE;
    size_t offset = 0;

    if (n_copies * exp_len != actual_len)
    {
        /* If we don't log anything, there's no need to look for more diffs. */
        if (log_level == 0)
            return FALSE;

        LOG_MSG(log_level, "Buffer lengths are not equal: "
                "%zu * %u != %zu", exp_len, n_copies, actual_len);

        result = FALSE;
    }

    while (n_copies != 0)
    {
        size_t chunk_len = MIN(exp_len, actual_len);

        if (memcmp(exp_buf, actual_buf, chunk_len) != 0 ||
            chunk_len < exp_len)
        {
            if (log_level == 0)
                return FALSE;

            result = FALSE;
            LOG_HEX_DIFF_DUMP_AT(log_level, exp_buf, exp_len,
                                 actual_buf, chunk_len, offset);
        }
        offset += chunk_len;
        actual_buf = (const uint8_t *)actual_buf + chunk_len;
        actual_len -= chunk_len;
        n_copies--;
    }

    if (actual_len > 0 && log_level != 0)
    {
        LOG_HEX_DIFF_DUMP_AT(log_level, exp_buf, 0,
                             actual_buf, actual_len, offset);
    }

    return result;
}
