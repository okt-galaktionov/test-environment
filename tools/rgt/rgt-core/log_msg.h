/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2004-2023 OKTET Labs Ltd. All rights reserved. */
/*
 * Test Environment: RGT Core
 * Common data structures and declarations.
 * Different structures that represent log message are declared.
 */

#ifndef __TE_RGT_LOG_MSG_H__
#define __TE_RGT_LOG_MSG_H__

#include "rgt_common.h"

#include <obstack.h>
#include <glib.h>

#include "logger_defs.h"

/*
 * The following declarations are about Control Log Messages that
 * outline test execution flow.
 */

#define CNTR_MSG_TEST    "TEST"
#define CNTR_MSG_PACKAGE "PACKAGE"
#define CNTR_MSG_SESSION "SESSION"

#define CNTR_MSG_TEST_JSON    "test"
#define CNTR_MSG_PACKAGE_JSON "pkg"
#define CNTR_MSG_SESSION_JSON "session"

#define CNTR_BIN2STR(val_) \
    (val_ == NT_TEST ? CNTR_MSG_TEST :            \
     val_ == NT_PACKAGE ? CNTR_MSG_PACKAGE :      \
     val_ == NT_SESSION ? CNTR_MSG_SESSION : (assert(0), ""))

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Structures that are used for representation of control log messages.
 * They are high level structures obtained from "struct log_msg" objects.
 */

/** Structure that represents session/test/package "parameter" entity */
typedef struct param {
    struct param *next; /**< Pointer to the next parameter */

    char *name; /**< Parameter name */
    char *val;  /**< Parameter value in string representation */
} param;

/** Possible results of test, package or session */
typedef enum result_status {
    RES_STATUS_PASSED,
    RES_STATUS_KILLED,
    RES_STATUS_CORED,
    RES_STATUS_SKIPPED,
    RES_STATUS_FAKED,
    RES_STATUS_FAILED,
    RES_STATUS_EMPTY,
    RES_STATUS_INCOMPLETE,
} result_status_t;

/**
 * Get string representation of result status.
 *
 * @param status      Result status.
 *
 * @return String representation.
 */
static inline const char *
result_status2str(result_status_t status)
{
#define RES_STATUS_CASE(val_) \
        case RES_STATUS_ ## val_: return #val_

    switch (status)
    {
        RES_STATUS_CASE(PASSED);
        RES_STATUS_CASE(KILLED);
        RES_STATUS_CASE(CORED);
        RES_STATUS_CASE(SKIPPED);
        RES_STATUS_CASE(FAKED);
        RES_STATUS_CASE(FAILED);
        RES_STATUS_CASE(EMPTY);
        RES_STATUS_CASE(INCOMPLETE);
    }

    return "<UNKNOWN>";
}

/** Structure for keeping session/package/test result information */
typedef struct result_info {
    enum result_status  status; /**< Result status */
    char               *err;    /**< An error message in the case of
                                     status field different from
                                     RES_STATUS_PASS */
} result_info_t;

/** Possible node types */
typedef enum node_type {
    NT_SESSION, /**< Node of session type */
    NT_PACKAGE, /**< Node of package type */
    NT_TEST,    /**< Node of test type */
    NT_BRANCH,  /**< It is used only for generation events
                     "branch start" / "branch end" */
    NT_LAST     /**< Last marker - the biggest value of the all elements */
} node_type_t;

static inline const char *
node_type2str(node_type_t node_type)
{
    switch (node_type)
    {
#define NODE_TYPE_CASE(val_) \
        case NT_ ## val_: return #val_

        NODE_TYPE_CASE(SESSION);
        NODE_TYPE_CASE(PACKAGE);
        NODE_TYPE_CASE(TEST);

#undef NODE_TYPE_CASE

        default:
            assert(0);
            return "Unknown";
    }

    assert(0);
    return "";
}


/**
 * A string used to identify per-iteration objectives,
 * generated by test control messages
 */
#define TE_TEST_OBJECTIVE_ID "<<OBJECTIVE>>"

/**
 * Test identification number for
 * prologues, epilogues, sessions, packages.
 */
#define TE_TIN_INVALID  ((unsigned int)(-1))

/** Package author. */
typedef struct rgt_author {
    /** Name. */
    const char *name;
    /** Email. */
    const char *email;
} rgt_author;

/**
 * Structure that represents information about a particular entry.
 * It is used for passing information about start/end events.
 */
typedef struct node_descr {
    char           *name;       /**< Entry name */
    char           *objective;  /**< Objectives of the entry */
    unsigned int    tin;        /**< Test identification number */
    char           *page;       /**< Name of the page with documentation */
    char           *hash;       /**< Parameters hash */
    int             n_branches; /**< Number of branches in the entry */

    /** Names and emails of authors. */
    rgt_author *authors;
    /** Number of authors. */
    unsigned int authors_num;
} node_descr_t;

typedef struct node_info {
    node_type_t     type;        /**< Node type */
    node_descr_t    descr;       /**< Description of the node */
    int             parent_id;   /**< ID of parent node */
    int             node_id;     /**< ID of this node */
    int             plan_id;     /**< ID of the next run item in
                                      the execution plan */
    param          *params;      /**< List of parameters */
    uint32_t        start_ts[2]; /**< Timestamp of a "node start" event */
    uint32_t        end_ts[2];   /**< Timestamp of a "node end" event */
    result_info_t   result;      /**< Node result info */
} node_info_t;

