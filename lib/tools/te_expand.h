/* SPDX-License-Identifier: Apache-2.0 */
/** @file
 * @brief Parameters expansion API
 *
 * Definitions of the C API that allows to expand parameters in a string
 *
 * Copyright (C) 2018-2023 OKTET Labs Ltd. All rights reserved.
 */

#ifndef __TE_TOOLS_EXPAND_H__
#define __TE_TOOLS_EXPAND_H__

#include "te_config.h"
#include "te_defs.h"
#include "te_errno.h"

#ifdef TE_EXPAND_XML
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#endif

#include "te_string.h"
#include "te_kvpair.h"

/** Maximum number of positional arguments */
#define TE_EXPAND_MAX_POS_ARGS 10

/**
 * Type for parameter expanding functions for te_string_expand_parameters().
 *
 * The function is expected to append a value associated
 * with @p name to @p dest, or leave @p dest if @p name is
 * undefined.
 *
 * @param[in]  name   parameter name
 * @param[in]  ctx    parameter expansion context
 * @param[out] dest   destination string
 *
 * @return @c true if @p dest has been appended to
 *
 * @note The function is allowed to return @c true without
 *       actually modifying @p dest meaning that @p name is
 *       associated with an "explicitly empty" value.
 */
typedef bool (*te_expand_param_func)(const char *name, const void *ctx,
                                        te_string *dest);

/**
 * Expands parameters in a string.
 *
 * Parameter names are mapped to values with @p expand_param callback.
 * Everything else is appended verbatim to @p dest string.
 *
 * The parameter names must be enclosed in `${` and `}`.
 *
 * Names are not necessary simple strings, specific expanders may define
 * pretty complex syntaxes for variable names e.g. with subscripts etc
 * (see te_string_expand_kvpairs()).
 *
 * Conditional expansion is supported:
 * - `${NAME:-VALUE}` is expanded into @c VALUE if @c NAME variable is not set,
 *   otherwise to its value
 * - `${NAME:+VALUE}` is expanded into @c VALUE if @c NAME variable is set,
 *   otherwise to an empty string.
 *
 * A @c NAME may have attached a pipeline of filters separated by a pipe
 * character, e.g. `${var|base64|json}`.
 *
 * The following filters are defined:
 *
 * Name      | Meaning
 * ----------|--------------------------------------------------------------
 * base64    | Use Base-64 encoding; see te_string_encode_base64().
 * base64uri | As the above, but use the URI-safe variant of Base-64.
 * c         | Escape characters not allowed in C literals.
 * cid       | Replace all non-alphanumerics with an underscore.
 * crlf      | Replace all newlines not preceded by @c CR with @c CR-LF.
 * hex       | Encode each character as a two-digit hex value.
 * json      | Encode the value as a JSON strings, quotes are added.
 * length    | Replace the value with its length.
 * normalize | Trim trailing spaces and contract all inner spaces.
 * notempty  | A special conditional filter, see below.
 * shell     | Quote all special shell characters.
 * upper     | Convert all letters to uppercase.
 * uri       | Percent-encode all characters not allowed in URI.
 * xml       | Represent all characters disallowed in XML as XML references.
 *
 * @c notempty filter is only useful after some other filters and in
 * conjuction with `${NAME:-VALUE}`. Namely, any non-empty string is passed
 * as is, but an empty string is discarded, as if the original reference
 * did not exist, thus allowing the default value to be substituted.
 *
 * There are also the following filters that require integral input values:
 *
 * Name     | Meaning
 * ---------|-----------------------------------------------
 * even     | Pass even values as is, drop odd values.
 * nonzero  | Pass non-zero values as is.
 * odd      | Pass odd values as is, drop even values.
 * pred     | Decrement a non-zero value by one.
 * succ     | Increment a value not equal to @c -1 by one.
 *
 * These filters are intended to be used together with looping constructs.
 * Hence seemingly illogical behaviour for @c pred and @c succ: since negative
 * indexes are treated as the count from the end, @c 0 should never become
 * @c -1, nor @c -1 become @c 0.
 * See the descrition of te_string_expand_kvpairs() for examples.
 *
 * @todo Support registering additional filters.
 * @note @c base64, @c base64uri and @c hex filters can properly handle
 *       values with embedded zeroes, however, standard expanders used in
 *       te_string_expand_env_vars() and te_string_expand_kvpairs() cannot
 *       produce such values.
 *
 * Only the actual value of a variable is passed through filters; default
 * @c VALUE is not processed.
 *
 * @param[in]  src               source string
 * @param[in]  expand_param      parameter expansion function
 * @param[in]  ctx               parameter expansion context
 * @param[out] dest              destination string
 *
 * @return status code
 * @retval TE_EINVAL  Unmatched `${` found
 *
 * @note If @p src has a valid syntax, the function and its derivatives
 *       may only fail if any of filters fail. All currently implemented
 *       filters always succeed.
 */
extern te_errno te_string_expand_parameters(const char *src,
                                            te_expand_param_func expand_param,
                                            const void *ctx, te_string *dest);

