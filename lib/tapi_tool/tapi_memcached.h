/* SPDX-License-Identifier: Apache-2.0 */
/** @file
 * @brief TAPI to manage memcached
 *
 * @defgroup tapi_memcached TAPI to manage memcached
 * @ingroup te_ts_tapi
 * @{
 *
 * TAPI to manage *memcached*.
 *
 * Copyright (C) 2022-2022 OKTET Labs Ltd. All rights reserved.
 */

#ifndef __TE_TAPI_MEMCACHED_H__
#define __TE_TAPI_MEMCACHED_H__

#include <sys/socket.h>

#include "te_errno.h"
#include "tapi_job_opt.h"
#include "tapi_job.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Constant for sockaddr "0.0.0.0:0" initialization.
 *
 * @note Can be used when there is a need to use localhost or zero port.
 */
extern const struct sockaddr_in zero_sockaddr;

/** memcached tool information. */
typedef struct tapi_memcached_app {
    /** TAPI job handle. */
    tapi_job_t *job;
    /** Output channel handles. */
    tapi_job_channel_t *out_chs[2];
} tapi_memcached_app;

/** Representation of possible values for memcached::protocol option. */
typedef enum tapi_mamcached_proto {
    TAPI_MEMCACHED_PROTO_AUTO,
    TAPI_MEMCACHED_PROTO_ASCII,
    TAPI_MEMCACHED_PROTO_BINARY,
} tapi_mamcached_proto_t;

/** Representation of possible values for memcached::verbose option. */
typedef enum tapi_mamcached_verbose {
    /** Option is omitted */
    TAPI_MEMCACHED_NOT_VERBOSE = TAPI_JOB_OPT_ENUM_UNDEF,

    TAPI_MEMCACHED_VERBOSE,
    TAPI_MEMCACHED_MORE_VERBOSE,
    TAPI_MEMCACHED_EXTRA_VERBOSE,
} tapi_mamcached_verbose_t;

