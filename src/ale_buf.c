/* Rhizomatica ALE HF controller */

/* (C) 2020 by Rafael Diniz <rafael@rhizomatica.org>
 * All Rights Reserved
 *
 * SPDX-License-Identifier: AGPL-3.0+
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <sys/resource.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/mman.h>
#include <malloc.h>
// #include <threads.h>

#include "ale_buf.h"
#include "ale_shm.h"

// Private functions

static void advance_pointer_n(cbuf_handle_t cbuf, size_t len)
{
    assert(cbuf);

    fprintf(stderr, "head = %ld\n", cbuf->internal->head);

    if(cbuf->internal->full)
    {
        cbuf->internal->tail = (cbuf->internal->tail + len) % cbuf->internal->max;
    }

    cbuf->internal->head = (cbuf->internal->head + len) % cbuf->internal->max;

    // We mark full because we will advance tail on the next time around
    cbuf->internal->full = (cbuf->internal->head == cbuf->internal->tail);
}

static void advance_pointer(cbuf_handle_t cbuf)
{
    assert(cbuf && cbuf->internal);

    if(cbuf->internal->full)
    {
        cbuf->internal->tail = (cbuf->internal->tail + 1) % cbuf->internal->max;
    }

    cbuf->internal->head = (cbuf->internal->head + 1) % cbuf->internal->max;

    // We mark full because we will advance tail on the next time around
    cbuf->internal->full = (cbuf->internal->head == cbuf->internal->tail);
}

static void retreat_pointer_n(cbuf_handle_t cbuf, size_t len)
{
    assert(cbuf && cbuf->internal);

    cbuf->internal->full = false;
    cbuf->internal->tail = (cbuf->internal->tail + len) % cbuf->internal->max;
}

static void retreat_pointer(cbuf_handle_t cbuf)
{
    assert(cbuf->internal);

    cbuf->internal->full = false;
    cbuf->internal->tail = (cbuf->internal->tail + 1) % cbuf->internal->max;
}

// User APIs

cbuf_handle_t circular_buf_init(uint8_t* buffer, size_t size)
{
    assert(buffer && size);

    cbuf_handle_t cbuf = memalign(SHMLBA, sizeof(struct circular_buf_t));
    assert(cbuf);

    cbuf->internal = memalign(SHMLBA, sizeof(struct circular_buf_t_aux));
    assert(cbuf->internal);

    cbuf->buffer = buffer;
    cbuf->internal->max = size;
    circular_buf_reset(cbuf);
    atomic_flag_clear(&cbuf->internal->acquire);

    assert(circular_buf_empty(cbuf));

    return cbuf;
}

cbuf_handle_t circular_buf_init_shm(size_t size, key_t key)
{
    assert(size);

    cbuf_handle_t cbuf = memalign(SHMLBA, sizeof(struct circular_buf_t));
    assert(cbuf);

    if (shm_is_created(key, size))
    {
        fprintf(stderr, "shm key %u already created. Re-creating.\n", key);
        shm_destroy(key, size);
    }
    shm_create(key, size);

    cbuf->buffer = shm_attach(key, size);
    assert(cbuf->buffer);

    key++;
    if (shm_is_created(key, sizeof(struct circular_buf_t_aux)))
    {
        fprintf(stderr, "shm key %u already created. Re-creating.\n", key);
        shm_destroy(key, sizeof(struct circular_buf_t_aux));
    }
    shm_create(key, sizeof(struct circular_buf_t_aux));

    cbuf->internal = shm_attach(key, sizeof(struct circular_buf_t_aux));
    assert(cbuf->internal);

    cbuf->internal->max = size;
    atomic_flag_clear(&cbuf->internal->acquire);
    circular_buf_reset(cbuf);

    assert(circular_buf_empty(cbuf));

    return cbuf;
}

cbuf_handle_t circular_buf_connect_shm(size_t size, key_t key)
{
    assert(size);

    cbuf_handle_t cbuf = memalign(SHMLBA, sizeof(struct circular_buf_t));
    assert(cbuf);

    cbuf->buffer = shm_attach(key, size);
    assert(cbuf->buffer);

    cbuf->internal = shm_attach(key+1, sizeof(struct circular_buf_t_aux));
    assert(cbuf->internal);

    assert (cbuf->internal->max == size);

    return cbuf;
}


void circular_buf_free(cbuf_handle_t cbuf)
{
    assert(cbuf && cbuf->internal);
    free(cbuf->internal);
    free(cbuf);
}

void circular_buf_free_shm(cbuf_handle_t cbuf, size_t size, key_t key)
{
    assert(cbuf && cbuf->internal && cbuf->buffer);
    shm_dettach(key, size, cbuf->buffer);
    shm_destroy(key, size);
    shm_dettach(key+1, sizeof(struct circular_buf_t_aux), cbuf->internal);
    shm_destroy(key+1, sizeof(struct circular_buf_t_aux));
    free(cbuf);
}

void circular_buf_reset(cbuf_handle_t cbuf)
{
    assert(cbuf && cbuf->internal);

    while (atomic_flag_test_and_set(&cbuf->internal->acquire));

    cbuf->internal->head = 0;
    cbuf->internal->tail = 0;
    cbuf->internal->full = false;

    atomic_flag_clear(&cbuf->internal->acquire);
}

size_t circular_buf_size(cbuf_handle_t cbuf)
{
    assert(cbuf && cbuf->internal);

    size_t size = circular_buf_capacity(cbuf);

    while (atomic_flag_test_and_set(&cbuf->internal->acquire));

    if(!cbuf->internal->full)
    {
        if(cbuf->internal->head >= cbuf->internal->tail)
        {
            size = (cbuf->internal->head - cbuf->internal->tail);
        }
        else
        {
            size = (cbuf->internal->max + cbuf->internal->head - cbuf->internal->tail);
        }

    }

    atomic_flag_clear(&cbuf->internal->acquire);

    fprintf(stderr, "size = %ld\n", size);
    return size;
}

size_t circular_buf_free_size(cbuf_handle_t cbuf)
{
    assert(cbuf->internal);

    size_t size = 0;

    while (atomic_flag_test_and_set(&cbuf->internal->acquire));

    if(!cbuf->internal->full)
    {
        if(cbuf->internal->head >= cbuf->internal->tail)
        {
            size = cbuf->internal->max - (cbuf->internal->head - cbuf->internal->tail);
        }
        else
        {
            size = (cbuf->internal->tail - cbuf->internal->head);
        }

    }

    atomic_flag_clear(&cbuf->internal->acquire);

    return size;
}

size_t circular_buf_capacity(cbuf_handle_t cbuf)
{
    assert(cbuf->internal);

    while (atomic_flag_test_and_set(&cbuf->internal->acquire));

    size_t capacity = cbuf->internal->max;

    atomic_flag_clear(&cbuf->internal->acquire);

    return capacity;
}

int circular_buf_put(cbuf_handle_t cbuf, uint8_t data)
{
    assert(cbuf && cbuf->internal && cbuf->buffer);

    int r = -1;

    if(!circular_buf_full(cbuf))
    {
        while (atomic_flag_test_and_set(&cbuf->internal->acquire));

        cbuf->buffer[cbuf->internal->head] = data;
        advance_pointer(cbuf);

        atomic_flag_clear(&cbuf->internal->acquire);
        r = 0;
    }

    return r;
}

int circular_buf_get(cbuf_handle_t cbuf, uint8_t * data)
{
    assert(cbuf && data && cbuf->internal && cbuf->buffer);

    int r = -1;

    if(!circular_buf_empty(cbuf))
    {
        while (atomic_flag_test_and_set(&cbuf->internal->acquire));

        *data = cbuf->buffer[cbuf->internal->tail];
        retreat_pointer(cbuf);

        atomic_flag_clear(&cbuf->internal->acquire);
        r = 0;
    }

    return r;
}

bool circular_buf_empty(cbuf_handle_t cbuf)
{
    assert(cbuf && cbuf->internal);

    while (atomic_flag_test_and_set(&cbuf->internal->acquire));

    bool is_empty = !cbuf->internal->full && (cbuf->internal->head == cbuf->internal->tail);

    atomic_flag_clear(&cbuf->internal->acquire);

    return is_empty;
}

bool circular_buf_full(cbuf_handle_t cbuf)
{
    assert(cbuf && cbuf->internal);

    while (atomic_flag_test_and_set(&cbuf->internal->acquire));

    bool is_full = cbuf->internal->full;

    atomic_flag_clear(&cbuf->internal->acquire);

    return is_full;
}


int circular_buf_get_range(cbuf_handle_t cbuf, uint8_t *data, size_t len)
{
    assert(cbuf && data && cbuf->internal && cbuf->buffer);

    int r = -1;
    size_t size = circular_buf_capacity(cbuf);

    if(circular_buf_size(cbuf) >= len)
    {
        while (atomic_flag_test_and_set(&cbuf->internal->acquire));

        if ( ((cbuf->internal->tail + len) % size) > cbuf->internal->tail)
        {
            memcpy(data, cbuf->buffer + cbuf->internal->tail, len);
        }
        else
        {
            memcpy(data, cbuf->buffer + cbuf->internal->tail, size - cbuf->internal->tail);
            memcpy(data + (size - cbuf->internal->tail), cbuf->buffer, len - (size - cbuf->internal->tail));
        }

        retreat_pointer_n(cbuf, len);

        atomic_flag_clear(&cbuf->internal->acquire);

        r = 0;
    }

    return r;
}

int circular_buf_put_range(cbuf_handle_t cbuf, uint8_t * data, size_t len)
{
    assert(cbuf && cbuf->internal && cbuf->buffer);

    int r = -1;
    size_t size = circular_buf_capacity(cbuf);

    if(!circular_buf_full(cbuf))
    {
        while (atomic_flag_test_and_set(&cbuf->internal->acquire));

        if ( ((cbuf->internal->head + len) % size) > cbuf->internal->head)
        {
            memcpy(cbuf->buffer + cbuf->internal->head, data, len);
        }
        else
        {
            memcpy(cbuf->buffer + cbuf->internal->head, data, size - cbuf->internal->head);
            memcpy(cbuf->buffer, data + (size - cbuf->internal->head), len - (size - cbuf->internal->head));
        }

        advance_pointer_n(cbuf, len);

        atomic_flag_clear(&cbuf->internal->acquire);

        r = 0;
    }

    return r;
}
