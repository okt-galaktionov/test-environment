/* SPDX-License-Identifier: Apache-2.0 */
/** @file
 * @brief PCI Configuration Model TAPI
 *
 * Implementation of test API for network configuration model
 * (doc/cm/cm_pci).
 *
 * Copyright (C) 2020-2022 OKTET Labs Ltd. All rights reserved.
 */

#define TE_LGR_USER      "Configuration TAPI"

#include "te_config.h"

#include "te_defs.h"
#include "te_errno.h"
#include "te_alloc.h"
#include "te_str.h"
#include "rcf_api.h"
#include "logger_api.h"
#include "conf_api.h"

#include "tapi_cfg.h"
#include "tapi_cfg_base.h"
#include "tapi_cfg_pci.h"

#define CFG_PCI_TA_DEVICE_FMT "/agent:%s/hardware:/pci:/device:%s"
#define CFG_PCI_TA_VEND_DEVICE_FMT "/agent:%s/hardware:/pci:/vendor:%s/device:%s"

te_errno
tapi_cfg_pci_get_pci_vendor_device(const char *ta, const char *pci_addr,
                                   char **vendor, char **device)
{
    char *vendor_str;
    char *device_str;
    te_errno rc;

    rc = cfg_get_string(&device_str, CFG_PCI_TA_DEVICE_FMT "/device_id:",
                        ta, pci_addr);
    if (rc != 0)
    {
        ERROR("Failed to get device ID by PCI addr %s, %r", pci_addr, rc);
        return rc;
    }

    rc = cfg_get_string(&vendor_str, CFG_PCI_TA_DEVICE_FMT "/vendor_id:",
                        ta, pci_addr);

    if (rc != 0)
    {
        free(device_str);
        ERROR("Failed to get vendor ID by PCI addr %s, %r", pci_addr, rc);
        return rc;
    }

    if (vendor != NULL)
        *vendor = vendor_str;

    if (device != NULL)
        *device = device_str;

    return 0;
}

te_errno
tapi_cfg_pci_get_max_vfs_of_pf(const char *pf_oid, unsigned int *n_vfs)
{
    te_errno rc;
    cfg_val_type type = CVT_INT32;

    rc = cfg_get_instance_fmt(&type, n_vfs, "%s/sriov:", pf_oid);
    if (rc != 0 && TE_RC_GET_ERROR(rc) != TE_ENOENT)
        ERROR("Failed to get virtual functions of a device: %r", rc);
    return rc;
}

