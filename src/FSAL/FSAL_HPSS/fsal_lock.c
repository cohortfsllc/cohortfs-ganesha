/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */

/**
 *
 * \file    fsal_lock.c
 * \author  $Author: leibovic $
 * \date    $Date: 2005/07/27 13:30:26 $
 * \version $Revision: 1.2 $
 * \brief   Locking operations.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"

/**
 * FSAL_lock:
 *
 * Called to push a lock into the substrate filesystem.  On success,
 * Cache_inode will consider the lock granted (and thus it will call
 * FSAL_unlock if the SAL transaction commit fails), so a degenerate
 * locking implementation with no push to the underlying filesystem
 * can be constructed simply by returning success from all calls.
 * (Obviously this is not a good idea if you expect the filesystem to
 * be accessed by means other than Ganesha, since it makes the claim
 * of safety while providing none, in that case.)
 *
 * \param descriptor (input):
 *        The open file descriptor associated with the open under
 *        whose auspices the lock is requested.
 * \param offset (input/output):
 *        On input, the position within the file of the first byte to
 *        be locked.  On output, the position within the file of the
 *        first byte of a lock conflicting with the requested lock.
 * \param length (input/output):
 *        On input, the length in bytes of the region to be locked.
 *        On output, the length in bytes of a lock conflicting with
 *        the requested lock.
 * \param type (input/output):
 *        On input, the type (read/write or blocking/non) of the lock
 *        requested.  On output, the type of a lock conflicting with
 *        the requested lock.
 * \param owner (input/output):
 *        On input, an opaque value identifying the entity requesting
 *        the lock.  On output, an opaque value identifying the entity
 *        associated with a conflicting lock.  Two special values may
 *        be filled in by the FSAL: FSAL_EXTERNAL_LOCK_OWNER indicates
 *        that the lock is held by an entity not accessing the file
 *        through Ganesha (thus any identifying information would be
 *        meaningless to a client), and FSAL_INTERNAL_LOCK_OWNER
 *        indicates that the FSAL believes the lock to be held by an
 *        NFS client, but wishes the SAL to infer an owner from its
 *        record of lock state.
 * \param reclaim (input):
 *        This flag indicates that an attempt is being made to reclaim
 *        lock state.  it is currently unused but included for future
 *        implementation of grace and recovery.
 * \param fileinfo (input/output):
 *        This datum may be filled in with anything the FSAL wishes.
 *        It will be passed to future locking calls on the same file.
 * \param promise (output):
 *        This parameter is currently unused, but exists for future
 *        blocking lock support.
 *
 * \return Major error codes:
 *        - ERR_FSAL_NO_ERROR     (no error, lock is granted)
 *        - ERR_FSAL_STALE        (object_handle does not address an existing object)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - ERR_FSAL_CONFLICT     (The lock is held by another
 *                                 client.  The FSAL SHOULD fill in
 *                                 the offset, range, type, and owner
 *                                 to indicate the conflicting lock
 *                                 but MAY leave them untouched to
 *                                 have the SAL pick a conflict to
 *                                 return.)
 *        - ERR_FSAL_RANGE        (An unsupported subrange operation
 *                                 was requested.)
 *        - ERR_FSAL_DEADLOCK     (The requested operation would cause
 *                                 a deadlock.)
 *        - ERR_FSAL_REVOKED      (A lock held by this owner has been
 *                                 revoked by the substrate
 *                                 filesystem. When this error is
 *                                 returned, the FSAL should update
 *                                 offset, length, and type to
 *                                 indicate the full lock revoked.  An
 *                                 offset of 0 and a range of
 *                                 UINT64_MAX indicates that all locks
 *                                 held by the client on the given
 *                                 file have been revoked.)
 *        - Other error codes when something abnormal occurs.
 */

fsal_status_t
HPSSFSAL_lock(xfsfsal_file_t* descriptor, /* IN */
	     fsal_off_t* offset, /* IN/OUT */
	     fsal_size_t* length, /* IN/OUT */
	     fsal_locktype_t* type, /* IN/OUT */
	     fsal_lockowner_t* owner, /* IN/OUT */
	     xfsfsal_filelockinfo_t* fileinfo, /* IN/OUT */
	     fsal_boolean_t reclaim, /* IN */
	     xfsfsal_lockpromise_t* promise /* OUT */
    )
{
    Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_lock);
}

