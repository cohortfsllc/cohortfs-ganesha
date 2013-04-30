/*
 * Copyright (c) 2013 CohortFS, LLC.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <assert.h>
#include "avl_x.h"

#define AVL_X_REC_MAXPART 23

int avlx_init(struct avl_x *xt, avltree_cmp_fn_t cmpf, uint32_t npart,
              uint32_t flags)
{
    int ix, code = 0;
    pthread_rwlockattr_t rwlock_attr;
    struct avl_x_part *t;

    xt->flags = flags;

    if ((npart > AVL_X_REC_MAXPART) ||
        (npart % 2 == 0)) {
        LogFullDebug(COMPONENT_AVL_CACHE,
                "avlx_init: value %d is an unlikely value for npart "
                "(suggest a small prime)",
                npart);
        }

    if (flags & AVL_X_FLAG_ALLOC)
        xt->tree = gsh_malloc(npart * sizeof(struct avl_x_part));

    /* prior versions of Linux tirpc are subject to default prefer-reader
     * behavior (so have potential for writer starvation) */
    pthread_rwlockattr_init(&rwlock_attr);
#ifdef GLIBC
    pthread_rwlockattr_setkind_np(
        &rwlock_attr, 
        PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif

    xt->npart = npart;

    for (ix = 0; ix < npart; ++ix) {
        t = &(xt->tree[ix]);
        pthread_mutex_init(&t->mtx, NULL);
        pthread_rwlock_init(&t->lock, &rwlock_attr);
        avltree_init(&t->t, cmpf, 0 /* must be 0 */);
    }

    return (code);
}
