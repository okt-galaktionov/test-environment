/* SPDX-License-Identifier: Apache-2.0 */
/** @file
 * @brief TAPI to manage memcached
 *
 * TAPI to manage memcached.
 *
 * Copyright (C) 2022-2022 OKTET Labs Ltd. All rights reserved.
 */

#define TE_LGR_USER "TAPI MEMCACHED"

#include <signal.h>
#include <netinet/in.h>

#include "tapi_memcached.h"
#include "tapi_sockaddr.h"
#include "tapi_job_opt.h"
#include "te_alloc.h"
#include "te_enum.h"

#define TAPI_MEMCACHED_TIMEOUT_MS 10000

/**
 * Path to memcached exec in the case of
 * tapi_memcached_opt::memcached_path is @c NULL.
 */
static const char *memcached_path = "memcached";

/* See description in tapi_memcached.h */
const struct sockaddr_in zero_sockaddr = {
                                            .sin_family = AF_INET,
                                            .sin_addr   = { 0 },
                                            .sin_port   = 0
                                         };

/** Mapping of possible values for memcached::protocol option. */
static const te_enum_map tapi_mamcached_proto_mapping[] = {
    {.name = "auto",    .value = TAPI_MEMCACHED_PROTO_AUTO},
    {.name = "ascii",   .value = TAPI_MEMCACHED_PROTO_ASCII},
    {.name = "binary",  .value = TAPI_MEMCACHED_PROTO_BINARY},
    TE_ENUM_MAP_END
};

/** Mapping of possible values for memcached::verbose option. */
static const te_enum_map tapi_mamcached_verbose_mapping[] = {
    {.name = "-v",      .value = TAPI_MEMCACHED_VERBOSE},
    {.name = "-vv",     .value = TAPI_MEMCACHED_MORE_VERBOSE},
    {.name = "-vvv",    .value = TAPI_MEMCACHED_EXTRA_VERBOSE},
    TE_ENUM_MAP_END
};

