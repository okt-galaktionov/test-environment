/* SPDX-License-Identifier: Apache-2.0 */
/** @file
 * @brief Tail queue of strings (char *).
 *
 * Implementation of API for working with tail queue of strings.
 *
 *
 * Copyright (C) 2004-2022 OKTET Labs Ltd. All rights reserved.
 */

#define TE_LGR_USER     "TQ String"

#include "te_config.h"

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_ASSERT_H
#include <assert.h>
#endif

#include "te_alloc.h"
#include "tq_string.h"


/* See the description in tq_string.h */
void
tq_strings_free(tqh_strings *head, void (*value_free)(void *))
{
    tqe_string *p;

    if (head == NULL)
        return;

    while ((p = TAILQ_FIRST(head)) != NULL)
    {
        TAILQ_REMOVE(head, p, links);
        if (value_free != NULL)
            value_free(p->v);
        free(p);
    }
}

/* See the description in tq_string.h */
bool
tq_strings_equal(const tqh_strings *s1, const tqh_strings *s2)
{
    const tqe_string *p1;
    const tqe_string *p2;

    if (s1 == s2)
        return true;
    if (s1 == NULL || s2 == NULL)
        return false;

    for (p1 = TAILQ_FIRST(s1), p2 = TAILQ_FIRST(s2);
         p1 != NULL && p2 != NULL && strcmp(p1->v, p2->v) == 0;
         p1 = TAILQ_NEXT(p1, links), p2 = TAILQ_NEXT(p2, links));

    return (p1 == NULL) && (p2 == NULL);
}

/* See the description in tq_string.h */
te_errno
tq_strings_add_uniq_gen(tqh_strings *list, const char *value,
                        bool duplicate)
{
    tqe_string *p;

    assert(list != NULL);
    assert(value != NULL);

    for (p = TAILQ_FIRST(list);
         p != NULL && strcmp(value, p->v) != 0;
         p = TAILQ_NEXT(p, links));

    if (p == NULL)
    {
        p = TE_ALLOC(sizeof(*p));

        if (duplicate)
            p->v = TE_STRDUP(value);
        else
            p->v = (char *)value;

        TAILQ_INSERT_TAIL(list, p, links);

        return 0;
    }

    return 1;
}

/* See the description in tq_string.h */
te_errno
tq_strings_add_uniq(tqh_strings *list, const char *value)
{
    return tq_strings_add_uniq_gen(list, value, false);
}

/* See the description in tq_string.h */
te_errno
tq_strings_add_uniq_dup(tqh_strings *list, const char *value)
{
    return tq_strings_add_uniq_gen(list, value, true);
}

/**
 * Copy values from source queue to destination queue.
 *
 * @param dst                Destination queue.
 * @param src                Source queue.
 * @param is_shallow_copy    Flag to shallow copy.
 *
 * @return Status code.
 */
static void
tq_strings_copy_internal(tqh_strings *dst,
                         const tqh_strings *src,
                         bool is_shallow_copy)
{
    tqe_string *str;
    tqe_string *str_aux;

    TAILQ_FOREACH_SAFE(str, src, links, str_aux)
    {
        tq_strings_add_uniq_gen(dst, str->v, !is_shallow_copy);
    }
}

/* See the description in tq_string.h */
te_errno
tq_strings_move(tqh_strings *dst,
                tqh_strings *src)
{
    tqe_string *str;
    tqe_string *str_aux;

    TAILQ_FOREACH_SAFE(str, src, links, str_aux)
    {
        TAILQ_REMOVE(src, str, links);
        TAILQ_INSERT_TAIL(dst, str, links);
    }

    return 0;
}

/* See the description in tq_string.h */
te_errno
tq_strings_copy(tqh_strings *dst,
                const tqh_strings *src)
{
    tq_strings_copy_internal(dst, src, false);
    return 0;
}

/* See the description in tq_string.h */
te_errno
tq_strings_shallow_copy(tqh_strings *dst,
                        const tqh_strings *src)
{
    tq_strings_copy_internal(dst, src, true);
    return 0;
}