/** Additional data passed to callbacks processing control messages */
typedef struct ctrl_msg_data {
    msg_queue verdicts;    /**< Test verdicts */
    msg_queue artifacts;   /**< Test artifacts */

    /**
     * If @c true, indicates that at least some human readable
     * artifacts are present.
     */
    bool not_mi_artifacts;
} ctrl_msg_data;

/**
 * Type of callback function used for processing control messages
 *
 * @param node      Control node information
 * @param data      Additional data (like test verdicts)
 */
typedef int (* f_process_ctrl_log_msg)(node_info_t *node,
                                       ctrl_msg_data *data);

/* Type of callback function used for processing regular messages */
typedef int (* f_process_reg_log_msg)(log_msg *);

/* Type of callback function used for processing start and end of log. */
typedef int (* f_process_log_root)(void);

/** The set of generic control event types */
enum ctrl_event_type {
    CTRL_EVT_START,  /**< Start control message */
    CTRL_EVT_END,    /**< End control message */
    CTRL_EVT_LAST    /**< Last marker - the biggest value of the all
                          elements */
};

/** External declarations of a set of message processing functions */
extern f_process_ctrl_log_msg ctrl_msg_proc[CTRL_EVT_LAST][NT_LAST];
extern f_process_reg_log_msg  reg_msg_proc;
extern f_process_log_root     log_root_proc[CTRL_EVT_LAST];

/**
 * The list of events that can be generated from the flow tree
 * for a particular node
 */
enum event_type {
    MORE_BRANCHES /**< An additional branch is added on the entry */
};

/**
 * Process control message from Tester:
 * Insert a new node into the flow tree if it is a start event;
 * Close node if it's an end event.
 *
 * @param msg  Pointer to the log message to be processed.
 *
 * @return  Status of operation.
 *
 * @se
 *    In the case of errors it frees log message and calls longjmp.
 */
extern int rgt_process_tester_control_message(log_msg *msg);

/**
 * Process regular log message:
 *   Checks if a message passes through user-defined filters,
 *   Attaches a message to the flow tree, or calls reg_msg_proc function
 *   depending on operation mode of the rgt.
 *
 * @param msg  Pointer to the log message to be processed.
 *
 * @return  Nothing.
 *
 * @se In the case of errors it frees log message and calls longjmp.
 *
 * @todo Don't free log message but rather use it for storing the next one.
 */
extern void rgt_process_regular_message(log_msg *msg);

/**
 * Emulate a set of close messages from Tester in order to
 * correctly complete flow tree.
 *
 * @param latest_ts  Timestamp of the latest log message,
 *                   which will be used in all close messages
 */
extern void rgt_emulate_accurate_close(uint32_t *latest_ts);

/**
 * Processes event occurred on a node of the flow tree.
 * Currently the only event that is actually processed is MORE_BRANCHES.
 *
 * @param type   Type of a node on which an event has occurred.
 * @param evt    Type of an event.
 * @param node   User-specific data that is passed on creation of the node.
 *
 * @return  Nothing.
 */
extern void rgt_process_event(node_type_t type, enum event_type evt,
                              node_info_t *node);

extern void log_msg_init_arg(log_msg *msg);

/**
 * Return pointer to the log message argument. The first call of the
 * function returns pointer to the first argument. The second call
 * returns pointer to the second argument and so on.
 *
 * @param  msg  Message which argument we are going to obtain
 *
 * @return Pointer to an argument of a message
 */
extern msg_arg *get_next_arg(log_msg *msg);

/**
 * Allocates a new log_msg structure from global memory pool for log
 * messages.
 *
 * @return An address of log_msg structure.
 */
extern log_msg *alloc_log_msg(void);

/**
 * Frees log message.
 *
 * @param  msg   Log message to be freed.
 *
 * @return  Nothing.
 *
 * @se
 *     The freeing of a log message leads to freeing all messages allocated
 *     after the message.
 */
extern void free_log_msg(log_msg *msg);


/**
 * Converts format string + arguments into a formatted string.
 * The result is put into txt_msg.
 * If the latter is not NULL, the function does nothing,
 * assuming immutable fmt_str and args.
 *
 * @param msg Log message
 */
extern void rgt_expand_log_msg(log_msg *msg);

/**
 * Create log_msg_ptr structure pointing to the last log message
 * read from the raw log file.
 *
 * @param msg         Log message
 *
 * @return Pointer to log_msg_ptr structure
 */
extern log_msg_ptr *log_msg_ref(log_msg *msg);

/**
 * Allocate new log_msg structure and read its contents from raw
 * log offset specified in a given log_msg_ptr.
 *
 * @param ptr        log_msg_ptr structure containing raw log offset
 *
 * @return Pointer to log_msg structure
 */
extern log_msg *log_msg_read(log_msg_ptr *ptr);

#ifdef __cplusplus
}
#endif

#endif /* __TE_RGT_LOG_MSG_H__ */
