/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Portions Copyright CEA/DAM/DIF  (2008)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
 *
 * Portions Copyright (C) 2012, The Linux Box Corporation
 * Contributor : Matt Benjamin <matt@linuxbox.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * -------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _SOLARIS
#include "solaris_port.h"
#endif                          /* _SOLARIS */

#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>

/* XXX prune: */
#include "log.h"
#include "nfs_core.h"
#include "nfs23.h"
#include "nfs4.h"
#include "fsal.h"
#include "nfs_tools.h"

#include "nfs_dupreq.h"
#include "murmur3.h"
#include "abstract_mem.h"
#include "gsh_intrinsic.h"

pool_t *dupreq_pool;
pool_t *nfs_res_pool;
pool_t *tcp_drc_pool; /* pool of per-connection DRC objects */

static struct {
    pthread_mutex_t mtx;
    /* shared DRC */
    drc_t udp_drc;
    /* recycle queues for per-connection DRCs */
    struct rbtree_x tcp_drc_recycle_t;
    struct opr_queue tcp_drc_recycle_q; /* fifo */
    int32_t tcp_drc_recycle_qlen;
    int32_t tcp_drc_hiwat;
} drc_st;

/**
 * @brief Comparison function for duplicate request entries.
 *
 * @return Nothing.
 */
static inline int
uint32_cmpf(uint32_t lhs, uint32_t rhs)
{
    if (lhs < rhs)
        return (-1);

    if (lhs == rhs)
        return 0;

    return (1);
}

/**
 * @brief Comparison function for entries in a shared DRC
 *
 * @param[in] lhs  Left-hand-side
 * @param[in] rhs  Right-hand-side
 *
 * @return -1,0,1.
 */
static inline int
dupreq_shared_cmpf(const struct opr_rbtree_node *lhs,
                   const struct opr_rbtree_node *rhs)
{
    dupreq_entry_t *lk, *rk;

    lk = opr_containerof(lhs, dupreq_entry_t, rbt_k);
    rk = opr_containerof(rhs, dupreq_entry_t, rbt_k);

    switch (cmp_sockaddr(&lk->hin.addr, &rk->hin.addr, CHECK_PORT)) {
    case -1:
        return -1;
        break;
    case 0:
        switch (uint32_cmpf(lk->hin.tcp.rq_xid, rk->hin.tcp.rq_xid)) {
        case -1:
            return (-1);
            break;
        case 0:
            if (lk->hin.drc->flags & DRC_FLAG_CKSUM) {
                return (memcmp(lk->hin.tcp.checksum, rk->hin.tcp.checksum,
                               sizeof(lk->hin.tcp.checksum)));
            }
            else
                return (0);            
            break;
        default:
            break;
        } /* xid */
        break;
    default:
            break;
    } /* addr+port */

    return (1);
}

/**
 * @brief Comparison function for entries in a per-connection (TCP) DRC
 *
 * @param[in] lhs  Left-hand-side
 * @param[in] rhs  Right-hand-side
 *
 * @return -1,0,1.
 */
static inline int
dupreq_tcp_cmpf(const struct opr_rbtree_node *lhs,
                const struct opr_rbtree_node *rhs)
{
    dupreq_entry_t *lk, *rk;

    lk = opr_containerof(lhs, dupreq_entry_t, rbt_k);
    rk = opr_containerof(rhs, dupreq_entry_t, rbt_k);

    if (lk->hin.tcp.rq_xid < rk->hin.tcp.rq_xid)
        return (-1);

    if (lk->hin.tcp.rq_xid == rk->hin.tcp.rq_xid) {
        if (lk->hin.drc->flags & DRC_FLAG_CKSUM) {
            return (memcmp(lk->hin.tcp.checksum, rk->hin.tcp.checksum,
                           sizeof(lk->hin.tcp.checksum)));
        } else
            return (0);
    }

    return (1);
}

/**
 * @brief Comparison function for recycled per-connection (TCP) DRCs
 *
 * @param[in] lhs  Left-hand-side
 * @param[in] rhs  Right-hand-side
 *
 * @return -1,0,1.
 */
static inline int
drc_recycle_cmpf(const struct opr_rbtree_node *lhs,
                 const struct opr_rbtree_node *rhs)
{
    drc_t *lk, *rk;

    lk = opr_containerof(lhs, drc_t, d_u.tcp.recycle_k);
    rk = opr_containerof(rhs, drc_t, d_u.tcp.recycle_k);

    return (cmp_sockaddr(&lk->d_u.tcp.addr, &rk->d_u.tcp.addr,
                         CHECK_PORT));
}

