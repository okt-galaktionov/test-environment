/* SPDX-License-Identifier: Apache-2.0 */
/** @file
 * @brief API to modify TRC tags from prologues
 *
 * Implementation of API to modify TRC tags from prologues.
 *
 * Copyright (C) 2022-2022 OKTET Labs Ltd. All rights reserved.
 */

#define TE_LGR_USER "TAPI Tags"

#include "te_config.h"

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "te_errno.h"
#include "conf_api.h"
#include "tapi_test.h"
#include "tapi_cfg_pci.h"

/* See the description from tapi_tags.h */
te_errno
tapi_tags_add_tag(const char *tag, const char *value)
{
    te_errno rc;

    if (tag == NULL)
        return TE_RC(TE_TAPI, TE_EINVAL);

    if (strcspn(tag, "/:") < strlen(tag))
    {
        ERROR("TRC tag name contains invalid characters");
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    /*
     * The check does not guarantee that it is root prologue, but it should
     * filter out almost all misuses.
     */
    if (te_test_id != TE_TEST_ID_ROOT_PROLOGUE)
    {
        ERROR("The root prologue only may modify TRC tags: %d", te_test_id);
        return TE_RC(TE_TAPI, TE_EPERM);
    }

    if (value == NULL)
        value = "";

    rc = cfg_add_instance_fmt(NULL, CFG_VAL(STRING, value), TE_CFG_TRC_TAGS_FMT,
                              tag);
    if (rc != 0)
    {
        ERROR("%s(): cfg_add_instance_fmt(" TE_CFG_TRC_TAGS_FMT ") failed: %r",
              __FUNCTION__, tag, rc);
    }
    return rc;
}

/* See the description from tapi_tags.h */
te_errno
tapi_tags_add_net_pci_tags(const char *ta, const char *if_name)
{
    char *pci_oid = NULL;
    unsigned int vendor_id = 0;
    unsigned int device_id = 0;
    unsigned int sub_vendor_id = 0;
    unsigned int sub_device_id = 0;
    te_string str = TE_STRING_INIT;
    te_errno rc;

    rc = tapi_cfg_pci_oid_by_net_if(ta, if_name, &pci_oid);
    if (TE_RC_GET_ERROR(rc) == TE_ENOENT)
        return 0;
    else if (rc != 0)
        return rc;

    rc = tapi_cfg_pci_get_vendor_dev_ids(pci_oid,
            &vendor_id, &device_id, &sub_vendor_id, &sub_device_id);
    if (rc != 0)
        goto done;

    te_string_append(&str, "pci-%04x", vendor_id);
    rc = tapi_tags_add_tag(te_string_value(&str), NULL);
    if (rc != 0)
        goto done;

    te_string_append(&str, "-%04x", device_id);
    rc = tapi_tags_add_tag(te_string_value(&str), NULL);
    if (rc != 0)
        goto done;

    te_string_reset(&str);
    te_string_append(&str, "pci-sub-%04x", sub_vendor_id);
    rc = tapi_tags_add_tag(te_string_value(&str), NULL);
    if (rc != 0)
        goto done;

    te_string_append(&str, "-%04x", sub_device_id);
    rc = tapi_tags_add_tag(te_string_value(&str), NULL);
    if (rc != 0)
        goto done;

done:
    te_string_free(&str);
    free(pci_oid);

    return rc;
}
