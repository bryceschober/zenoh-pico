//
// Copyright (c) 2022 ZettaScale Technology
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
// which is available at https://www.apache.org/licenses/LICENSE-2.0.
//
// SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
//
// Contributors:
//   ZettaScale Zenoh Team, <zenoh@zettascale.tech>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "zenoh-pico.h"

void data_handler(const z_sample_t *sample, void *arg)
{
    (void) (arg);
    printf(">> [Pull Subscriber] Received ('%s': '%.*s')\n",
           z_keyexpr_to_string(sample->keyexpr), (int)sample->payload.len, sample->payload.start);
}

int main(int argc, char **argv)
{
    z_init_logger();

    char *keyexpr = "demo/example/**";
    if (argc > 1)
        keyexpr = argv[1];

    z_owned_config_t config = zp_config_default();
    if (argc > 2)
        zp_config_insert(z_config_loan(&config), Z_CONFIG_PEER_KEY, z_string_make(argv[2]));

    printf("Opening session...\n");
    z_owned_session_t s = z_open(z_config_move(&config));
    if (!z_session_check(&s))
    {
        printf("Unable to open session!\n");
        exit(-1);
    }

    // Start read and lease tasks for zenoh-pico
    zp_start_read_task(z_session_loan(&s));
    zp_start_lease_task(z_session_loan(&s));

    z_owned_closure_sample_t callback = z_closure_sample(data_handler, NULL, NULL);
    printf("Declaring Subscriber on '%s'...\n", keyexpr);
    z_owned_pull_subscriber_t sub = z_declare_pull_subscriber(z_session_loan(&s), z_keyexpr(keyexpr), z_closure_sample_move(&callback), NULL);
    if (!z_pull_subscriber_check(&sub))
    {
        printf("Unable to declare subscriber.\n");
        exit(-1);
    }

    printf("Enter any key to pull data or 'q' to quit...\n");
    char c = 0;
    while (1)
    {
        c = getchar();
        if (c == 'q')
            break;
        z_pull(z_pull_subscriber_loan(&sub));
    }

    z_undeclare_pull_subscriber(z_pull_subscriber_move(&sub));

    // Stop the receive and the session lease loop for zenoh-pico
    zp_stop_read_task(z_session_loan(&s));
    zp_stop_lease_task(z_session_loan(&s));

    z_close(z_session_move(&s));

    return 0;
}