/**
 * Expand environment variables in a string.
 *
 * See te_string_expand_parameters() for the expansion syntax.
 *
 * @param[in]  src               source string
 * @param[in]  posargs           positional arguments
 *                               (accesible through `${[0-9]}`, may be @c NULL)
 * @param[out] dest              destination string
 *
 * @return status code
 */
extern te_errno te_string_expand_env_vars(const char *src,
                                          const char *posargs
                                          [TE_EXPAND_MAX_POS_ARGS],
                                          te_string *dest);


/**
 * Expand key references in a string.
 *
 * See te_string_expand_parameters() for the expansion syntax.
 *
 * The expanders support multi-valued keys in @p kvpairs using
 * the following syntax for names:
 * - an empty name is expanded to the current loop index;
 * - a name staring with @c # is expanded to the count of values
 *   associated with the rest of the name;
 * - `NAME[INDEX]` is processed in the following way:
 *   + first, @c INDEX is recursively expanded
 *   + then if the result is a valid nonnegative number @c N, it is
 *     used to select the @c Nth value;
 *   + else if the result is a negative number @c -N, it is used
 *     to select the @c Nth value counting from the last, i.e.
 *     @c -1 refers to the last associated value;
 *   + otherwise the expanded index is treated as a separator and
 *     all values associated with the @c NAME are output separated
 *     by the given separator. Note that literal @c | and @c : cannot
 *     be used as separators, because they would be processed by the
 *     general syntax.
 * - `NAME*EXPR` is a loop construct. @c EXPR is recursively expanded
 *   as many times as there are values associated with @c NAME, varying
 *   the current loop index (which may be accessed with @c ${}).
 *   If there is no values associated with @c NAME, it is treated as
 *   a missing variable references, so e.g. a default value may be
 *   substituted.
 *
 * @par Examples of list references.
 * The @c // comments below mark the expected content of the destination
 * buffer.
 *
 * @code
 * te_kvpair_push(data, "ip_address", "%s", "172.16.1.1");
 * te_kvpair_push(data, "netmask", "%d", 16);
 * te_kvpair_push(data, "ip_address", "%s", "192.168.1.1");
 * te_kvpair_push(data, "netmask", "%d", 24);
 * te_kvpair_push(data, "ip_address", "%s", "127.0.0.1");
 * te_kvpair_push(data, "netmask", "%d", 32);
 * te_kvpair_push(data, "index", "%d", 1);
 *
 * te_string_expand_kvpairs("${ip_address}", NULL, data, dest);
 * // 127.0.0.1
 *
 * te_string_expand_kvpairs("${ip_address[0]}", NULL, data, dest);
 * // 127.0.0.1
 *
 * te_string_expand_kvpairs("${ip_address[1]}", NULL, data, dest);
 * // 192.168.1.1
 *
 * te_string_expand_kvpairs("${ip_address[-1]}", NULL, data, dest);
 * // 172.16.1.1
 *
 * te_string_expand_kvpairs("${ip_address[3]:-missing}", NULL, data, dest);
 * // missing
 *
 * te_string_expand_kvpairs("${ip_address[${index}]}", NULL, data, dest);
 * // 192.168.1.1
 *
 * te_string_expand_kvpairs("${ip_address[3]:-missing}", NULL, data, dest);
 * // missing
 *
 * te_string_expand_kvpairs("${ip_address[, ]}", NULL, data, dest);
 * // 127.0.0.1, 192.168.1.1, 172.16.1.1
 *
 * te_string_expand_kvpairs("${#ip_address}", NULL, data, dest);
 * // 3
 *
 * te_string_expand_kvpairs("${ip_address*address ${ip_address[${}]}/"
 *                          "${netmask[${}]}}\n}", NULL, data, dest);
 * // address 127.0.0.1/32
 * // address 192.168.1.1/24
 * // address 172.16.1.1/16
 *
 * te_string_expand_kvpairs("${ip_address*${|nonzero:+,}"
 *                          "\"${ip_address[${}]}\"}", NULL, data, dest);
 * // "127.0.0.1","192.168.1.1","172.16.1.1"
 *
 * te_string_expand_kvpairs("${ip_address*${ip_address[${|pred}]:-none} - "
 *                          "${ip_address[${|succ}]:-none}\n}",
 *                          NULL, data, dest);
 * // none - 192.168.1.1
 * // 127.0.0.1 - 172.16.1.1
 * // 192.168.1.1 - none
 * @endcode
 *
 * More examples can also be found in
 * @path{suites/selftest/ts/tool/expand_list.c}.
 *
 * @param[in]  src               source string
 * @param[in]  posargs           positional arguments
 *                               (accesible through `${[0-9]}`, may be @c NULL)
 * @param[in]  kvpairs           key-value pairs
 * @param[out] dest              destination string
 *
 * @return status code
 */