/* Possible memcached command line arguments. */
static const tapi_job_opt_bind memcached_binds[] = TAPI_JOB_OPT_SET(
    TAPI_JOB_OPT_STRING("--unix-socket=", true, tapi_memcached_opt,
                        unix_socket),
    TAPI_JOB_OPT_BOOL("--enable-shutdown", tapi_memcached_opt,
                      enable_ascii_shutdown),
    TAPI_JOB_OPT_UINT_T_OCTAL("--unix-mask=", true, NULL, tapi_memcached_opt,
                              unix_mask),
    TAPI_JOB_OPT_SOCKADDR_PTR("--listen=", true, tapi_memcached_opt,
                              listen_ipaddr),
    TAPI_JOB_OPT_STRING("--user=", true, tapi_memcached_opt, username),
    TAPI_JOB_OPT_UINT_T("--memory-limit=", true, NULL, tapi_memcached_opt,
                        memory_limit),
    TAPI_JOB_OPT_UINT_T("--conn-limit=", true, NULL, tapi_memcached_opt,
                        conn_limit),
    TAPI_JOB_OPT_UINT_T("--max-reqs-per-event=", true, NULL, tapi_memcached_opt,
                        max_reqs_per_event),
    TAPI_JOB_OPT_BOOL("--lock-memory", tapi_memcached_opt, lock_memory),
    TAPI_JOB_OPT_SOCKPORT_PTR("--port=", true, tapi_memcached_opt, tcp_port),
    TAPI_JOB_OPT_SOCKPORT_PTR("--udp-port=", true, tapi_memcached_opt,
                              udp_port),
    TAPI_JOB_OPT_BOOL("--disable-evictions", tapi_memcached_opt,
                      disable_evictions),
    TAPI_JOB_OPT_BOOL("--enable-coredumps", tapi_memcached_opt,
                      enable_coredumps),
    TAPI_JOB_OPT_DOUBLE("--slab-growth-factor=", true, NULL, tapi_memcached_opt,
                        slab_growth_factor),
    TAPI_JOB_OPT_UINT_T("--slab-min-size=", true, NULL, tapi_memcached_opt,
                        slab_min_size),
    TAPI_JOB_OPT_BOOL("--disable-cas", tapi_memcached_opt, disable_cas),
    TAPI_JOB_OPT_ENUM(NULL, false, tapi_memcached_opt, verbose,
                      tapi_mamcached_verbose_mapping),
    TAPI_JOB_OPT_UINT_T("--threads=", true, NULL, tapi_memcached_opt,
                        threads),
    TAPI_JOB_OPT_UINT_T("--napi-ids=", true, NULL, tapi_memcached_opt,
                        napi_ids),
    TAPI_JOB_OPT_STRING("-D", false, tapi_memcached_opt, delimiter),
    TAPI_JOB_OPT_BOOL("--enable-largepages", tapi_memcached_opt,
                      enable_largepages),
    TAPI_JOB_OPT_UINT_T("--listen-backlog=", true, NULL, tapi_memcached_opt,
                        listen_backlog),
    TAPI_JOB_OPT_ENUM("--protocol=", true, tapi_memcached_opt, protocol,
                      tapi_mamcached_proto_mapping),
    TAPI_JOB_OPT_UINT_T("--max-item-size=", true, "k", tapi_memcached_opt,
                        max_item_size),
    TAPI_JOB_OPT_BOOL("--enable-sasl", tapi_memcached_opt,
                      enable_sasl),
    TAPI_JOB_OPT_BOOL("--disable-flush-all", tapi_memcached_opt,
                      disable_flush_all),
    TAPI_JOB_OPT_BOOL("--disable-dumping", tapi_memcached_opt,
                      disable_dumping),
    TAPI_JOB_OPT_BOOL("--disable-watch", tapi_memcached_opt,
                      disable_watch),
    TAPI_JOB_OPT_BOOL("-omaxconns_fast", tapi_memcached_opt,
                      maxconns_fast),
    TAPI_JOB_OPT_BOOL("-ono_maxconns_fast", tapi_memcached_opt,
                      no_maxconns_fast),
    TAPI_JOB_OPT_UINT_T("-ohashpower=", true, NULL, tapi_memcached_opt,
                        hashpower),
    TAPI_JOB_OPT_UINT_T("-otail_repair_time=", true, NULL, tapi_memcached_opt,
                        tail_repair_time),
    TAPI_JOB_OPT_BOOL("-ono_lru_crawler", tapi_memcached_opt, no_lru_crawler),
    TAPI_JOB_OPT_UINT_T("-olru_crawler_sleep=", true, NULL, tapi_memcached_opt,
                        lru_crawler_sleep),
    TAPI_JOB_OPT_UINT_T("-olru_crawler_tocrawl=", true, NULL,
                        tapi_memcached_opt, lru_crawler_tocrawl),
    TAPI_JOB_OPT_BOOL("-ono_lru_maintainer", tapi_memcached_opt,
                      no_lru_maintainer),
    TAPI_JOB_OPT_UINT_T("-ohot_lru_pct=", true, NULL, tapi_memcached_opt,
                        hot_lru_pct),
    TAPI_JOB_OPT_UINT_T("-owarm_lru_pct=", true, NULL, tapi_memcached_opt,
                        warm_lru_pct),
    TAPI_JOB_OPT_DOUBLE("-ohot_max_factor=", true, NULL, tapi_memcached_opt,
                        hot_max_factor),
    TAPI_JOB_OPT_DOUBLE("-owarm_max_factor=", true, NULL, tapi_memcached_opt,
                        warm_max_factor),
    TAPI_JOB_OPT_UINT_T("-otemporary_ttl=", true, NULL, tapi_memcached_opt,
                        temporary_ttl),
    TAPI_JOB_OPT_UINT_T("-oidle_timeout=", true, NULL, tapi_memcached_opt,
                        idle_timeout),
    TAPI_JOB_OPT_UINT_T("-owatcher_logbuf_size=", true, NULL, tapi_memcached_opt,
                        watcher_logbuf_size),
    TAPI_JOB_OPT_UINT_T("-oworker_logbuf_size=", true, NULL, tapi_memcached_opt,
                        worker_logbuf_size),
    TAPI_JOB_OPT_BOOL("-otrack_sizes", tapi_memcached_opt, track_sizes),
    TAPI_JOB_OPT_BOOL("-ono_hashexpand", tapi_memcached_opt, no_hashexpand),
    TAPI_JOB_OPT_UINT_T("-oext_page_size=", true, NULL, tapi_memcached_opt,
                        ext_page_size),
    TAPI_JOB_OPT_STRUCT("-oext_path=", true, ":", NULL,
                        TAPI_JOB_OPT_STRING(NULL, false, tapi_memcached_opt,
                                            ext_path.path),
                        TAPI_JOB_OPT_UINT_T(NULL, false, "G", tapi_memcached_opt,
                                            ext_path.size)),
    TAPI_JOB_OPT_UINT_T("-oext_wbuf_size=", true, NULL, tapi_memcached_opt,
                        ext_wbuf_size),
    TAPI_JOB_OPT_UINT_T("-oext_threads=", true, NULL, tapi_memcached_opt,
                        ext_threads),
    TAPI_JOB_OPT_UINT_T("-oext_item_size=", true, NULL, tapi_memcached_opt,
                        ext_item_size),
    TAPI_JOB_OPT_UINT_T("-oext_item_age=", true, NULL, tapi_memcached_opt,
                        ext_item_age),
    TAPI_JOB_OPT_UINT_T("-oext_low_ttl=", true, NULL, tapi_memcached_opt,
                        ext_low_ttl),
    TAPI_JOB_OPT_BOOL("-oext_drop_unread", tapi_memcached_opt, ext_drop_unread),
    TAPI_JOB_OPT_UINT_T("-oext_recache_rate=", true, NULL, tapi_memcached_opt,
                        ext_recache_rate),
    TAPI_JOB_OPT_UINT_T("-oext_compact_under=", true, NULL, tapi_memcached_opt,
                        ext_compact_under),
    TAPI_JOB_OPT_UINT_T("-oext_drop_under=", true, NULL, tapi_memcached_opt,
                        ext_drop_under),
    TAPI_JOB_OPT_DOUBLE("-oext_max_frag=", true, NULL, tapi_memcached_opt,
                        ext_max_frag),
    TAPI_JOB_OPT_DOUBLE("-oslab_automove_freeratio=", true, NULL,
                        tapi_memcached_opt, slab_automove_freeratio)
);