/**
 * @brief Hash function for entries in a shared DRC
 *
 * @param[in] drc  DRC
 * @param[in] arg  The request arguments
 * @param[in] v    The duplicate request entry being hashed
 *
 * The checksum step is conditional on drc->flags.  Note that
 * Oracle DirectNFS and other clients are believed to produce
 * workloads that may fail without checksum support.
 *
 * @return the (definitive) 64-bit hash value as a uint64_t.
 */
static inline uint64_t
drc_shared_hash(drc_t *drc, nfs_arg_t *arg, dupreq_entry_t *v)
{
    if (drc->flags & DRC_FLAG_CKSUM) {
        MurmurHash3_x64_128(arg, 200, 911, v->hin.tcp.checksum);
        MurmurHash3_x64_128(&v->hin, sizeof(v->hin), 911, v->hk);
    } else
        MurmurHash3_x64_128(&v->hin, sizeof(v->hin)-sizeof(v->hin.tcp.checksum),
                            911, v->hk);
    return (v->hk[0]);
}

/**
 * @brief Hash function for entries in a per-connection (TCP) DRC
 *
 * @param[in] drc  DRC
 * @param[in] arg  The request arguments
 * @param[in] v    The duplicate request entry being hashed
 *
 * The hash and checksum steps is conditional on drc->flags.  Note
 * that Oracle DirectNFS and other clients are believed to produce
 * workloads that may fail without checksum support.
 *
 * HOWEVER!  We might omit the address component of the hash here,
 * probably should for performance.
 *
 * @return the (definitive) 64-bit hash value as a uint64_t.
 */
static inline uint64_t
drc_tcp_hash(drc_t *drc, nfs_arg_t *arg, dupreq_entry_t *v)
{
    if (drc->flags & DRC_FLAG_HASH) {
        if (drc->flags & DRC_FLAG_CKSUM) {
            MurmurHash3_x64_128(arg, 200, 911, v->hin.tcp.checksum);
            MurmurHash3_x64_128(&v->hin, sizeof(v->hin), 911, v->hk);
        } else
            MurmurHash3_x64_128(&v->hin, sizeof(v->hin), 911, v->hk);
    } else
        v->hk[0] = v->hin.tcp.rq_xid;
    return (v->hk[0]);
}

/**
 * @brief Initialize a shared duplicate request cache
 *
 * @param[in] drc  The cache
 * @param[in] npart Number of concurrent partitions (a number > 1 is suitable
 * for a shared DRC) 
 * @param[in] cachesz Number of entries in the closed table (not really a cache)
 *
 * @return Nothing.
 */
static inline void
init_shared_drc(drc_t *drc, uint32_t npart, uint32_t maxsz, uint32_t cachesz,
         uint32_t flags)
{
    int code __attribute__((unused)) = 0;
    int ix;

    pthread_mutex_init(&drc->mtx, NULL);
    drc->npart = npart;
    drc->maxsize = maxsz;
    drc->retwnd = 0;

    /* init dict */
    code = rbtx_init(&drc->xt, drc_recycle_cmpf, npart, RBT_X_FLAG_ALLOC);
    /* XXX error? */
 
    /* init closed-form "cache" partition */
    for (ix = 0; ix < drc->npart; ++ix) {
        struct rbtree_x_part *xp = &(drc->xt.tree[ix]);
        xp->cache = gsh_calloc(cachesz, sizeof(struct opr_rbtree_node *));
        if (unlikely(! xp->cache)) {
            LogCrit(COMPONENT_DUPREQ, "alloc TCP DRC hash partition failed");
            drc->cachesz = cachesz;
        } else
            drc->cachesz = 0;            
    }
    drc->flags = flags;
    /* completed requests */
    opr_queue_Init(&drc->dupreq_q);
    /* recycling DRC */
    opr_queue_Init(&drc->d_u.tcp.recycle_q);
}

/**
 * @brief Initialize the DRC package.
 *
 * @return Nothing.
 */
