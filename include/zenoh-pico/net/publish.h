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

#ifndef ZENOH_PICO_PUBLISH_NETAPI_H
#define ZENOH_PICO_PUBLISH_NETAPI_H

#include "zenoh-pico/protocol/core.h"

/**
 * Return type when declaring a publisher.
 */
typedef struct
{
    void *_zn;  // FIXME: _z_session_t *zn;
    _z_zint_t _id;
    _z_keyexpr_t _key;
    int8_t _local_routing;
    z_congestion_control_t _congestion_control;
    z_priority_t _priority;
} _z_publisher_t;

#endif /* ZENOH_PICO_PUBLISH_NETAPI_H */