/** Specific memcached options. */
typedef struct tapi_memcached_opt {
    /** Unix socket path to listen on (disables network support). */
    const char                         *unix_socket;
    /** Enable ascii "shutdown" command. */
    bool enable_ascii_shutdown;
    /** Permissions (in octal form) for Unix socket created with -s option. */
    tapi_job_opt_uint_t                 unix_mask;
    /** Listen on @c ip_addr. */
    const struct sockaddr              *listen_ipaddr;
    /** Assume the identity of @c username. */
    const char                         *username;
    /** Memory usage in MB. */
    tapi_job_opt_uint_t                 memory_limit;
    /** Max simultaneous connections. */
    tapi_job_opt_uint_t                 conn_limit;
    /**
     * Once a connection exceeds this number of consecutive requests,
     * the server will try to process I/O on other connections before
     * processing any further requests from that connection.
     */
    tapi_job_opt_uint_t                 max_reqs_per_event;
    /**
     * Lock down all paged memory.
     * This is a somewhat dangerous option with large caches.
     */
    bool lock_memory;
    /**
     * TCP port to listen on (0 by default, 0 to turn off).
     *
     * @note To set 0 use @c zero_sockaddr.
     */
    const struct sockaddr              *tcp_port;
    /**
     * UDP port to listen on (0 by default, 0 to turn off).
     *
     * @note To set 0 use @c zero_sockaddr.
    */
    const struct sockaddr              *udp_port;
    /**
     * Disable automatic removal of items from the cache when out of memory.
     * Additions will not be possible until adequate space is freed up.
     */
    bool disable_evictions;
    /** Raise the core file size limit to the maximum allowable. */
    bool enable_coredumps;
    /**
     * A lower value may result in less wasted memory depending on the total
     * amount of memory available and the distribution of item sizes.
     */
    tapi_job_opt_double_t               slab_growth_factor;
    /**
     * Allocate a minimum of @c size bytes for the item key, value,
     * and flags.
     */
    tapi_job_opt_uint_t                 slab_min_size;
    /** Disable the use of CAS (and reduce the per-item size by 8 bytes). */
    bool disable_cas;
    /**
     * Be verbose during the event loop.
     * Print out errors and warnings (none by default).
     */
    tapi_mamcached_verbose_t            verbose;
    /** Number of threads to use to process incoming requests. */
    tapi_job_opt_uint_t                 threads;
    /** Number of NAPI ids (see napi_ids.txt in memcached docs for details) */
    tapi_job_opt_uint_t                 napi_ids;
    /**
     * One char delimiter between key prefixes and IDs.
     * This is used for per-prefix stats reporting.
     */
    const char                         *delimiter;
    /**
     * Try to use large memory pages (if available).
     * Increasing the memory page size could reduce the number of TLB misses
     * and improve the performance.
     */
    bool enable_largepages;
    /** Set the backlog queue limit to number of connections. */
    tapi_job_opt_uint_t                 listen_backlog;
    /** Specify the binding protocol to use ("auto" by default). */
    tapi_mamcached_proto_t              protocol;
    /** Override the default size of each slab page in Kilobytes. */
    tapi_job_opt_uint_t                 max_item_size;
    /**
     * Turn on SASL authentication. This option is only meaningful
     * if memcached was compiled with SASL support enabled.
     */
    bool enable_sasl;
    /**
     * Disable the "flush_all" command. The cmd_flush counter will
     * increment, but clients will receive an error message and the
     * flush will not occur.
     */
    bool disable_flush_all;
    /** Disable the "stats cachedump" and "lru_crawler metadump" commands. */
    bool disable_dumping;
    /** Disable watch commands (live logging). */
    bool disable_watch;
    /**
     * @name Extended options
     * @{
     */
    /** Immediately close new connections after limit. */
    bool maxconns_fast;
    /** Cancel maxconns_fast option. */
    bool no_maxconns_fast;
    /**
     * An integer multiplier for how large the hash
     * table should be. Normally grows at runtime.
     * Set based on "STAT hash_power_level".
     */
    tapi_job_opt_uint_t                 hashpower;
    /**
     * Time in seconds for how long to wait before
     * forcefully killing LRU tail item.
     * Very dangerous option!
     */
    tapi_job_opt_uint_t                 tail_repair_time;
    /** Disable LRU Crawler background thread. */
    bool no_lru_crawler;
    /** Microseconds to sleep between items. */
    tapi_job_opt_uint_t                 lru_crawler_sleep;
    /** Max items to crawl per slab per run (if 0 then unlimited). */
    tapi_job_opt_uint_t                 lru_crawler_tocrawl;
    /**  Disable new LRU system + background thread. */
    bool no_lru_maintainer;
    /** pct of slab memory to reserve for hot lru. Requires lru_maintainer. */
    tapi_job_opt_uint_t                 hot_lru_pct;
    /** pct of slab memory to reserve for warm lru. Requires lru_maintainer. */
    tapi_job_opt_uint_t                 warm_lru_pct;
    /** Items idle > cold lru age * drop from hot lru. */
    tapi_job_opt_double_t               hot_max_factor;
    /** Items idle > cold lru age * this drop from warm. */
    tapi_job_opt_double_t               warm_max_factor;
    /**
     * TTL's below get separate LRU, can't be evicted.
     * Requires lru_maintainer.
     */
    tapi_job_opt_uint_t                 temporary_ttl;
    /** Timeout for idle connections (if 0 then no timeout). */
    tapi_job_opt_uint_t                 idle_timeout;
    /** Size in kilobytes of per-watcher write buffer. */
    tapi_job_opt_uint_t                 watcher_logbuf_size;
    /**
     * Size in kilobytes of per-worker-thread buffer
     * read by background thread, then written to watchers.
     */
    tapi_job_opt_uint_t                 worker_logbuf_size;
    /** Enable dynamic reports for 'stats sizes' command. */
    bool track_sizes;
    /** Disables hash table expansion. Dangerous! */
    bool no_hashexpand;

    /**
     * @name External storage options
     * @{
     */
    /**
     * File to write to for external storage.
     *
     * Example: @c "ext_path=/mnt/d1/extstore:1G".
     * This initializes extstore with up to 5 gigabytes of storage.
     * Storage is split internally into pages size of @p ext_page_size.
     */
    struct
    {
        const char *path;
        tapi_job_opt_uint_t size;
    } ext_path;
    /** Size of storage pages, in megabytes. */
    tapi_job_opt_uint_t                 ext_page_size;
    /** Size of page write buffers, in megabytes. */
    tapi_job_opt_uint_t                 ext_wbuf_size;
    /**
     * Number of IO threads to run.
     *
     * If you have a high read latency but the drive is idle,
     * you can increase this number.
     * Stick to low values; no more than 8 threads.
     */
    tapi_job_opt_uint_t                 ext_threads;
    /**
     * Store items larger than this, in bytes.
     *
     * Items larger than this can be flushed.
     * You can lower this value if you want to save a little extra RAM
     * and your keys are short. You can also raise this value if you
     * only wish to flush very large objects, which is a good place to start.
     */
    tapi_job_opt_uint_t                 ext_item_size;
    /**
     * Store items idle at least this long, in seconds.
     * If not used then no age limit.
     */
    tapi_job_opt_uint_t                 ext_item_age;
    /** Consider TTLs lower than this specially. */
    tapi_job_opt_uint_t                 ext_low_ttl;
    /** Don't re-write unread values during compaction. */
    bool ext_drop_unread;
    /**
     * Recache an item every N accesses.
     *
     * If an item stored on flash has been accessed more than once in the last
     * minute, it has a one in N chance of being recached into RAM and removed
     * from flash. It's good to keep this value high; recaches into RAM cause
     * fragmentation on disk, and it's rare for objects in flash to become
     * frequently accessed. If they do, they will eventually be recached.
     */
    tapi_job_opt_uint_t                 ext_recache_rate;
    /** Compact when fewer than this many free pages. */
    tapi_job_opt_uint_t                 ext_compact_under;
    /** Drop COLD items when fewer than this many free pages. */
    tapi_job_opt_uint_t                 ext_drop_under;
    /**
     * Max page fragmentation to tolerate.
     *
     * Example: "ext_max_frag=0.5"
     * This will rewrite pages which are at least half empty.
     * If no pages are half empty, the oldest page will be evicted.
     */
    tapi_job_opt_double_t               ext_max_frag;
    /** Ratio of memory to hold free as buffer. */
    tapi_job_opt_double_t               slab_automove_freeratio;
    /** @} */
    /** @} */
    /** Path to memcached exec (if @c NULL then "memcached"). */
    const char                         *memcached_path;
} tapi_memcached_opt;