void dupreq2_pkginit(void)
{
    int code __attribute__((unused)) = 0;

    dupreq_pool = pool_init("Duplicate Request Pool",
                            sizeof(dupreq_entry_t), NULL, NULL);
    if (unlikely(! (dupreq_pool))) {
        LogCrit(COMPONENT_INIT,
                "Error while allocating duplicate request pool");
        LogError(COMPONENT_INIT, ERR_SYS, ERR_MALLOC, errno);
        Fatal();
    }

    nfs_res_pool = pool_init("nfs_res_t pool",
                             sizeof(nfs_res_t), NULL, NULL);
    if (unlikely(! (nfs_res_pool))) {
        LogCrit(COMPONENT_INIT,
                "Error while allocating nfs_res_t pool");
        LogError(COMPONENT_INIT, ERR_SYS, ERR_MALLOC, errno);
        Fatal();
    }

    tcp_drc_pool = pool_init("TCP DRC Pool",
                             sizeof(drc_t), NULL, NULL);
    if (! (dupreq_pool)) {
        LogCrit(COMPONENT_INIT,
                "Error while allocating duplicate request pool");
        LogError(COMPONENT_INIT, ERR_SYS, ERR_MALLOC, errno);
        Fatal();
    }

    /* init shared statics */
    pthread_mutex_init(&drc_st.mtx, NULL);

    /* recycle_t */
    code = rbtx_init(&drc_st.tcp_drc_recycle_t, drc_recycle_cmpf, 17,
                     RBT_X_FLAG_ALLOC);
    /* XXX error? */
    /* init recycle_q */
    opr_queue_Init(&drc_st.tcp_drc_recycle_q);
    drc_st.tcp_drc_recycle_qlen = 0;
    drc_st.tcp_drc_hiwat = 1024; /* parameterize */

    /* UDP DRC is global, shared */
    init_shared_drc(&drc_st.udp_drc, 17, 24575, 32767,
                    DRC_FLAG_HASH|DRC_FLAG_CKSUM);
}

extern nfs_function_desc_t nfs2_func_desc[];
extern nfs_function_desc_t nfs3_func_desc[];
extern nfs_function_desc_t nfs4_func_desc[];
extern nfs_function_desc_t mnt1_func_desc[];
extern nfs_function_desc_t mnt3_func_desc[];
#ifdef _USE_NLM
extern nfs_function_desc_t nlm4_func_desc[];
#endif                          /* _USE_NLM */
#ifdef _USE_RQUOTA
extern nfs_function_desc_t rquota1_func_desc[];
extern nfs_function_desc_t rquota2_func_desc[];
#endif                          /* _USE_QUOTA */

/**
 * @brief Determine the protocol of the supplied TI-RPC SVCXPRT*
 *
 * @param[in] xprt  The SVCXPRT
 *
 * @return IPPROTO_UDP or IPPROTO_TCP.
 */
static inline unsigned int
get_ipproto_by_xprt(SVCXPRT *xprt) /* XXX correct, but inelegant */
{
   if( xprt->xp_p2 != NULL )
     return IPPROTO_UDP ;
   else if ( xprt->xp_p1 != NULL )
     return IPPROTO_TCP;
   else
     return IPPROTO_IP ; /* Dummy output */
}

/**
 * @brief Determine the dupreq2 DRC type to handle the supplied svc_req
 *
 * @param[in] req  The svc_req being processed
 *
 * @return a value of type enum_drc_type.
 */
static inline enum drc_type
get_drc_type(struct svc_req *req)
{
    if (get_ipproto_by_xprt(req->rq_xprt) == IPPROTO_UDP)
        return (DRC_UDP_V234);
    else {
        if (req->rq_vers == 4)
            return (DRC_TCP_V4);
    }
    return (DRC_TCP_V3);
}

/**
 * @brief Allocate a duplicate request cache
 *
 * @param[in] dtype  Style DRC to allocate (e.g., TCP, by enum drc_type)
 * @param[in] maxsz  Upper bound on requests to cache
 * @param[in] cachesz  Number of entries in the closed hash partition
 * @param[in] flags  DRC flags
 *
 * @return the drc, if successfully allocated, else NULL.
 */