/* Default values of memcached command line arguments. */
const tapi_memcached_opt tapi_memcached_default_opt = {
    .unix_socket                = NULL,
    .enable_ascii_shutdown      = false,
    .unix_mask                  = TAPI_JOB_OPT_UINT_UNDEF,
    .listen_ipaddr              = NULL,
    .username                   = NULL,
    .memory_limit               = TAPI_JOB_OPT_UINT_UNDEF,
    .conn_limit                 = TAPI_JOB_OPT_UINT_UNDEF,
    .max_reqs_per_event         = TAPI_JOB_OPT_UINT_UNDEF,
    .lock_memory                = false,
    .tcp_port                   = (const struct sockaddr *) &zero_sockaddr,
    .udp_port                   = (const struct sockaddr *) &zero_sockaddr,
    .disable_evictions          = false,
    .enable_coredumps           = false,
    .slab_growth_factor         = TAPI_JOB_OPT_DOUBLE_UNDEF,
    .slab_min_size              = TAPI_JOB_OPT_UINT_UNDEF,
    .disable_cas                = false,
    .verbose                    = TAPI_MEMCACHED_NOT_VERBOSE,
    .threads                    = TAPI_JOB_OPT_UINT_UNDEF,
    .napi_ids                   = TAPI_JOB_OPT_UINT_UNDEF,
    .delimiter                  = NULL,
    .enable_largepages          = false,
    .listen_backlog             = TAPI_JOB_OPT_UINT_UNDEF,
    .protocol                   = TAPI_MEMCACHED_PROTO_AUTO,
    .max_item_size              = TAPI_JOB_OPT_UINT_UNDEF,
    .enable_sasl                = false,
    .disable_flush_all          = false,
    .disable_dumping            = false,
    .disable_watch              = false,
    .maxconns_fast              = false,
    .no_maxconns_fast           = false,
    .hashpower                  = TAPI_JOB_OPT_UINT_UNDEF,
    .tail_repair_time           = TAPI_JOB_OPT_UINT_UNDEF,
    .no_lru_crawler             = false,
    .lru_crawler_sleep          = TAPI_JOB_OPT_UINT_UNDEF,
    .lru_crawler_tocrawl        = TAPI_JOB_OPT_UINT_UNDEF,
    .no_lru_maintainer          = false,
    .hot_lru_pct                = TAPI_JOB_OPT_UINT_UNDEF,
    .warm_lru_pct               = TAPI_JOB_OPT_UINT_UNDEF,
    .hot_max_factor             = TAPI_JOB_OPT_DOUBLE_UNDEF,
    .warm_max_factor            = TAPI_JOB_OPT_DOUBLE_UNDEF,
    .temporary_ttl              = TAPI_JOB_OPT_UINT_UNDEF,
    .idle_timeout               = TAPI_JOB_OPT_UINT_UNDEF,
    .watcher_logbuf_size        = TAPI_JOB_OPT_UINT_UNDEF,
    .worker_logbuf_size         = TAPI_JOB_OPT_UINT_UNDEF,
    .track_sizes                = false,
    .no_hashexpand              = false,
    .ext_path                   = {
                                    .path = NULL,
                                    .size = TAPI_JOB_OPT_UINT_UNDEF
                                  },
    .ext_page_size              = TAPI_JOB_OPT_UINT_UNDEF,
    .ext_wbuf_size              = TAPI_JOB_OPT_UINT_UNDEF,
    .ext_threads                = TAPI_JOB_OPT_UINT_UNDEF,
    .ext_item_size              = TAPI_JOB_OPT_UINT_UNDEF,
    .ext_item_age               = TAPI_JOB_OPT_UINT_UNDEF,
    .ext_low_ttl                = TAPI_JOB_OPT_UINT_UNDEF,
    .ext_drop_unread            = false,
    .ext_recache_rate           = TAPI_JOB_OPT_UINT_UNDEF,
    .ext_compact_under          = TAPI_JOB_OPT_UINT_UNDEF,
    .ext_drop_under             = TAPI_JOB_OPT_UINT_UNDEF,
    .ext_max_frag               = TAPI_JOB_OPT_DOUBLE_UNDEF,
    .slab_automove_freeratio    = TAPI_JOB_OPT_DOUBLE_UNDEF,
    .memcached_path             = NULL
};