te_errno
tapi_cfg_pci_get_vfs_of_pf(const char *pf_oid, bool pci_device,
                           unsigned int *n_pci_vfs, cfg_oid ***pci_vfs,
                           unsigned int **pci_vf_ids)
{
    cfg_handle *vfs = NULL;
    unsigned int n_vfs;
    cfg_oid **result = NULL;
    unsigned int *ids = NULL;
    unsigned int i;
    te_errno rc;

    if (n_pci_vfs == NULL)
    {
        ERROR("%s: pointer to number of VFs must not be NULL", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    rc = cfg_find_pattern_fmt(&n_vfs, &vfs, "%s/sriov:/vf:*", pf_oid);
    if (rc != 0)
    {
        if (TE_RC_GET_ERROR(rc) != TE_ENOENT)
            ERROR("Failed to get virtual functions of a device");
        return rc;
    }

    result = TE_ALLOC(n_vfs * sizeof(*result));

    ids = TE_ALLOC(n_vfs * sizeof(*ids));

    for (i = 0; i < n_vfs; i++)
    {
        char *vf_instance = NULL;
        char *vf_device = NULL;
        cfg_oid *vf_oid;
        cfg_oid *vf_ref_oid;

        rc = cfg_get_oid(vfs[i], &vf_ref_oid);
        if (rc != 0)
        {
            ERROR("Failed to get VF reference from PF");
            goto out;
        }

        rc = te_strtoui(CFG_OID_GET_INST_NAME(vf_ref_oid, 6), 10, &ids[i]);
        cfg_free_oid(vf_ref_oid);
        if (rc != 0)
        {
            ERROR("Failed to parse VF index");
            goto out;
        }

        rc = cfg_get_instance(vfs[i], NULL, &vf_instance);
        if (rc != 0)
        {
            ERROR("Failed to get VF instance");
            goto out;
        }

        if (pci_device)
        {
            rc = cfg_get_string(&vf_device, "%s", vf_instance);
            free(vf_instance);
            vf_instance = NULL;
            if (rc != 0)
            {
                ERROR("Failed to get VF device");
                goto out;
            }
        }

        vf_oid = cfg_convert_oid_str(pci_device ? vf_device : vf_instance);
        free(vf_instance);
        free(vf_device);
        if (vf_oid == NULL)
        {
            rc = TE_RC(TE_TAPI, TE_EINVAL);
            ERROR("Failed to get VF oid");
            goto out;
        }

        result[i] = vf_oid;
    }

    if (pci_vfs != NULL)
        *pci_vfs = result;
    if (pci_vf_ids != NULL)
        *pci_vf_ids = ids;
    *n_pci_vfs = n_vfs;

out:
    free(vfs);
    if (rc != 0 || pci_vfs == NULL)
    {
        for (i = 0; result != NULL && i < n_vfs; i++)
            free(result[i]);
        free(result);
    }

    if (rc != 0 || pci_vf_ids == NULL)
        free(ids);

    return rc;
}

te_errno
tapi_cfg_pci_enable_vfs_of_pf(const char *pf_oid, unsigned int n_vfs)
{
    te_errno rc;

    rc = cfg_set_instance_fmt(CFG_VAL(INT32, n_vfs),
                              "%s/sriov:/num_vfs:", pf_oid);
    if (rc != 0 && TE_RC_GET_ERROR(rc) != TE_ENOENT)
        ERROR("Failed to set the number of VFs for a device: %r", rc);
    return rc;
}

te_errno
tapi_cfg_pci_addr_by_oid(const cfg_oid *pci_device, char **pci_addr)
{
    char *result;

    if (pci_addr == NULL)
    {
        ERROR("%s: output argument must not be NULL", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    result = cfg_oid_get_inst_name(pci_device, 4);
    if (result == NULL)
    {
        ERROR("Failed to get PCI addr by oid");
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    *pci_addr = result;

    return 0;
}

te_errno
tapi_cfg_pci_oid_by_addr(const char *ta, const char *pci_addr,
                         char **pci_oid)
{
    int rc;

    rc = te_asprintf(pci_oid, CFG_PCI_TA_DEVICE_FMT, ta, pci_addr);
    if (rc < 0)
    {
        ERROR("Failed to create a PCI device OID string");
        return TE_RC(TE_TAPI, TE_ENOMEM);
    }

    return 0;
}

te_errno
tapi_cfg_pci_instance_by_addr(const char *ta, const char *pci_addr,
                              char **pci_inst)
{
    cfg_handle *instances = NULL;
    unsigned int n_instances = 0;
    char *vendor = NULL;
    char *device = NULL;
    char *pci_oid = NULL;
    unsigned int i;
    te_errno rc;

    rc = tapi_cfg_pci_get_pci_vendor_device(ta, pci_addr, &vendor, &device);
    if (rc != 0)
        goto out;

    rc = cfg_find_pattern_fmt(&n_instances, &instances,
                              CFG_PCI_TA_VEND_DEVICE_FMT "/instance:*",
                              ta, vendor, device);
    if (rc != 0)
        goto out;

    rc = tapi_cfg_pci_oid_by_addr(ta, pci_addr, &pci_oid);
    if (rc != 0)
        goto out;

    for (i = 0; i < n_instances; ++i)
    {
        char *inst_value = NULL;

        rc = cfg_get_instance(instances[i], NULL, &inst_value);
        if (rc != 0)
        {
            ERROR("Failed to get PCI instance value: %r", rc);
            goto out;
        }
        if (strcmp(inst_value, pci_oid) == 0)
        {
            free(inst_value);
            rc = cfg_get_oid_str(instances[i], pci_inst);
            break;
        }
        free(inst_value);
    }
    if (i == n_instances)
    {
        ERROR("Failed to get PCI instance by '%s' on '%s'", pci_addr, ta);
        rc = TE_RC(TE_TAPI, TE_ENOENT);
    }

out:
    free(pci_oid);
    free(instances);
    free(device);
    free(vendor);

    return rc;
}

te_errno
tapi_cfg_pci_addr_by_oid_array(unsigned int n_devices, const cfg_oid **pci_devices,
                               char ***pci_addrs)
{
    char **result = NULL;
    unsigned int i;
    te_errno rc = 0;

    if (pci_addrs == NULL)
    {
        ERROR("%s: output argument must not be NULL", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    result = TE_ALLOC(sizeof(*result) * n_devices);

    for (i = 0; i < n_devices; i++)
    {
        rc = tapi_cfg_pci_addr_by_oid(pci_devices[i], &result[i]);
        if (rc != 0)
            goto out;
    }

    *pci_addrs = result;

out:
    if (rc != 0)
    {
        for (i = 0; result != NULL && i < n_devices; i++)
            free(result[i]);
        free(result);
    }

    return rc;
}

static char *
tapi_cfg_pci_rsrc_name_gen(const cfg_oid *oid, const char *rsrc_pfx)
{
    te_string rsrc_name = TE_STRING_INIT;
    unsigned int i;
    char *inst_name;

    te_string_append(&rsrc_name, "%s", rsrc_pfx);
    /* The agent's name (1) is not interesting on the agent itself */
    for (i = 2; i < oid->len; i++)
    {
        inst_name = CFG_OID_GET_INST_NAME(oid, i);
        if (*inst_name != '\0')
            te_string_append(&rsrc_name, ":%s", inst_name);
    }

    return rsrc_name.ptr;
}

char *
tapi_cfg_pci_rsrc_name(const cfg_oid *pci_instance)
{
    return tapi_cfg_pci_rsrc_name_gen(pci_instance, "pci_fn");
}

char *
tapi_cfg_pci_fn_netdev_rsrc_name(const cfg_oid *oid)
{
    return tapi_cfg_pci_rsrc_name_gen(oid, "pci_fn_netdev");
}

te_errno
tapi_cfg_pci_grab(const cfg_oid *pci_instance)
{
    char *rsrc_name = NULL;
    char *oid_str = NULL;
    te_errno rc;

    rsrc_name = tapi_cfg_pci_rsrc_name(pci_instance);
    if (rsrc_name == NULL)
    {
        rc = TE_RC(TE_TAPI, TE_ENOMEM);
        goto out;
    }

    rc = cfg_get_instance_fmt(NULL, NULL, "/agent:%s/rsrc:%s",
                              CFG_OID_GET_INST_NAME(pci_instance, 1), rsrc_name);
    if (rc == 0)
    {
        rc = TE_RC(TE_TAPI, TE_EALREADY);
        goto out;
    }

    oid_str = cfg_convert_oid(pci_instance);
    if (oid_str == NULL)
    {
        rc = TE_RC(TE_TAPI, TE_ENOMEM);
        goto out;
    }

    rc = cfg_add_instance_fmt(NULL, CFG_VAL(STRING, oid_str),
                              "/agent:%s/rsrc:%s",
                              CFG_OID_GET_INST_NAME(pci_instance, 1), rsrc_name);
    if (rc != 0)
        ERROR("Failed to reserve resource '%s': %r", oid_str, rc);

out:
    free(rsrc_name);
    free(oid_str);

    return rc;
}

te_errno
tapi_cfg_pci_bind_ta_driver_on_device(const char *ta,
                                      enum tapi_cfg_driver_type type,
                                      const char *pci_addr)
{
    char *ta_driver = NULL;
    char *pci_oid = NULL;
    char *pci_driver = NULL;
    te_errno rc;

    rc = tapi_cfg_pci_get_ta_driver(ta, type, &ta_driver);
    if (rc != 0)
        goto out;

    rc = tapi_cfg_pci_oid_by_addr(ta, pci_addr, &pci_oid);
    if (rc != 0)
        goto out;

    rc = tapi_cfg_pci_get_driver(pci_oid, &pci_driver);
    if (rc != 0)
        goto out;

    if (strcmp(ta_driver, pci_driver) != 0)
    {
        rc = tapi_cfg_pci_bind_driver(pci_oid, ta_driver);
        if (rc != 0)
            goto out;
        /*
         * Synchronize possible changes in PCI device configuration after
         * driver bind.
         */
        rc = cfg_synchronize(pci_oid, true);
        if (rc != 0)
            goto out;
    }

out:
    free(ta_driver);
    free(pci_oid);
    free(pci_driver);

    return rc;
}

te_errno
tapi_cfg_pci_get_ta_driver(const char *ta,
                           enum tapi_cfg_driver_type type,
                           char **driver)
{
    const char *driver_prefix = "";
    char *result = NULL;
    te_errno rc;

    switch (type)
    {
        case NET_DRIVER_TYPE_NONE:
            *driver = strdup("");
            if (*driver == 0)
                return TE_RC(TE_TAPI, TE_ENOMEM);
            return 0;

        case NET_DRIVER_TYPE_NET:
            driver_prefix = "net";
            break;

        case NET_DRIVER_TYPE_DPDK:
            driver_prefix = "dpdk";
            break;

        default:
            ERROR("Invalid PCI driver type");
            return TE_RC(TE_CONF_API, TE_EINVAL);
    }

    rc = cfg_get_string(&result, "/local:%s/%s_driver:", ta, driver_prefix);
    if (rc != 0 && TE_RC_GET_ERROR(rc) != TE_ENOENT)
    {
        ERROR("Failed to get PCI driver of agent %s", ta);
        return rc;
    }

    if (result == NULL || *result == '\0')
    {
        free(result);
        result = NULL;
    }

    *driver = result;

    return 0;
}

static te_errno
tapi_cfg_pci_get_net_if_gen(const char *pci_oid, const char *netdev,
                            char **interface)
{
    te_errno rc;

    rc = cfg_get_string(interface, "%s/net:%s", pci_oid,
                        te_str_is_null_or_empty(netdev) ? "" : netdev);

    if (rc != 0 && rc != TE_RC(TE_CS, TE_ENOENT))
    {
        ERROR("Failed to get the only interface of a PCI device %s: %r",
              pci_oid, rc);
    }

    return rc;
}

te_errno
tapi_cfg_pci_fn_netdev_get_net_if(const char *pci_fn_oid, const char *netdev,
                                  char **interface)
{
    return tapi_cfg_pci_get_net_if_gen(pci_fn_oid, netdev, interface);
}

te_errno
tapi_cfg_pci_get_net_if(const char *pci_oid, char **interface)
{
    return tapi_cfg_pci_get_net_if_gen(pci_oid, NULL, interface);
}

/* See description in tapi_cfg_pci.h */
te_errno
tapi_cfg_pci_oid_by_net_if(const char *ta, const char *if_name,
                           char **pci_oid)
{
    unsigned int names_count;
    cfg_handle *name_handles = NULL;
    te_errno rc;
    unsigned int i;

    cfg_val_type val_type = CVT_STRING;
    char *val_str;

    rc = cfg_find_pattern_fmt(&names_count, &name_handles,
                              CFG_PCI_TA_DEVICE_FMT "/net:*", ta, "*");
    if (rc != 0)
        return rc;

    for (i = 0; i < names_count; i++)
    {
        rc = cfg_get_instance(name_handles[i], &val_type, &val_str);
        if (rc != 0)
            goto out;

        if (strcmp(val_str, if_name) == 0)
        {
            cfg_handle pci_handle;

            free(val_str);

            rc = cfg_get_father(name_handles[i], &pci_handle);
            if (rc != 0)
                goto out;

            rc = cfg_get_oid_str(pci_handle, pci_oid);
            goto out;
        }

        free(val_str);
    }

    rc = TE_ENOENT;

out:

    free(name_handles);
    return rc;
}

te_errno
tapi_cfg_pci_get_numa_node(const char *pci_oid, char **numa_node)
{
    te_errno rc;

    rc = cfg_get_string(numa_node, "%s/node:", pci_oid);
    if (rc != 0)
        ERROR("Failed to get the NUMA node of a PCI device: %r", rc);

    return rc;
}

te_errno
tapi_cfg_pci_get_numa_node_id(const char *pci_oid, int *numa_node)
{
    char *node_oid;
    char *node_str;
    te_errno rc;

    rc = tapi_cfg_pci_get_numa_node(pci_oid, &node_oid);
    if (rc != 0)
        return rc;

    if (node_oid[0] == '\0')
    {
        *numa_node = -1;
        free(node_oid);
        return 0;
    }

    node_str = cfg_oid_str_get_inst_name(node_oid, 3);
    free(node_oid);
    if (node_str == NULL)
    {
        ERROR("Failed to get NUMA node index from OID '%s'", node_oid);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    return te_strtoi(node_str, 0, numa_node);
}

te_errno
tapi_cfg_pci_bind_driver(const char *pci_oid, const char *driver)
{
    char *pci_device = NULL;
    te_errno rc;

    rc = tapi_cfg_pci_resolve_device_oid(&pci_device, "%s", pci_oid);
    if (rc != 0)
        return rc;

    rc = cfg_set_instance_fmt(CFG_VAL(STRING, driver),
                              "%s/driver:", pci_device);
    if (rc != 0)
        ERROR("Failed to bind driver %s on PCI device %s", driver, pci_device);

    free(pci_device);
    return rc;
}

te_errno
tapi_cfg_pci_get_driver(const char *pci_oid, char **driver)
{
    char *pci_device = NULL;
    te_errno rc;

    rc = tapi_cfg_pci_resolve_device_oid(&pci_device, "%s", pci_oid);
    if (rc != 0)
        return rc;

    rc = cfg_get_string(driver, "%s/driver:", pci_device);
    if (rc != 0)
        ERROR("Failed to get current driver of PCI device %s", pci_device);

    free(pci_device);
    return rc;
}

te_errno
tapi_cfg_pci_get_devices(const char *pci_oid, unsigned int *count,
                         char ***device_names)
{
    unsigned int n_devices = 0;
    cfg_handle *devices = NULL;
    char **result = NULL;
    unsigned int i;
    te_errno rc;

    rc = cfg_find_pattern_fmt(&n_devices, &devices,
                              "%s/dev:*", pci_oid);
    if (rc != 0)
        goto out;

    result = TE_ALLOC(n_devices * sizeof(*result));

    for (i = 0; i < n_devices; i++)
    {
        rc = cfg_get_inst_name(devices[i], &result[i]);
        if (rc != 0)
            goto out;
    }

    *count = n_devices;
    if (device_names != NULL)
        *device_names = result;

out:
    free(devices);
    if (rc != 0 || device_names == NULL)
    {
        for (i = 0; result != NULL && i < n_devices; i++)
            free(result[i]);

        free(result);
    }

    return rc;
}

te_errno
tapi_cfg_pci_devices_by_vendor_device(const char *ta, const char *vendor,
                                      const char *device, unsigned int *size,
                                      char ***pci_oids)
{
    cfg_handle *instances = NULL;
    unsigned int n_instances = 0;
    char **result = NULL;
    unsigned int i;
    te_errno rc;

    rc = cfg_find_pattern_fmt(&n_instances, &instances,
                              CFG_PCI_TA_VEND_DEVICE_FMT "/instance:*",
                              ta, vendor, device);
    if (rc != 0)
        goto out;

    result = TE_ALLOC(n_instances * sizeof(*result));

    for (i = 0; i < n_instances; i++)
    {
        rc = cfg_get_instance(instances[i], NULL, &result[i]);
        if (rc != 0)
        {
            ERROR("Failed to get PCI device");
            goto out;
        }
    }

    *size = n_instances;
    if (pci_oids != NULL)
        *pci_oids = result;

out:
    free(instances);
    if (rc != 0 || pci_oids == NULL)
    {
        for (i = 0; result != NULL && i < n_instances; i++)
            free(result[i]);

        free(result);
    }

    return rc;
}

static te_errno
pci_oid_copy(const char *pci_oid, const cfg_oid *parsed_oid, void *ctx)
{
    UNUSED(parsed_oid);
    *(char **)ctx = strdup(pci_oid);
    return 0;
}

static te_errno
pci_oid_do_resolve(const char *pci_oid, const cfg_oid *parsed_oid, void *ctx)
{
    UNUSED(parsed_oid);
    return cfg_get_string(ctx, "%s", pci_oid);
}

static cfg_oid_rule pci_oid_resolve_rules[] = {
    CFG_OID_RULE(false, pci_oid_copy, {"agent"}, {"hardware"},
                 {"pci"}, {"device"}),
    CFG_OID_RULE(false, pci_oid_do_resolve, {"agent"}, {"hardware"},
                 {"pci"}, {"vendor"}, {"device"}, {"instance"}),
    CFG_OID_RULE_END
};

te_errno
tapi_cfg_pci_resolve_device_oid(char **pci_dev_oid, const char *pci_inst_fmt,
                                ...)
{
    te_string pci_inst_oid = TE_STRING_INIT;
    te_errno rc;
    va_list args;

    va_start(args, pci_inst_fmt);
    te_string_append_va(&pci_inst_oid, pci_inst_fmt, args);
    va_end(args);

    rc = cfg_oid_dispatch(pci_oid_resolve_rules, pci_inst_oid.ptr, pci_dev_oid);
    te_string_free(&pci_inst_oid);

    return rc;
}

static te_errno
tapi_cfg_pci_get_pcioid_by_vend_dev_inst(const char *ta, const char *vendor,
                                         const char *device,
                                         unsigned int instance,
                                         char **pci_oid)
{
    char *pci_oidstr;
    te_errno rc;

    if (ta == NULL)
    {
        ERROR("%s: test agent name must not be NULL", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    if (vendor == NULL)
    {
        ERROR("%s: vendor parameter must not be NULL", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    if (device == NULL)
    {
        ERROR("%s: device parameter must not be NULL", __FUNCTION__);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    rc = cfg_get_string(&pci_oidstr,
                        CFG_PCI_TA_VEND_DEVICE_FMT "/instance:%d",
                        ta, vendor, device, instance);
    if (rc != 0)
    {
        ERROR("Failed to get PCI oid by %s:%s:%d, %r", vendor, device,
              instance, rc);
        pci_oidstr = NULL;
    }

    *pci_oid = pci_oidstr;

    return rc;
}

te_errno
tapi_cfg_pci_bind_driver_by_vend_dev_inst(const char *ta, const char *vendor,
                                          const char *device,
                                          unsigned int instance,
                                          const char *driver)
{
    te_errno rc;
    char *pci_oidstr = NULL;

    rc = tapi_cfg_pci_get_pcioid_by_vend_dev_inst(ta, vendor, device,
                                                  instance, &pci_oidstr);
    if (rc != 0)
        goto out;

    rc = tapi_cfg_pci_bind_driver(pci_oidstr, driver);

out:
    free(pci_oidstr);
    return rc;
}

te_errno
tapi_cfg_pci_unbind_driver_by_vend_dev_inst(const char *ta, const char *vendor,
                                            const char *device,
                                            unsigned int instance)
{
    te_errno rc;
    char *pci_oidstr = NULL;

    rc = tapi_cfg_pci_get_pcioid_by_vend_dev_inst(ta, vendor, device,
                                                  instance, &pci_oidstr);
    if (rc != 0)
        goto out;

    rc = tapi_cfg_pci_bind_driver(pci_oidstr, "");

out:
    free(pci_oidstr);
    return rc;
}

te_errno
tapi_cfg_pci_get_driver_by_vend_dev_inst(const char *ta, const char *vendor,
                                         const char *device,
                                         unsigned int instance,
                                         char **driver)
{
    te_errno rc;
    char *pci_oidstr = NULL;

    rc = tapi_cfg_pci_get_pcioid_by_vend_dev_inst(ta, vendor, device,
                                                  instance, &pci_oidstr);
    if (rc != 0)
        goto out;

    rc = tapi_cfg_pci_get_driver(pci_oidstr, driver);

out:
    free(pci_oidstr);
    return rc;
}

/* See description in tapi_cfg_pci.h */
te_errno
tapi_cfg_pci_get_serialno(const char *pci_oid, char **serialno)
{
    return cfg_get_string(serialno, "%s/serialno:", pci_oid);
}

te_errno
tapi_cfg_pci_get_class(const char *pci_oid, unsigned int *class_id,
                       unsigned int *subclass_id, unsigned int *intf_id)
{
    char *resolved_oid = NULL;
    char *class_str = NULL;
    te_errno rc;
    unsigned int class_code;

    rc = tapi_cfg_pci_resolve_device_oid(&resolved_oid, "%s", pci_oid);
    if (rc != 0)
        return rc;

    rc = cfg_get_string(&class_str, "%s/class:", resolved_oid);
    free(resolved_oid);
    if (rc != 0)
        return rc;

    rc = te_strtoui(class_str, 16, &class_code);
    free(class_str);
    if (rc != 0)
        return TE_RC_UPSTREAM(TE_TAPI, rc);

    /* High byte should be zero */
    if ((class_code >> 24) != 0)
    {
        ERROR("Invalid class code %08x", class_code);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    if (class_id != NULL)
        *class_id = te_pci_progintf2class(class_code);

    if (subclass_id != NULL)
        *subclass_id = te_pci_progintf2subclass(class_code);

    if (intf_id != NULL)
        *intf_id = class_code;

    return 0;
}

/* Convert configuration mode constant to string name */
static const char *
cmode_to_str(tapi_cfg_pci_param_cmode cmode)
{
    switch (cmode)
    {
        case TAPI_CFG_PCI_PARAM_RUNTIME:
            return "runtime";

        case TAPI_CFG_PCI_PARAM_DRIVERINIT:
            return "driverinit";

        case TAPI_CFG_PCI_PARAM_PERMANENT:
            return "permanent";
    }

    return "<unknown>";
}

/* See description in tapi_cfg_pci.h */
te_errno
tapi_cfg_pci_param_is_present(const char *pci_oid,
                              const char *param_name,
                              bool *present)
{
    cfg_handle handle;
    te_errno rc;

    rc = cfg_find_fmt(&handle, "%s/param:%s", pci_oid, param_name);
    if (rc == 0)
    {
        *present = true;
    }
    else if (rc == TE_RC(TE_CS, TE_ENOENT))
    {
        *present = false;
        rc = 0;
    }

    return rc;
}


/* See description in tapi_cfg_pci.h */
te_errno
tapi_cfg_pci_get_param_str(const char *pci_oid,
                           const char *param_name,
                           tapi_cfg_pci_param_cmode cmode,
                           char **value)
{
    return cfg_get_string(value, "%s/param:%s/value:%s",
                          pci_oid, param_name, cmode_to_str(cmode));
}

/* See description in tapi_cfg_pci.h */
te_errno
tapi_cfg_pci_get_param_uint(const char *pci_oid, const char *param_name,
                            tapi_cfg_pci_param_cmode cmode,
                            uint64_t *value)
{
    char *val_str;
    te_errno rc;

    rc = tapi_cfg_pci_get_param_str(pci_oid, param_name, cmode, &val_str);
    if (rc != 0)
        return rc;

    rc = te_str_to_uint64(val_str, 10, value);
    free(val_str);
    return rc;
}

/* See description in tapi_cfg_pci.h */
te_errno
tapi_cfg_pci_set_param_str(const char *pci_oid,
                           const char *param_name,
                           tapi_cfg_pci_param_cmode cmode,
                           const char *value)
{
    return cfg_set_instance_fmt(CFG_VAL(STRING, value),
                                "%s/param:%s/value:%s",
                                pci_oid, param_name,
                                cmode_to_str(cmode));
}

/* See description in tapi_cfg_pci.h */
te_errno
tapi_cfg_pci_set_param_uint(const char *pci_oid,
                            const char *param_name,
                            tapi_cfg_pci_param_cmode cmode,
                            uint64_t value)
{
    char val_str[RCF_MAX_VAL] = "";
    te_errno rc;

    rc = te_snprintf(val_str, sizeof(val_str), "%" TE_PRINTF_64 "u",
                     value);
    if (rc != 0)
        return rc;

    return tapi_cfg_pci_set_param_str(pci_oid, param_name, cmode, val_str);
}

/*
 * Get value of a hexadecimal property (like vendor ID or device ID).
 */
static te_errno
get_hex_prop(const char *pci_oid, const char *name,
             unsigned int *value)
{
    te_errno rc;
    char *id = NULL;

    if (value == NULL)
        return 0;

    rc = cfg_get_string(&id, "%s/%s:", pci_oid, name);
    if (rc != 0)
        return rc;

    rc = te_strtoui(id, 16, value);
    if (rc != 0)
        ERROR("Cannot convert PCI %s '%s' to number", name, id);

    free(id);
    return rc;
}

/* See description in tapi_cfg_pci.h */
te_errno
tapi_cfg_pci_get_vendor_dev_ids(const char *pci_oid,
                                unsigned int *vendor_id,
                                unsigned int *device_id,
                                unsigned int *subsystem_vendor_id,
                                unsigned int *subsystem_device_id)
{
    te_errno rc = 0;

    rc = get_hex_prop(pci_oid, "vendor_id", vendor_id);
    if (rc != 0)
        return rc;

    rc = get_hex_prop(pci_oid, "device_id", device_id);
    if (rc != 0)
        return rc;

    rc = get_hex_prop(pci_oid, "subsystem_vendor",
                      subsystem_vendor_id);
    if (rc != 0)
        return rc;

    return get_hex_prop(pci_oid, "subsystem_device",
                        subsystem_device_id);
}


/* See description in tapi_cfg_pci.h */
te_errno
tapi_cfg_pci_get_spdk_config_filename(const char *pci_oid, const char *cfg_name,
                                      bool create, char **filename)
{
    char *resolved_oid = NULL;
    te_errno rc;

    rc = tapi_cfg_pci_resolve_device_oid(&resolved_oid, "%s", pci_oid);
    if (rc != 0)
        return rc;

    if (create)
    {
        rc = cfg_add_instance_fmt(NULL, CFG_VAL(NONE, NULL),
                                  "%s/spdk_config:%s", resolved_oid, cfg_name);
        if (rc != 0)
        {
            free(resolved_oid);
            return rc;
        }
    }

    rc = cfg_get_string(filename, "%s/spdk_config:%s/filename:",
                        resolved_oid, cfg_name);
    free(resolved_oid);

    return rc;
}