static inline drc_t *
alloc_tcp_drc(enum drc_type dtype, uint32_t maxsz, uint32_t cachesz, uint32_t flags)
{
    drc_t *drc = pool_alloc(tcp_drc_pool, NULL);
    int npart, code  __attribute__((unused)) = 0;

    if (unlikely(! drc)) {
        LogCrit(COMPONENT_DUPREQ, "alloc TCP DRC failed");
        goto out;
    }

    switch (dtype) {
    case DRC_UDP_V234:
        npart = 17;
        break;
    case DRC_TCP_V4:
    case DRC_TCP_V3:
    default:
        npart = 1;
        break;
    };

    drc->type = dtype;
    drc->flags = flags;
    drc->maxsize = maxsz;

    pthread_mutex_init(&drc->mtx, NULL);

    /* init dict */
    code = rbtx_init(&drc->xt, dupreq_tcp_cmpf, npart, RBT_X_FLAG_ALLOC);
    /* XXX error? */

    drc->xt.tree[0].cache =
        gsh_calloc(sizeof(struct opr_rbtree_node*), cachesz);
    if (unlikely(! drc->xt.tree[0].cache)) {
        LogCrit(COMPONENT_DUPREQ, "allocation of TCP DRC hash partition failed "
                "(continuing)");
        drc->cachesz = 0;
    } else
        drc->cachesz = cachesz;

    /* completed requests */
    opr_queue_Init(&drc->dupreq_q);
    /* recycling DRC */
    opr_queue_Init(&drc->d_u.tcp.recycle_q);

    drc->refcnt = 0;
    drc->usecnt = 0;
    drc->retwnd = 0;
    drc->d_u.tcp.recycle_time = 0;
    drc->flags = DRC_FLAG_HASH|DRC_FLAG_CKSUM;

out:
    return (drc);
}

/**
 * @brief Deep-free a per-connection (TCP) duplicate request cache
 *
 * @param[in] drc  The DRC to dispose
 *
 * Assumes that the DRC has been allocated from the tcp_drc_pool.
 *
 * @return Nothing.
 */
static inline void
free_tcp_drc(drc_t *drc) {
    if (drc->xt.tree[0].cache)
        gsh_free(drc->xt.tree[0].cache);
    pthread_mutex_destroy(&drc->mtx);
    pool_free(tcp_drc_pool, drc);
}

/**
 * @brief Increment the reference count on a DRC
 *
 * @param[in] drc  The DRC to ref
 *
 * @return the new value of refcnt.
 */
static inline uint32_t
nfs_dupreq_ref_drc(drc_t *drc)
{
    return (++(drc->refcnt)); /* locked */
}

/**
 * @brief Decrement the reference count on a DRC
 *
 * @param[in] drc  The DRC to unref
 *
 * @return the new value of refcnt.
 */
static inline uint32_t
nfs_dupreq_unref_drc(drc_t *drc)
{
    return (--(drc->refcnt)); /* locked */
}

/**
 * @brief Find and reference a DRC to process the supplied svc_req.
 *
 * @param[in] req  The svc_req being processed.
 *
 * @return The ref'd DRC if sucessfully located, else NULL.
 */
static inline drc_t *
nfs_dupreq_get_drc(struct svc_req *req)
{
    enum drc_type dtype = get_drc_type(req);
    gsh_xprt_private_t *xu = (gsh_xprt_private_t *) req->rq_xprt->xp_u1;
    drc_t *drc = NULL;

    switch (dtype) {
    case DRC_UDP_V234:
        drc = &(drc_st.udp_drc);
        pthread_mutex_lock(&drc_st.mtx);
        (void) nfs_dupreq_ref_drc(drc);
        pthread_mutex_unlock(&drc_st.mtx);
        goto out;
        break;
    case DRC_TCP_V4:
    case DRC_TCP_V3:
        pthread_rwlock_wrlock(&req->rq_xprt->lock);
        if (xu->drc) {
            drc = xu->drc;
            pthread_mutex_lock(&drc->mtx); /* LOCKED */
        } else {
            drc_t drc_k;
            struct rbtree_x_part *t;
            struct opr_rbtree_node *ndrc;
            drc_t *tdrc;

            drc_k.type = dtype;
            (void) copy_xprt_addr(&drc_k.d_u.tcp.addr, req->rq_xprt);
            MurmurHash3_x64_128(&drc_k.d_u.tcp.addr, sizeof(sockaddr_t),
                                911, &drc_k.d_u.tcp.hk);
            t = rbtx_partition_of_scalar(&drc_st.tcp_drc_recycle_t,
                                         drc_k.d_u.tcp.hk[0]);
            pthread_mutex_lock(&t->mtx);
            ndrc = opr_rbtree_lookup(&t->t, &drc_k.d_u.tcp.recycle_k);
            if (ndrc) {
                tdrc = opr_containerof(ndrc, drc_t, d_u.tcp.recycle_k);
                if (tdrc) {
                    /* ok, tdrc exists and has refcnt >= 0 (ie, 1), we may now
                     * recycle it */
                    pthread_mutex_lock(&tdrc->mtx); /* LOCKED */
                    (void) opr_rbtree_remove(&t->t, &tdrc->d_u.tcp.recycle_k);
                    opr_queue_Remove(&tdrc->d_u.tcp.recycle_q);
                    pthread_mutex_unlock(&t->mtx);
                    --(drc_st.tcp_drc_recycle_qlen);
                    drc = tdrc;
                }
            }
            if (! drc) {
                drc = alloc_tcp_drc(dtype, 127, 127,
                                    DRC_FLAG_HASH|DRC_FLAG_CKSUM);

                /* assign addr */
                memcpy(&drc->d_u.tcp.addr, &drc_k.d_u.tcp.addr,
                       sizeof(sockaddr_t));
                /* assign already-computed hash */
                memcpy(drc->d_u.tcp.hk, drc_k.d_u.tcp.hk, 128);
                pthread_mutex_lock(&drc->mtx); /* LOCKED */
            }
            drc->d_u.tcp.recycle_time = 0;
            /* xprt drc */
            ++(drc->usecnt);
            (void) nfs_dupreq_ref_drc(drc); /* xu ref */
            xu->drc = drc;
        }
        pthread_rwlock_unlock(&req->rq_xprt->lock);
        break;
    default:
        /* XXX error */
        break;
    }

    /* call path ref */
    (void) nfs_dupreq_ref_drc(drc);
    pthread_mutex_unlock(&drc->mtx);

out:
    return (drc);
}