extern te_errno te_string_expand_kvpairs(const char *src,
                                         const char *posargs
                                         [TE_EXPAND_MAX_POS_ARGS],
                                         const te_kvpair_h *kvpairs,
                                         te_string *dest);

/**
 * A function type for getting a value by name from given context.
 *
 * @param name   parameter name
 * @param ctx    parameter context
 *
 * @return a value associated with @p name or @c NULL
 *
 * @deprecated This type is only used by deprecated old
 *             te_expand_parameters(). See te_expand_param_func().
 */
typedef const char *(*te_param_value_getter)(const char *name, const void *ctx);

/**
 * Expands parameters in a string.
 *
 * See te_string_expand_parameters() for the expansion syntax.
 *
 * @param[in]  src              source string
 * @param[in]  posargs          positional arguments
 *                              (accessible through `${[0-9]}`, may be @c NULL)
 * @param[in]  get_param_value  function to get param value by name
 * @param[in]  params_ctx       context for parameters
 * @param[out] retval           resulting string
 *                              (must be free()'d by the caller)
 *
 * @return status code
 * @retval TE_EINVAL  Unmatched `${` found
 *
 * @deprecated te_string_expand_parameters() should be used instead.
 */
extern te_errno te_expand_parameters(const char *src, const char **posargs,
                                     te_param_value_getter get_param_value,
                                     const void *params_ctx, char **retval);

/**
 * Expands environment variables in a string.
 *
 * See te_string_expand_parameters() for the expansion syntax.
 *
 * @param[in]  src     source string
 * @param[in]  posargs positional arguments
 *                     (accessible through `${[0-9]}`, may be @c NULL)
 * @param[out] retval  resulting string
 *                     (must be free()'d by the caller)
 *
 * @return status code
 *
 * @deprecated te_string_expand_env_vars() should be used instead.
 */
static inline te_errno
te_expand_env_vars(const char *src, const char **posargs, char **retval)
{
    te_string tmp = TE_STRING_INIT;
    te_errno rc;

    rc = te_string_expand_env_vars(src, posargs, &tmp);
    if (rc != 0)
        te_string_free(&tmp);
    else
        te_string_move(retval, &tmp);

    return rc;
}

/**
 * Expands key-value pairs in a string.
 *
 * See te_string_expand_parameters() for the expansion syntax.
 *
 * @param[in]  src     source string
 * @param[in]  posargs positional arguments
 *                     (accessible through `${[0-9]}`, may be @c NULL)
 * @param[in]  kvpairs key-value pairs
 * @param[out] retval  resulting string
 *                     (must be free()'d by the caller)
 *
 * @return status code
 *
 * @deprecated te_string_expand_kvpairs() should be used instead.
 */
static inline te_errno
te_expand_kvpairs(const char *src, const char **posargs,
                  const te_kvpair_h *kvpairs, char **retval)
{
    te_string tmp = TE_STRING_INIT;
    te_errno rc;

    rc = te_string_expand_kvpairs(src, posargs, kvpairs, &tmp);
    if (rc != 0)
        te_string_free(&tmp);
    else
        te_string_move(retval, &tmp);

    return rc;
}

#ifdef TE_EXPAND_XML
/**
 * A wrapper around xmlGetProp that expands custom parameters from
 * list of key-value pairs if given. Otherwise it expands environment variable
 * references.
 *
 * @param node          XML node
 * @param name          XML attribute name
 * @param expand_vars   List of key-value pairs for expansion in file,
 *                      @c NULL if environment variables are used for
 *                      substitutions
 *
 * @return The expanded attribute value or NULL if no attribute
 * or an error occurred while expanding.
 *
 * @sa cfg_expand_env_vars
 */
static inline char *
xmlGetProp_exp_vars_or_env(xmlNodePtr node, const xmlChar *name,
                           const te_kvpair_h *kvpairs)
{
    xmlChar *value = xmlGetProp(node, name);

    if (value)
    {
        char *result = NULL;
        te_errno rc;

        if (kvpairs == NULL)
            rc = te_expand_env_vars((const char *)value, NULL, &result);
        else
            rc = te_expand_kvpairs((const char *)value, NULL, kvpairs, &result);

        if (rc == 0)
        {
            xmlFree(value);
            value = (xmlChar *)result;
        }
        else
        {
            ERROR("Error substituting variables in %s '%s': %r",
                  name, value, rc);
            xmlFree(value);
            value = NULL;
        }
    }
    return (char *)value;
}

/**
 * Case of xmlGetProp_exp_vars_or_env that expands only environment
 * variables reference.
 *
 * @param node    XML node
 * @param name    XML attribute name
 *
 * @return The expanded attribute value or NULL if no attribute
 * or an error occurred while expanding.
 *
 * @sa cfg_expand_env_vars
 */
static inline char *
xmlGetProp_exp(xmlNodePtr node, const xmlChar *name)
{
    return xmlGetProp_exp_vars_or_env(node, name, NULL);
}
#endif /* TE_EXPAND_XML */



#endif /* !__TE_TOOLS_EXPAND_H__ */