/* See description in tapi_memcached.h */
te_errno
tapi_memcached_create(tapi_job_factory_t *factory,
                      const tapi_memcached_opt *opt,
                      tapi_memcached_app **app)
{
    te_errno            rc;
    te_vec              args = TE_VEC_INIT(char *);
    tapi_memcached_app *new_app;
    const char         *exec_path = memcached_path;

    if (factory == NULL)
    {
        rc = TE_EFAULT;
        ERROR("Memcached factory to create job can't be NULL: %r", rc);
        return TE_RC(TE_TAPI, rc);
    }

    if (opt == NULL)
    {
        rc = TE_EFAULT;
        ERROR("Memcached opt to create job can't be NULL: %r", rc);
        return TE_RC(TE_TAPI, rc);
    }

    if (app == NULL)
    {
        rc = TE_EFAULT;
        ERROR("Memcached app to create job can't be NULL: %r", rc);
        return TE_RC(TE_TAPI, rc);
    }

    if (opt->tcp_port == NULL)
    {
        rc = TE_EINVAL;
        ERROR("Failed to create memcached app without TCP port: %r", rc);
        return TE_RC(TE_TAPI, rc);
    }

    new_app = TE_ALLOC(sizeof(*new_app));

    if (opt->memcached_path != NULL)
        exec_path = opt->memcached_path;

    rc = tapi_job_opt_build_args(exec_path, memcached_binds,
                                 opt, &args);
    if (rc != 0)
    {
        ERROR("Failed to build memcached job arguments: %r", rc);
        te_vec_deep_free(&args);
        free(new_app);
        return rc;
    }

    rc = tapi_job_simple_create(factory,
                        &(tapi_job_simple_desc_t){
                           .program    = exec_path,
                           .argv       = (const char **)args.data.ptr,
                           .job_loc    = &new_app->job,
                           .stdout_loc = &new_app->out_chs[0],
                           .stderr_loc = &new_app->out_chs[1],
                           .filters    = TAPI_JOB_SIMPLE_FILTERS(
                               {
                                   .use_stdout  = true,
                                   .readable    = false,
                                   .log_level   = TE_LL_RING,
                                   .filter_name = "memcached stdout"
                               },
                               {
                                   .use_stderr  = true,
                                   .readable    = false,
                                   .log_level   = TE_LL_WARN,
                                   .filter_name = "memcached stderr"
                               }
                           )
                        });
    if (rc != 0)
    {
        ERROR("Failed to create %s job: %r", exec_path, rc);
        te_vec_deep_free(&args);
        free(new_app);
        return rc;
    }

    *app = new_app;
    te_vec_deep_free(&args);
    return 0;
}

