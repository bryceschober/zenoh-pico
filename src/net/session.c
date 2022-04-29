/*
 * Copyright (c) 2017, 2021 ADLINK Technology Inc.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
 * which is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *
 * Contributors:
 *   ADLINK zenoh team, <zenoh@adlink-labs.tech>
 */

#include "zenoh-pico/net/session.h"
#include "zenoh-pico/net/memory.h"
#include "zenoh-pico/session/utils.h"
#include "zenoh-pico/transport/link/task/lease.h"
#include "zenoh-pico/transport/link/task/read.h"
#include "zenoh-pico/utils/logging.h"

_z_session_t *__z_open_inner(_z_str_t locator, int mode)
{
    _z_session_t *zn = _z_session_init();

    _z_transport_p_result_t res = _z_new_transport(zn->_tp_manager, locator, mode);
    if (res._tag == _Z_RES_ERR)
        goto ERR;

    // Attach session and transport to one another
    zn->_tp = res._value._transport;
    if (zn->_tp->_type == _Z_TRANSPORT_UNICAST_TYPE)
        zn->_tp->_transport._unicast._session = zn;
    else if (zn->_tp->_type == _Z_TRANSPORT_MULTICAST_TYPE)
        zn->_tp->_transport._multicast._session = zn;

    return zn;

ERR:
    _z_session_free(&zn);

    return NULL;
}

_z_session_t *_z_open(_z_properties_t *config)
{
    if (config == NULL)
        return NULL;

    _z_str_t locator = NULL;
    // Scout if peer is not configured
    if (_z_properties_get(config, Z_CONFIG_PEER_KEY) == NULL)
    {
        // Z_CONFIG_SCOUTING_TIMEOUT_KEY is expressed in seconds as a float
        // while the scout loop timeout uses milliseconds granularity
        _z_str_t tout = _z_properties_get(config, Z_CONFIG_SCOUTING_TIMEOUT_KEY);
        if (tout == NULL)
            tout = Z_CONFIG_SCOUTING_TIMEOUT_DEFAULT;
        clock_t timeout = strtof(tout, NULL);

        // Scout and return upon the first result
        _z_hello_array_t locs = _z_scout_inner(Z_ROUTER, config, timeout, 1);
        if (_z_hello_array_len(&locs) > 0)
        {
            if (_z_str_array_len(&_z_hello_array_get(&locs, 0)->_locators) > 0)
                locator = _z_str_clone(*_z_str_array_get(&_z_hello_array_get(&locs, 0)->_locators, 0));
            else
            {
                _z_hello_array_clear(&locs);
                return NULL;
            }

            _z_hello_array_clear(&locs);
        }
        else
        {
            _Z_INFO("Unable to scout a zenoh router\n");
            _Z_ERROR("Please make sure at least one router is running on your network!\n");

            return NULL;
        }
    }
    else
        locator = _z_str_clone(_z_properties_get(config, Z_CONFIG_PEER_KEY));

    // @TODO: check invalid configurations
    // For example, client mode in multicast links

    // Check operation mode
    _z_str_t s_mode = _z_properties_get(config, Z_CONFIG_MODE_KEY);
    int mode = 0; // By default, zenoh-pico will operate as a client
    if (_z_str_eq(s_mode, Z_CONFIG_MODE_CLIENT))
        mode = 0;
    else if (_z_str_eq(s_mode, Z_CONFIG_MODE_PEER))
        mode = 1;

    _z_session_t *zn = __z_open_inner(locator, mode);
    
    free(locator);
    return zn;
}

void _z_close(_z_session_t *zn)
{
    _z_session_close(zn, _Z_CLOSE_GENERIC);
}

_z_properties_t *_z_info(const _z_session_t *zn)
{
    _z_properties_t *ps = (_z_properties_t *)malloc(sizeof(_z_properties_t));
    _z_properties_init(ps);
    _z_properties_insert(ps, Z_INFO_PID_KEY, _z_string_from_bytes(&zn->_tp_manager->_local_pid));
    if (zn->_tp->_type == _Z_TRANSPORT_UNICAST_TYPE)
    {
        _z_properties_insert(ps, Z_INFO_ROUTER_PID_KEY, _z_string_from_bytes(&zn->_tp->_transport._unicast._remote_pid));
    }
    else if (zn->_tp->_type == _Z_TRANSPORT_MULTICAST_TYPE)
    {
        _z_transport_peer_entry_list_t *xs = zn->_tp->_transport._multicast._peers;
        while (xs != NULL)
        {
            _z_transport_peer_entry_t *peer = _z_transport_peer_entry_list_head(xs);
            _z_properties_insert(ps, Z_INFO_PEER_PID_KEY, _z_string_from_bytes(&peer->_remote_pid));

            xs = _z_transport_peer_entry_list_tail(xs);
        }
    }
    return ps;
}

int _zp_read(_z_session_t *zn)
{
    return _z_read(zn->_tp);
}

int _zp_send_keep_alive(_z_session_t *zn)
{
    return _z_send_keep_alive(zn->_tp);
}

int _zp_start_read_task(_z_session_t *zn)
{
    _z_task_t *task = (_z_task_t *)malloc(sizeof(_z_task_t));
    memset(task, 0, sizeof(_z_task_t));

    if (zn->_tp->_type == _Z_TRANSPORT_UNICAST_TYPE)
    {
        zn->_tp->_transport._unicast._read_task = task;
        if (_z_task_init(task, NULL, _zp_unicast_read_task, &zn->_tp->_transport._unicast) != 0)
            return -1;
    }
    else if (zn->_tp->_type == _Z_TRANSPORT_MULTICAST_TYPE)
    {
        zn->_tp->_transport._multicast._read_task = task;
        if (_z_task_init(task, NULL, _zp_multicast_read_task, &zn->_tp->_transport._multicast) != 0)
            return -1;
    }
    else
        return -1;

    return 0;
}

int _zp_stop_read_task(_z_session_t *zn)
{
    if (zn->_tp->_type == _Z_TRANSPORT_UNICAST_TYPE)
        zn->_tp->_transport._unicast._read_task_running = 0;
    else if (zn->_tp->_type == _Z_TRANSPORT_MULTICAST_TYPE)
        zn->_tp->_transport._multicast._read_task_running = 0;

    return 0;
}

int _zp_start_lease_task(_z_session_t *zn)
{
    _z_task_t *task = (_z_task_t *)malloc(sizeof(_z_task_t));
    memset(task, 0, sizeof(_z_task_t));

    if (zn->_tp->_type == _Z_TRANSPORT_UNICAST_TYPE)
    {
        zn->_tp->_transport._unicast._lease_task = task;
        if (_z_task_init(task, NULL, _zp_unicast_lease_task, &zn->_tp->_transport._unicast) != 0)
            return -1;
    }
    else if (zn->_tp->_type == _Z_TRANSPORT_MULTICAST_TYPE)
    {
        zn->_tp->_transport._multicast._lease_task = task;
        if (_z_task_init(task, NULL, _zp_multicast_lease_task, &zn->_tp->_transport._multicast) != 0)
            return -1;
    }
    else
        return -1;

    return 0;
}

int _zp_stop_lease_task(_z_session_t *zn)
{
    if (zn->_tp->_type == _Z_TRANSPORT_UNICAST_TYPE)
        zn->_tp->_transport._unicast._lease_task_running = 0;
    else if (zn->_tp->_type == _Z_TRANSPORT_MULTICAST_TYPE)
        zn->_tp->_transport._multicast._lease_task_running = 0;

    return 0;
}