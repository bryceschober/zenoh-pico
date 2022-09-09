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
//

#include "zenoh-pico/collections/list.h"

#include <stddef.h>

/*-------- Inner single-linked list --------*/

// Pointer
_z_list_t *_z_list_of(void *x) {
    _z_list_t *xs = (_z_list_t *)z_malloc(sizeof(_z_list_t));
    xs->_val = x;
    xs->_tail = NULL;
    return xs;
}

_z_list_t *_z_list_push(_z_list_t *xs, void *x) {
    _z_list_t *lst = _z_list_of(x);
    lst->_tail = xs;
    return lst;
}

void *_z_list_head(const _z_list_t *xs) { return xs->_val; }

_z_list_t *_z_list_tail(const _z_list_t *xs) { return xs->_tail; }

size_t _z_list_len(const _z_list_t *xs) {
    size_t len = 0;
    _z_list_t *l = (_z_list_t *)xs;
    while (l != NULL) {
        len += 1;
        l = _z_list_tail(l);
    }
    return len;
}

uint8_t _z_list_is_empty(const _z_list_t *xs) { return _z_list_len(xs) == 0; }

_z_list_t *_z_list_pop(_z_list_t *xs, z_element_free_f f_f) {
    _z_list_t *head = xs;
    xs = head->_tail;
    f_f(&head->_val);
    z_free(head);

    return xs;
}

_z_list_t *_z_list_find(const _z_list_t *xs, z_element_eq_f c_f, void *e) {
    _z_list_t *l = (_z_list_t *)xs;
    while (l != NULL) {
        void *head = _z_list_head(l);
        if (c_f(e, head)) return l;
        l = _z_list_tail(l);
    }
    return NULL;
}

_z_list_t *_z_list_drop_filter(_z_list_t *xs, z_element_free_f f_f, z_element_eq_f c_f, void *left) {
    _z_list_t *previous = xs;
    _z_list_t *current = xs;

    while (current != NULL) {
        if (c_f(left, current->_val)) {
            _z_list_t *this = current;

            // head removal
            if (this == xs) {
                xs = xs->_tail;
                previous = xs;
            }
            // tail removal
            else if (this->_tail == NULL) {
                previous->_tail = NULL;
            }
            // middle removal
            else {
                previous->_tail = this->_tail;
            }

            current = this->_tail;

            f_f(&this->_val);
            z_free(this);
            return xs;
        } else {
            previous = current;
            current = current->_tail;
        }
    }

    return xs;
}

_z_list_t *_z_list_clone(const _z_list_t *xs, z_element_clone_f d_f) {
    _z_list_t *new = NULL;

    _z_list_t *head = (_z_list_t *)xs;
    while (head != NULL) {
        void *x = d_f(_z_list_head(head));
        new = _z_list_push(new, x);
        head = _z_list_tail(head);
    }

    return new;
}

void _z_list_free(_z_list_t **xs, z_element_free_f f) {
    _z_list_t *ptr = (_z_list_t *)*xs;
    while (ptr != NULL) ptr = _z_list_pop(ptr, f);

    *xs = NULL;
}

// Smart pointer
_z_sptr_list_t *_z_list_sptr_of(_z_elem_sptr_t x) {
    _z_sptr_list_t *xs = (_z_sptr_list_t *)z_malloc(sizeof(_z_sptr_list_t));
    xs->_val = x;
    xs->_tail = NULL;
    return xs;
}

_z_sptr_list_t *_z_list_sptr_push(_z_sptr_list_t *xs, _z_elem_sptr_t x) {
    _z_sptr_list_t *lst = _z_list_sptr_of(x);
    lst->_tail = xs;
    return lst;
}

_z_elem_sptr_t _z_list_sptr_head(const _z_sptr_list_t *xs) { return xs->_val; }

_z_sptr_list_t *_z_sptr_list_tail(const _z_sptr_list_t *xs) { return xs->_tail; }

size_t _z_list_sptr_len(const _z_sptr_list_t *xs) {
    size_t len = 0;
    _z_sptr_list_t *l = (_z_sptr_list_t *)xs;
    while (l != NULL) {
        len += 1;
        l = _z_sptr_list_tail(l);
    }
    return len;
}

uint8_t _z_list_sptr_is_empty(const _z_sptr_list_t *xs) { return _z_list_sptr_len(xs) == 0; }

_z_sptr_list_t *_z_list_sptr_pop(_z_sptr_list_t *xs, z_element_free_f f_f) {
    _z_sptr_list_t *head = xs;
    xs = head->_tail;
    _z_elem_sptr_drop(&head->_val, f_f);
    z_free(head);

    return xs;
}

_z_sptr_list_t *_z_list_sptr_find(const _z_sptr_list_t *xs, _z_elem_sptr_t *e) {
    _z_sptr_list_t *l = (_z_sptr_list_t *)xs;
    while (l != NULL) {
        _z_elem_sptr_t head = _z_list_sptr_head(l);
        if (_z_elem_sptr_eq(e, &head)) return l;
        l = _z_sptr_list_tail(l);
    }
    return NULL;
}

_z_sptr_list_t *_z_list_sptr_drop_filter(_z_sptr_list_t *xs, z_element_free_f f_f, _z_elem_sptr_t *left) {
    _z_sptr_list_t *previous = xs;
    _z_sptr_list_t *current = xs;

    while (current != NULL) {
        if (_z_elem_sptr_eq(left, &current->_val)) {
            _z_sptr_list_t *this = current;

            // head removal
            if (this == xs) {
                xs = xs->_tail;
                previous = xs;
            }
            // tail removal
            else if (this->_tail == NULL) {
                previous->_tail = NULL;
            }
            // middle removal
            else {
                previous->_tail = this->_tail;
            }

            current = this->_tail;

            _z_elem_sptr_drop(&this->_val, f_f);
            z_free(this);
            return xs;
        } else {
            previous = current;
            current = current->_tail;
        }
    }

    return xs;
}

_z_sptr_list_t *_z_list_sptr_clone(const _z_sptr_list_t *xs) {
    _z_sptr_list_t *new = NULL;

    _z_sptr_list_t *head = (_z_sptr_list_t *)xs;
    while (head != NULL) {
        _z_elem_sptr_t sptr = _z_list_sptr_head(head);
        new = _z_list_sptr_push(new, _z_elem_sptr_clone(&sptr));
        head = _z_sptr_list_tail(head);
    }

    return new;
}

void _z_list_sptr_free(_z_sptr_list_t **xs, z_element_free_f f) {
    _z_sptr_list_t *ptr = (_z_sptr_list_t *)*xs;
    while (ptr != NULL) ptr = _z_list_sptr_pop(ptr, f);

    *xs = NULL;
}