/**
 * FSAL_unlock:
 *
 * Called to free a lock in the substrate filesystem.  On success,
 * Cache_inode will consider the lock freed and free the range in its
 * record of state.  It will also free the range for many errors,
 * however it will NOT free the range if this function returns 
 * ERR_FSAL_FAULT or ERR_FSAL_RANGE.  (If no such lock exists, there
 * is nothing to free, so NOENT and INVAL cases aren't relevant.)
 *
 * \param descriptor (input):
 *        The open file descriptor associated with the open under
 *        whose auspices the lock to be freed is held.
 * \param offset (input):
 *        The position within the file of the first byte to be
 *        unlocked. 
 * \param length (input):
 *        The length in bytes of the region to be unlocked.
 * \param type (input):
 *        The type (read/write or blocking/non) of the lock to be
 *        freed.
 * \param owner (input):
 *        An opaque value identifying the entity associated
 *        with the lock to be freed.
 * \param fileinfo (input/output):
 *        This datum may be filled in with anything the FSAL wishes.
 *        It will be passed to future locking calls on the same file.
 *
 * \return Major error codes :
 *        - ERR_FSAL_NO_ERROR     (no error, region is freed)
 *        - ERR_FSAL_STALE        (object_handle does not address an existing object)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - ERR_FSAL_RANGE        (An unsupported subrange operation
 *                                 was requested.) 
 *        - ERR_FSAL_DEADLOCK     (The requested operation would cause
 *                                 a deadlock.)
 *        - ERR_FSAL_REVOKED      (A lock held by this owner has been
 *                                 revoked by the substrate
 *                                 filesystem. When this error is
 *                                 returned, the FSAL should update
 *                                 offset, length, and type to
 *                                 indicate the full lock revoked.  An
 *                                 offset of 0 and a range of
 *                                 UINT64_MAX indicates that all locks
 *                                 held by the client on the given
 *                                 file have been revoked.)
 *        - Other error codes when something abnormal occurs.
 */


fsal_status_t
HPSSFSAL_unlock(xfsfsal_file_t* descriptor, /* IN */
	       fsal_off_t offset, /* IN */
	       fsal_size_t length, /* IN */
	       fsal_locktype_t type, /* IN */
	       fsal_lockowner_t owner, /* IN */
	       fsal_filelockinfo_t* fileinfo /* IN/OUT */
    )
{
    Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_unlock);
}

/**
 * FSAL_lockt:
 *
 * Called to query the substrate filesystem for a conflicting lock.
 * This function should never modify locking state of the substrate
 * filesystem.
 *
 * \param descriptor (input):
 *        The open file descriptor associated with the open under
 *        whose auspices the lock is to be tested.
 * \param offset (input/output):
 *        On input, the position within the file of the first byte of
 *        the lock to test.  On output, the position within the file
 *        of the first byte of a lock conflicting with the lock to be
 *        tested.
 * \param length (input/output):
 *        On input, the length in bytes of the region specified in the
 *        lock to be tested. On output, the length in bytes of a lock
 *        conflicting with the lock to be tested.
 * \param type (input/output):
 *        On input, the type (read/write or blocking/non) of the lock
 *        to be tested.  On output, the type of a lock conflicting with
 *        the lock to be tested.
 * \param owner (input/output):
 *        On input, an opaque value identifying the entity that would
 *        be associated with the lock to be tested. On output, an
 *        opaque value identifying the entity associated with a
 *        conflicting lock.  Two special values may be filled in by
 *        the FSAL: FSAL_EXTERNAL_LOCK_OWNER indicates that the lock
 *        is held by an entity not accessing the file through Ganesha
 *        (thus any identifying information would be meaningless to a
 *        client), and FSAL_INTERNAL_LOCK_OWNER indicates that the
 *        FSAL believes the lock to be held by an NFS client, but
 *        wishes the SAL to infer an owner from its record of lock
 *        state.
 * \param fileinfo (input/output):
 *        This datum may be filled in with anything the FSAL wishes.
 *        It will be passed to future locking calls on the same file.
 *
 * \return Major error codes:
 *        - ERR_FSAL_NO_ERROR     (no error, lock is granted)
 *        - ERR_FSAL_STALE        (object_handle does not address an existing object)
 *        - ERR_FSAL_FAULT        (a NULL pointer was passed as mandatory argument)
 *        - ERR_FSAL_CONFLICT     (The lock is held by another
 *                                 client.  The FSAL SHOULD fill in
 *                                 the offset, range, type, and owner
 *                                 to indicate the conflicting lock
 *                                 but MAY leave them untouched to
 *                                 have the SAL pick a conflict to
 *                                 return.)
 *        - ERR_FSAL_REVOKED      (A lock held by this owner has been
 *                                 revoked by the substrate
 *                                 filesystem. When this error is
 *                                 returned, the FSAL should update
 *                                 offset, length, and type to
 *                                 indicate the full lock revoked.  An
 *                                 offset of 0 and a range of
 *                                 UINT64_MAX indicates that all locks
 *                                 held by the client on the given
 *                                 file have been revoked.)
 *        - Other error codes when something abnormal occurs.
 */


fsal_status_t
HPSSFSAL_lockt(xfsfsal_file_t* descriptor, /* IN */
	      fsal_off_t* offset, /* IN/OUT */
	      fsal_size_t* length, /* IN/OUT */
	      fsal_locktype_t* type, /* IN/OUT */
	      fsal_lockowner_t* owner, /* IN/OUT */
	      fsal_filelockinfo_t* fileinfo /* IN/OUT */
    )
{
    Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_lockt);
}

