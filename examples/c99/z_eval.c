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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "zenoh-pico.h"

char *expr = "/demo/example/zenoh-pico-eval";
char *value = "Eval from Pico!";

void query_handler(const z_query_t *query, const void *arg)
{
    (void) (arg);

    char *res = query->key.suffix;
    char *pred = query->predicate;
    printf(">> [Queryable ] Received Query '%s?%s'\n", res, pred);
    z_send_reply(query, expr, (const unsigned char *)value, strlen(value));

    _z_str_clear(res);
}

int main(int argc, char **argv)
{
    z_init_logger();

    if (argc > 1)
    {
        expr = argv[1];
    }
    z_owned_config_t config = z_config_default();
    if (argc > 2)
    {
        z_config_insert(z_config_loan(&config), Z_CONFIG_PEER_KEY, z_string_make(argv[2]));
    }

    z_config_insert(z_config_loan(&config), Z_CONFIG_USER_KEY, z_string_make("user"));
    z_config_insert(z_config_loan(&config), Z_CONFIG_PASSWORD_KEY, z_string_make("password"));

    printf("Openning session...\n");
    z_owned_session_t s = z_open(z_config_move(&config));
    if (!z_session_check(&s))
    {
        printf("Unable to open session!\n");
        exit(-1);
    }

    // Start the receive and the session lease loop for zenoh-pico
    zp_start_read_task(z_session_loan(&s));
    zp_start_lease_task(z_session_loan(&s));

    printf("Creating Queryable on '%s'...\n", expr);
    z_owned_queryable_t qable = z_queryable_new(z_session_loan(&s), z_keyexpr(expr), Z_QUERYABLE_EVAL, query_handler, NULL);
    if (!z_queryable_check(&qable))
    {
        printf("Unable to create queryable.\n");
        goto EXIT;
    }

    printf("Enter 'q' to quit...\n");
    char c = 0;
    while (c != 'q')
    {
        c = getchar();
    }

    z_queryable_close(z_queryable_move(&qable));

EXIT:
    zp_stop_read_task(z_session_loan(&s));
    zp_stop_lease_task(z_session_loan(&s));
    z_close(z_session_move(&s));
    return 0;
}