/**
 * @brief Release and previously-ref'd DRC, freeing it if its refcnt drops to 0
 *
 * @param[in] xprt  The SVCXPRT associated with DRC, if applicable
 * @param[in] drc  The DRC.
 *
 * @return Nothing.
 */
void
nfs_dupreq_put_drc(SVCXPRT *xprt, drc_t *drc, uint32_t flags)
{
    gsh_xprt_private_t *xu = (gsh_xprt_private_t *) xprt->xp_u1;

    pthread_rwlock_wrlock(&xprt->lock);

    if (! (flags & DRC_FLAG_LOCKED))
        pthread_mutex_lock(&drc->mtx);
    /* drc LOCKED */

    nfs_dupreq_unref_drc(drc);

    switch (drc->type) {
    case DRC_UDP_V234:
        /* do nothing */
        break;
    case DRC_TCP_V4:
    case DRC_TCP_V3:
        if (drc->refcnt == 0) {
            struct rbtree_x_part *t;
            struct opr_rbtree_node *odrc;

            xu->drc = NULL;
            --(drc->usecnt); /* XXX should be 0 */

            /* note t's lock order wrt drc->mtx is the opposite
             * of drc->xt[*].lock */
            t = rbtx_partition_of_scalar(&drc_st.tcp_drc_recycle_t,
                                         drc->d_u.tcp.hk[0]);

            pthread_mutex_lock(&t->mtx);
            odrc = opr_rbtree_lookup(&t->t, &drc->d_u.tcp.recycle_k);
            if (! odrc) {
                /* XXX no DRC is currently in the recycle queue, so
                 * we enqueue this one */
                drc->d_u.tcp.recycle_time = time(NULL);
                /* insert dict */
                opr_rbtree_insert(&t->t, &drc->d_u.tcp.recycle_k);
                /* insert q */
                opr_queue_Append(
                    &drc_st.tcp_drc_recycle_q, &drc->d_u.tcp.recycle_q);
                ++(drc_st.tcp_drc_recycle_qlen);

            } else {
                pthread_mutex_unlock(&drc->mtx); /* !LOCKED */
                free_tcp_drc(drc);
                goto out;
            }
            pthread_mutex_unlock(&t->mtx);
        }
    default:        
        break;
    };

    pthread_mutex_unlock(&drc->mtx); /* !LOCKED */

out:
    pthread_rwlock_unlock(&xprt->lock);

    return;
}

/**
 * @brief Resolve an indirect request function vector for the supplied DRC entry
 *
 * @param[in] dv  The duplicate request entry.
 *
 * @return The function vector if successful, else NULL.
 */