/** Default memcached options initializer. */
extern const tapi_memcached_opt tapi_memcached_default_opt;

/**
 * Create memcached app.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  opt          memcached options.
 * @param[out] app          memcached app handle.
 *
 * @return Status code.
 */
extern te_errno tapi_memcached_create(tapi_job_factory_t *factory,
                                      const tapi_memcached_opt *opt,
                                      tapi_memcached_app **app);

/**
 * Start memcached.
 *
 * @param[in]  app          memcached app handle.
 *
 * @return Status code.
 */
extern te_errno tapi_memcached_start(const tapi_memcached_app *app);

/**
 * Wait for memcached completion.
 *
 * @param[in]  app          memcached app handle.
 * @param[in]  timeout_ms   Wait timeout in milliseconds.
 *
 * @return Status code.
 */
extern te_errno tapi_memcached_wait(const tapi_memcached_app *app,
                                    int timeout_ms);

/**
 * Stop memcached. It can be started over with tapi_memcached_start().
 *
 * @param[in]  app          memcached app handle.
 *
 * @return Status code.
 */
extern te_errno tapi_memcached_stop(const tapi_memcached_app *app);

/**
 * Send a signal to memcached.
 *
 * @param[in]  app          memcached app handle.
 * @param[in]  signum       Signal to send.
 *
 * @return Status code.
 */
extern te_errno tapi_memcached_kill(const tapi_memcached_app *app,
                                    int signum);

/**
 * Destroy memcached.
 *
 * @param[in]  app          memcached app handle.
 *
 * @return Status code.
 */
extern te_errno tapi_memcached_destroy(tapi_memcached_app *app);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TE_TAPI_MEMCACHED_H__ */
/**@} <!-- END tapi_memcached --> */