/* See description in tapi_memcached.h */
te_errno
tapi_memcached_start(const tapi_memcached_app *app)
{
    te_errno rc;

    if (app == NULL)
    {
        rc = TE_EFAULT;
        ERROR("Memcached app to start job can't be NULL: %r", rc);
        return TE_RC(TE_TAPI, rc);
    }

    return tapi_job_start(app->job);
}

/* See description in tapi_memcached.h */
te_errno
tapi_memcached_wait(const tapi_memcached_app *app, int timeout_ms)
{
    te_errno          rc;
    tapi_job_status_t status;

    if (app == NULL)
    {
        rc = TE_EFAULT;
        ERROR("Memcached app to wait job can't be NULL: %r", rc);
        return TE_RC(TE_TAPI, rc);
    }

    rc = tapi_job_wait(app->job, timeout_ms, &status);
    if (rc != 0)
    {
        if (TE_RC_GET_ERROR(rc) == TE_EINPROGRESS)
            RING("Job was still in process at the end of the wait");

        return rc;
    }

    TAPI_JOB_CHECK_STATUS(status);
    return 0;
}

/* See description in tapi_memcached.h */
te_errno
tapi_memcached_stop(const tapi_memcached_app *app)
{
    te_errno rc;

    if (app == NULL)
    {
        rc = TE_EFAULT;
        ERROR("Memcached app to stop job can't be NULL: %r", rc);
        return TE_RC(TE_TAPI, rc);
    }

    return tapi_job_stop(app->job, SIGTERM, TAPI_MEMCACHED_TIMEOUT_MS);
}

/* See description in tapi_memcached.h */
te_errno
tapi_memcached_kill(const tapi_memcached_app *app, int signum)
{
    te_errno rc;

    if (app == NULL)
    {
        rc = TE_EFAULT;
        ERROR("Memcached app to kill job can't be NULL: %r", rc);
        return TE_RC(TE_TAPI, rc);
    }

    return tapi_job_kill(app->job, signum);
}

/* See description in tapi_memcached.h */
te_errno
tapi_memcached_destroy(tapi_memcached_app *app)
{
    te_errno rc;

    if (app == NULL)
        return 0;

    rc = tapi_job_destroy(app->job, TAPI_MEMCACHED_TIMEOUT_MS);
    if (rc != 0)
    {
        ERROR("Failed to destroy memcached job: %r", rc);
        return rc;
    }

    free(app);
    return 0;
}