static inline nfs_function_desc_t*
nfs_dupreq_func(dupreq_entry_t *dv)
{
    nfs_function_desc_t *func = NULL;

    if(dv->hin.rq_prog == nfs_param.core_param.program[P_NFS]) {
        switch (dv->hin.rq_vers) {
        case NFS_V2:
            func = &nfs2_func_desc[dv->hin.rq_proc];
          break;
        case NFS_V3:
            func = &nfs3_func_desc[dv->hin.rq_proc];
            break;
        case NFS_V4:
            func = &nfs4_func_desc[dv->hin.rq_proc];
          break;
        default:
            /* not reached */
            LogMajor(COMPONENT_DUPREQ,
                     "NFS Protocol version %d unknown",
                     (int) dv->hin.rq_vers);
        }
    }
    else if(dv->hin.rq_prog == nfs_param.core_param.program[P_MNT]) {
        switch (dv->hin.rq_vers) {
        case MOUNT_V1:
            func = &mnt1_func_desc[dv->hin.rq_proc];
          break;
        case MOUNT_V3:
            func = &mnt3_func_desc[dv->hin.rq_proc];
            break;
        default:
            /* not reached */
            LogMajor(COMPONENT_DUPREQ,
                     "MOUNT Protocol version %d unknown",
                     (int) dv->hin.rq_vers);
          break;
        }
    }
#ifdef _USE_NLM
    else if(dv->hin.rq_prog == nfs_param.core_param.program[P_NLM]) {
        switch (dv->hin.rq_vers) {
        case NLM4_VERS:
            func = &nlm4_func_desc[dv->hin.rq_proc];
            break;
        }
    }
#endif /* _USE_NLM */
#ifdef _USE_RQUOTA
    else if(dv->hin.rq_prog == nfs_param.core_param.program[P_RQUOTA]) {
        switch (dv->hin.rq_vers) {
        case RQUOTAVERS:
            func = &rquota1_func_desc[dv->hin.rq_proc];
            break;
        case EXT_RQUOTAVERS:
            func = &rquota2_func_desc[dv->hin.rq_proc];
          break;
        }
    }
#endif
    else {
        /* not reached */
        LogMajor(COMPONENT_DUPREQ,
                 "protocol %d is not managed",
                 (int) dv->hin.rq_prog);
    }

    return (func);
}

/**
 * @brief Construct a duplicate request cache entry.
 *
 * Entries are allocated from the dupreq_pool.  Since dupre_entry_t
 * presently contains an expanded nfs_arg_t, zeroing of at least corresponding
 * value pointers is required for XDR allocation.
 *
 * @return Nothing.
 */
static inline dupreq_entry_t *
alloc_dupreq(void)
{
    dupreq_entry_t *dv;

    dv = pool_alloc(dupreq_pool, NULL);
    if (! dv) {
        LogCrit(COMPONENT_DUPREQ, "alloc dupreq_entry_t failed");
        goto out;
    }
    memset(dv, 0, sizeof(dupreq_entry_t)); /* XXX pool_zalloc */

    pthread_mutex_init(&dv->mtx, NULL);
    opr_queue_Init(&dv->fifo_q);

out:
    return (dv);
}

/**
 * @brief Deep-free a duplicate request cache entry.
 *
 * If the entry has processed request data, the corresponding free
 * function is called on the result.  The cache entry is then returned
 * to the dupreq_pool.
 *
 * @return Nothing.
 */
static inline void
nfs_dupreq_free_dupreq(dupreq_entry_t *dv)
{
    nfs_function_desc_t *func;

    if (dv->res) {
        func = nfs_dupreq_func(dv);
        func->free_function(dv->res);
        free_nfs_res(dv->res);
    }
    pthread_mutex_destroy(&dv->mtx);
    pool_free(dupreq_pool, dv);
}

/*
 * DRC request retire heuristic.
 * 
 * We add a new, per-drc semphore like counter, retwnd.  The value of
 * retwnd begins at 0, and is always >= 0.  The value of retwnd is increased
 * when a a duplicate req cache hit occurs.  If it was 0, it is increased by
 * some small constant, say, 16, otherwise, by 1.  And retwnd decreases by 1
 * when we successfully finish any request.  Likewise in finish, a cached
 * request may be retired iff we are above our water mark, and retwnd is 0.
 */

#define RETWND_START_BIAS 16

/**
 * @brief advance retwnd.
 *
 * If (drc)->retwnd is 0, advance its value to RETWND_START_BIAS, else
 * increase its value by 1.
 *
 * @param drc [IN] The duplicate request cache
 *
 * @return Nothing.
 */
#define drc_inc_retwnd(drc) \
    do { \
    if ((drc)->retwnd == 0) \
        (drc)->retwnd = RETWND_START_BIAS; \
    else \
        ++((drc)->retwnd); \
    } while (0);

/**
 * @brief conditionally decrement retwnd.
 *
 * If (drc)->retwnd > 0, decrease its value by 1.
 *
 * @param drc [IN] The duplicate request cache
 *
 * @return Nothing.
 */
#define drc_dec_retwnd(drc) \
    do { \
    if ((drc)->retwnd > 0) \
        --((drc)->retwnd); \
    } while (0);

/**
 * @brief retire request predicate.
 *
 * Calculate whether a request may be retired from the provided duplicate
 * request cache.
 *
 * @param drc [IN] The duplicate request cache
 *
 * @return TRUE if a request may be retired, else FALSE.
 */
static inline bool
drc_should_retire(drc_t *drc)
{
    /* do not exeed the hard bound on cache size */
    if (unlikely((drc)->size > drc_st.tcp_drc_hiwat))
        return (TRUE);

    /* otherwise, are we permitted to retire requests */
    if (unlikely(drc->retwnd > 0))
        return (FALSE);

    /* finally, retire if drc->size is above intended high water mark */
    if (drc->size > drc->maxsize)
        return (TRUE);

    return (FALSE);
}

/**
 *
 * nfs_dupreq_add_not_finished: adds an entry in the duplicate request cache.
 *
 * Adds an entry to the correct duplicate request cache.
 *
 * @param req [IN] the request to be cached
 * @param arg [IN] pointer to the called-with arguments
 * @param res_nfs [IN] pointer to the result to cache
 *
 * @return DUPREQ_SUCCESS if successful.
 * @return DUPREQ_INSERT_MALLOC_ERROR if an error occured during insertion.
 *
 */
dupreq_status_t
nfs_dupreq_add_not_finished(struct svc_req *req, nfs_arg_t *arg_nfs,
                            nfs_res_t **res_nfs)
{
    dupreq_status_t status = DUPREQ_SUCCESS;
    drc_t *drc = nfs_dupreq_get_drc(req);
    dupreq_entry_t *dv, *dk = alloc_dupreq();
    bool release_dk = TRUE;

    dk->hin.drc = drc; /* trans. call path ref to dv */

    switch (drc->type) {
    case DRC_TCP_V4:
    case DRC_TCP_V3:
        dk->hin.tcp.rq_xid = req->rq_xid;
        /* XXX needed? */
        dk->hin.rq_prog = req->rq_prog;
        dk->hin.rq_vers = req->rq_vers;
        dk->hin.rq_proc = req->rq_proc;
        break;
    case DRC_UDP_V234:
        dk->hin.tcp.rq_xid = req->rq_xid;
        if (unlikely(! copy_xprt_addr(&dk->hin.addr, req->rq_xprt))) {
            status = DUPREQ_INSERT_MALLOC_ERROR;
            goto out;
        }
        dk->hin.rq_prog = req->rq_prog;
        dk->hin.rq_vers = req->rq_vers;
        dk->hin.rq_proc = req->rq_proc;
        break;
    default:
        /* error */
        break;
    }

    switch (drc->type) {
    case DRC_UDP_V234:
        (void) drc_shared_hash(drc, arg_nfs, dk);
        break;
    case DRC_TCP_V3:
    case DRC_TCP_V4:
        (void) drc_tcp_hash(drc, arg_nfs, dk);
        break;
        default:
            /* error */
            break;
    }

    dk->state = DUPREQ_START;
    dk->timestamp = time(NULL);

    {
        struct opr_rbtree_node *nv;
        struct rbtree_x_part *t =
            rbtx_partition_of_scalar(&drc->xt, dk->hk[0]);
        pthread_mutex_lock(&t->mtx); /* partition lock */
        nv = rbtree_x_cached_lookup(&drc->xt, t, &dk->rbt_k, dk->hk[0]);
        if (nv) {
            /* cached request */
            dv = opr_containerof(nv, dupreq_entry_t, rbt_k);
            if (unlikely(dv->state == DUPREQ_START)) {
                /* XXX reached? */
                status = DUPREQ_BEING_PROCESSED;
            } else {
                /* satisfy req from the DC, extend window */
                *res_nfs = dv->res;
                pthread_mutex_lock(&drc->mtx);
                drc_inc_retwnd(drc);
                pthread_mutex_unlock(&drc->mtx);
                status = DUPREQ_ALREADY_EXISTS;
            }
        } else {
            /* new request */
            *res_nfs = dk->res = alloc_nfs_res();
            (void) rbtree_x_cached_insert_wt(&drc->xt, t, &dk->rbt_k,
                                             dk->hk[0]);
            /* add to q tail */
            pthread_mutex_lock(&drc->mtx);
            opr_queue_Append(&drc->dupreq_q, &dk->fifo_q);
            ++(drc->size);
            pthread_mutex_unlock(&drc->mtx);
            req->rq_u1 = dk;
            release_dk = FALSE;
        }
        pthread_mutex_unlock(&t->mtx);
    }

out:
    if (release_dk) {
        nfs_dupreq_put_drc(req->rq_xprt, drc, DRC_FLAG_NONE); /* dk ref */
        nfs_dupreq_free_dupreq(dk);
    }

    return (status);
}

/**
 *
 * @brief Completes a request in the cache
 *
 * Completes a cache insertion operation begun in nfs_dupreq_add_not_finished.
 *
 * We assert req->rq_u1 now points to the corresonding duplicate request
 * cache entry.
 *
 * @param xid [IN] the transfer id to be used as key
 * @param pnfsreq [IN] the request pointer to cache
 *
 * @return DUPREQ_SUCCESS if successful.
 * @return DUPREQ_INSERT_MALLOC_ERROR if an error occured.
 *
 */
dupreq_status_t
nfs_dupreq_finish(struct svc_req *req,  nfs_res_t *res_nfs)
{
    dupreq_entry_t *ov = NULL, *dv = (dupreq_entry_t *) req->rq_u1;
    dupreq_status_t status = DUPREQ_SUCCESS;
    struct rbtree_x_part *t;
    struct opr_queue *qn;
    drc_t *drc = NULL;

    pthread_mutex_lock(&dv->mtx);
    dv->res = res_nfs;
    dv->timestamp = time(NULL);
    dv->state = DUPREQ_COMPLETE;
    drc = dv->hin.drc;
    pthread_mutex_unlock(&dv->mtx);

    /* cond. remove from q head */
    pthread_mutex_lock(&drc->mtx);

    /* ok, do the new retwnd calculation here.  then, put drc only if
     * we retire an entry */
    if (drc_should_retire(drc)) {
        qn = opr_queue_First(&drc->dupreq_q, dupreq_entry_t, fifo_q);
        if (likely(qn)) {
            ov = opr_queue_Entry(qn, dupreq_entry_t, fifo_q);
            /* remove q entry */
            opr_queue_Remove(&ov->fifo_q);
            /* remove dict entry */
            t = rbtx_partition_of_scalar(&drc->xt, ov->hk[0]);
            rbtree_x_cached_remove_wt(&drc->xt, t, &ov->rbt_k, ov->hk[0]);
            --(drc->size);
            /* deep free ov */
            nfs_dupreq_free_dupreq(ov);
            drc_dec_retwnd(drc);
            nfs_dupreq_put_drc(req->rq_xprt, drc, DRC_FLAG_LOCKED);
            goto out;
        }
    }

    /* always adjust retwnd */
    drc_dec_retwnd(drc)

    pthread_mutex_unlock(&drc->mtx);

out:
    return (status);
}

/**
 *
 * @brief Remove an entry (request) from a duplicate request cache.
 *
 * We assert req->rq_u1 now points to the corresonding duplicate request
 * cache entry.
 *
 * @param req [IN] The svc_req structure.
 *
 * @return DUPREQ_SUCCESS if successful.
 *
 */
dupreq_status_t
nfs_dupreq_delete(struct svc_req *req)
{
    dupreq_entry_t *dv = (dupreq_entry_t *) req->rq_u1;
    dupreq_status_t status = DUPREQ_SUCCESS;
    struct rbtree_x_part *t;
    drc_t *drc;

    pthread_mutex_lock(&dv->mtx);
    drc = dv->hin.drc;
    dv->state = DUPREQ_DELETED;
    pthread_mutex_unlock(&dv->mtx);

    /* XXX dv holds a ref on drc */
    t = rbtx_partition_of_scalar(&drc->xt, dv->hk[0]);

    pthread_mutex_lock(&t->mtx);
    rbtree_x_cached_remove_wt(&drc->xt, t, &dv->rbt_k, dv->hk[0]);

    /* t->mtx also protects drc's fifo q, on which we assert dv is
     * enqueued */
    if (opr_queue_IsOnQueue(&dv->fifo_q))
        opr_queue_Remove(&dv->fifo_q);

    pthread_mutex_unlock(&t->mtx);

    /* deep free */
    nfs_dupreq_free_dupreq(dv);

    pthread_mutex_lock(&drc->mtx);
    --(drc->size);

    /* release dv's ref */
    nfs_dupreq_put_drc(req->rq_xprt, drc, DRC_FLAG_LOCKED);
    /* !LOCKED */

    return (status);
}

/**
 * @brief Shutdown the dupreq2 package.
 *
 * @return Nothing.
 */
void dupreq2_pkgshutdown(void)
{
    /* XXX do nothing */
}
