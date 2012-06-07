/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright CEA/DAM/DIF  (2008)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * ---------------------------------------
 */

/**
 * \file  nfs_proto_tools.c
 * \brief A set of functions used to managed NFS.
 *
 * Helper functions to work with NFS protocol objects.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _SOLARIS
#include "solaris_port.h"
#endif

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/file.h>           /* for having FNDELAY */
#include <pwd.h>
#include <grp.h>
#include <stdint.h>
#include "HashData.h"
#include "HashTable.h"
#include "log.h"
#include "ganesha_rpc.h"
#include "nfs23.h"
#include "nfs4.h"
#include "mount.h"
#include "nfs_core.h"
#include "cache_inode.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_tools.h"
#include "nfs_file_handle.h"
#include "nfs_proto_tools.h"
#include "nfs4_acls.h"
#ifdef _PNFS_MDS
#include "sal_data.h"
#include "sal_functions.h"
#include "fsal.h"
#include "fsal_pnfs.h"
#include "pnfs_common.h"
#endif /* _PNFS_MDS */

#ifdef _USE_NFS4_ACL
/**
 * Define mapping of NFS4 who name and type.
 */
static struct {
  char *string;
  int   stringlen;
  int type;
} whostr_2_type_map[] = {
  {
    .string    = "OWNER@",
    .stringlen = sizeof("OWNER@") - 1,
    .type      = FSAL_ACE_SPECIAL_OWNER,
  },
  {
    .string    = "GROUP@",
    .stringlen = sizeof("GROUP@") - 1,
    .type      = FSAL_ACE_SPECIAL_GROUP,
  },
  {
    .string    = "EVERYONE@",
    .stringlen = sizeof("EVERYONE@") - 1,
    .type      = FSAL_ACE_SPECIAL_EVERYONE,
  },
};
#endif                          /* _USE_NFS4_ACL */

/**
 * @brief Converts a file handle to a string representation
 *
 * This function converts a file handle to a string representation.
 *
 * @param[in]  rq_vers Version of the NFS protocol to be used
 * @param[in]  fh      The file handle
 * @param[out] str     String version of handle
 */

void
nfs_FhandleToStr(u_long rq_vers,
                 void *fh,
                 char *str)
{
  switch (rq_vers)
    {
    case NFS_V4:
      sprint_fhandle4(str, (nfs_fh4 *)fh);
      break;

    case NFS_V3:
      sprint_fhandle3(str, (nfs_fh3 *)fh);
      break;

    case NFS_V2:
      sprint_fhandle2(str, (fhandle2 *)fh);
      break;
    }
}                               /* nfs_FhandleToStr */

/**
 *
 * @brief Gets a cache entry using a file handle as input
 *
 * This function returns a cache entry corresponding to the supplied
 * filehandle.
 *
 * If a cache entry is returned, its refcount is incremented by 1.
 *
 * @param[in]  rq_vers  Version of the NFS protocol to be used
 * @param[in]  fh       Filehandle to look up
 * @param[out] status   Status, as appropriate to supplied version
 * @param[out] attr     FSAL attributes for this cache entry
 * @param[in]  context  Client's FSAL credentials
 * @param[out] rc       Status for the request (NFS_REQ_DROP or NFS_REQ_OK)
 *
 * @return a cache entry if successful, NULL otherwise
 *
 */
cache_entry_t *
nfs_FhandleToCache(u_long rq_vers,
                   void *fh,
                   int *status,
                   fsal_attrib_list_t *attr,
                   fsal_op_context_t *context,
                   int *rc)
{
  cache_inode_fsal_data_t fsal_data;
  cache_inode_status_t cache_status;
  cache_entry_t *entry = NULL;
  fsal_attrib_list_t obj_attr;
  exportlist_t *export = NULL;
  short exportid = 0;

  /* Default behaviour */
  *rc = NFS_REQ_OK;

  memset(&fsal_data, 0, sizeof(fsal_data));
  switch (rq_vers)
    {
    case NFS_V4:
      if(!nfs4_FhandleToFSAL((nfs_fh4 *)fh, &fsal_data.fh_desc, context))
        {
          *rc = NFS_REQ_DROP;
          *status = NFS4ERR_BADHANDLE;
          return NULL;
        }
      exportid = nfs4_FhandleToExportId((nfs_fh4 *)fh);
      break;

    case NFS_V3:
      if(!nfs3_FhandleToFSAL((nfs_fh3 *)fh, &fsal_data.fh_desc, context))
        {
          *rc = NFS_REQ_DROP;
          *status = NFS3ERR_BADHANDLE;
          return NULL;
        }
      exportid = nfs3_FhandleToExportId((nfs_fh3 *)fh);
      break;

    case NFS_V2:
      if(!nfs2_FhandleToFSAL((fhandle2 *)fh, &fsal_data.fh_desc, pcontext))
        {
          *rc = NFS_REQ_DROP;
          *status = NFSERR_STALE;
          return NULL;
        }
      exportid = nfs2_FhandleToExportId((fhandle2 *)fh);
      break;
    }

  print_buff(COMPONENT_FILEHANDLE,
             fsal_data.fh_desc.start,
             fsal_data.fh_desc.len);

  if((pexport = nfs_Get_export_by_id(nfs_param.pexportlist, exportid)) == NULL)
    {
      /* invalid handle */
      switch (rq_vers)
        {
        case NFS_V4:
          *status = NFS4ERR_STALE;
          break;

        case NFS_V3:
          *status = NFS3ERR_STALE;
          break;

        case NFS_V2:
          *status = NFSERR_STALE;
          break;
        }
      *rc = NFS_REQ_DROP;

      LogFullDebug(COMPONENT_NFSPROTO,
                   "Invalid file handle passed to nfsFhandleToCache ");
      return NULL;
    }

  if((entry = cache_inode_get(&fsal_data, &obj_attr, context,
                              NULL, &cache_status)) == NULL)
    {
      switch (rq_vers)
        {
        case NFS_V4:
          *status = NFS4ERR_STALE;
          break;

        case NFS_V3:
          *status = NFS3ERR_STALE;
          break;

        case NFS_V2:
          *status = NFSERR_STALE;
          break;
        }
      *prc = NFS_REQ_OK;
      return NULL;
    }

  if(attr != NULL)
    *attr = obj_attr;

  return entry;
}                               /* nfs_FhandleToCache */

/**
 * @brief Converts FSAL Attributes to NFSv3 PostOp Attributes structure.
 *
 * This function converts FSAL Attributes to NFSv3 PostOp Attributes structure.
 *
 * @param[in]  export    The related export entry
 * @param[in]  fsal_attr FSAL attributes
 * @param[out] attr      NFSv3 PostOp structure attributes.
 *
 */
int nfs_SetPostOpAttr(exportlist_t *export,
                      const fsal_attrib_list_t *fsal_attr,
                      post_op_attr *result)
{
  if(fsal_attr == NULL)
    {
      result->attributes_follow
           = nfs3_FSALattr_To_Fattr(export,
                                    fsal_attr,
                                    &(result->post_op_attr_u.attributes));
    }

  if(nfs3_FSALattr_To_Fattr(export,
                            fsal_attr,
                            &(presult->post_op_attr_u.attributes))
     == 0)
    result->attributes_follow = FALSE;
  else
    result->attributes_follow = TRUE;

  return 0;
} /* nfs_SetPostOpAttr */

/**
 * @brief Converts FSAL Attributes to NFSv3 PreOp Attributes structure.
 *
 * Converts FSAL Attributes to NFSv3 PreOp Attributes structure.
 *
 * @param[in]  fsal_attr FSAL attributes.
 * @param[out] attr      NFSv3 PreOp structure attributes.
 */

void nfs_SetPreOpAttr(fsal_attrib_list_t *fsal_attr, pre_op_attr *attr)
{
  if(fsal_attr == NULL)
    {
      attr->attributes_follow = FALSE;
    }
  else
    {
      attr->pre_op_attr_u.attributes.size = pfsal_attr->filesize;
      attr->pre_op_attr_u.attributes.mtime.seconds = fsal_attr->mtime.seconds;
      attr->pre_op_attr_u.attributes.mtime.nseconds = 0;

      attr->pre_op_attr_u.attributes.ctime.seconds = fsal_attr->ctime.seconds;
      attr->pre_op_attr_u.attributes.ctime.nseconds = 0;

      attr->attributes_follow = TRUE;
    }
}                               /* nfs_SetPreOpAttr */

/**
 * @brief Sets NFSv3 Weak Cache Coherency structure.
 *
 * Sets NFSv3 Weak Cache Coherency structure.
 *
 * @param[in]  export      Export entry
 * @param[in]  before_attr The attributes before the operation.
 * @param[in]  after_attr  The attributes after the operation
 * @param[out] wcc_data    The Weak Cache Coherency structure
 */
void nfs_SetWccData(exportlist_t *export,
                    fsal_attrib_list_t *before_attr,
                    fsal_attrib_list_t *after_attr,
                    wcc_data *wcc_data)
{
  /* Build directory pre operation attributes */
  nfs_SetPreOpAttr(before_attr, &(wcc_data->before));

  /* Build directory post operation attributes */
  nfs_SetPostOpAttr(export, after_attr, &(wcc_data->after));
} /* nfs_SetWccData */

/**
 * @brief Indicates if an error is retryable or not.
 *
 * Ths function indicates if an error is retryable or not.
 *
 * @param[in] cache_status Input Cache Inode Status value, to be tested
 *
 * @return TRUE if retryable, FALSE otherwise.
 */
int nfs_RetryableError(cache_inode_status_t cache_status)
{
  switch (cache_status)
    {
    case CACHE_INODE_IO_ERROR:
      if(nfs_param.core_param.drop_io_errors)
        {
          /* Drop the request */
          return TRUE;
        }
      else
        {
          /* Propagate error to the client */
          return FALSE;
        }
      break;

    case CACHE_INODE_INVALID_ARGUMENT:
      if(nfs_param.core_param.drop_inval_errors)
        {
          /* Drop the request */
          return TRUE;
        }
      else
        {
          /* Propagate error to the client */
          return FALSE;
        }
      break;

    case CACHE_INODE_DELAY:
      if(nfs_param.core_param.drop_delay_errors)
        {
          /* Drop the request */
          return TRUE;
        }
      else
        {
          /* Propagate error to the client */
          return FALSE;
        }
      break;

    case CACHE_INODE_SUCCESS:
      LogCrit(COMPONENT_NFSPROTO,
              "Possible implementation error: CACHE_INODE_SUCCESS managed as an error");
      return FALSE;
      break;

    case CACHE_INODE_MALLOC_ERROR:
    case CACHE_INODE_POOL_MUTEX_INIT_ERROR:
    case CACHE_INODE_GET_NEW_LRU_ENTRY:
    case CACHE_INODE_UNAPPROPRIATED_KEY:
    case CACHE_INODE_INIT_ENTRY_FAILED:
    case CACHE_INODE_FSAL_ERROR:
    case CACHE_INODE_LRU_ERROR:
    case CACHE_INODE_HASH_SET_ERROR:
    case CACHE_INODE_INCONSISTENT_ENTRY:
    case CACHE_INODE_HASH_TABLE_ERROR:
    case CACHE_INODE_INSERT_ERROR:
      /* Internal error, should be dropped and retryed */
      return TRUE;
      break;

    case CACHE_INODE_NOT_A_DIRECTORY:
    case CACHE_INODE_BAD_TYPE:
    case CACHE_INODE_ENTRY_EXISTS:
    case CACHE_INODE_DIR_NOT_EMPTY:
    case CACHE_INODE_NOT_FOUND:
    case CACHE_INODE_FSAL_EACCESS:
    case CACHE_INODE_IS_A_DIRECTORY:
    case CACHE_INODE_FSAL_EPERM:
    case CACHE_INODE_NO_SPACE_LEFT:
    case CACHE_INODE_CACHE_CONTENT_ERROR:
    case CACHE_INODE_CACHE_CONTENT_EXISTS:
    case CACHE_INODE_CACHE_CONTENT_EMPTY:
    case CACHE_INODE_READ_ONLY_FS:
    case CACHE_INODE_KILLED:
    case CACHE_INODE_FSAL_ESTALE:
    case CACHE_INODE_FSAL_ERR_SEC:
    case CACHE_INODE_QUOTA_EXCEEDED:
    case CACHE_INODE_NOT_SUPPORTED:
    case CACHE_INODE_NAME_TOO_LONG:
    case CACHE_INODE_STATE_CONFLICT:
    case CACHE_INODE_DEAD_ENTRY:
    case CACHE_INODE_ASYNC_POST_ERROR:
    case CACHE_INODE_STATE_ERROR:
    case CACHE_INODE_BAD_COOKIE:
    case CACHE_INODE_FILE_BIG:
      /* Non retryable error, return error to client */
      return FALSE;
      break;
    }

  /* Should never reach this */
  LogDebug(COMPONENT_NFSPROTO,
           "cache_inode_status=%u not managed properly in nfs_RetryableError,"
           " line %u should never be reached", cache_status, __LINE__);
  return FALSE;
}

/**
 * @brief Set fields in response for failure
 *
 * This function sets a grab-bag of data structures to properly
 * reflect an error.
 *
 * @param[in]  export       The associated export
 * @param[in]  version      The NFS protocol version
 * @param[in]  cache_status The result returned from cache_inode
 * @param[out] status       NFS statuc code
 * @param[out] post_op_attr Relevant attributes after failed operation
 * @param[out] pre_vattr1   Attributes of first file before operation
 * @param[out] wcc_data1    Weak cache coherency for first file
 * @param[out] pre_vattr2   Attributes of second file before operation
 * @param[out] wcc_data1    Weak cache coherency for second file
 */

void nfs_SetFailedStatus(exportlist_t *export,
                         int version,
                         cache_inode_status_t cache_status,
                         unsigned int *status,
                         post_op_attr *post_op_attr,
                         fsal_attrib_list_t *pre_vattr1,
                         wcc_data *wcc_data1,
                         fsal_attrib_list_t *pre_vattr2,
                         wcc_data *wcc_data2)
{
  switch (version)
    {
    case NFS_V2:
      if(status != CACHE_INODE_SUCCESS)
        *status = nfs2_Errno(cache_status);
      break;

    case NFS_V3:
      if(status != CACHE_INODE_SUCCESS) /* Should not use success to address a failed status */
        *status = nfs3_Errno(cache_status);

      if(post_op_attr != NULL)
        nfs_SetPostOpAttr(export, NULL, post_op_attr);

      if(wcc_data1 != NULL)
        nfs_SetWccData(export, pre_vattr1, NULL, wcc_data1);

      if(wcc_data2 != NULL)
        nfs_SetWccData(export, pre_vattr2, NULL, wcc_data2);
      break;

    }
}

#ifdef _USE_NFS4_ACL
/**
 * @brief Encode a special username
 *
 * Encode a string representation of a special userid directly into
 * the attribute value buffer.
 *
 * @param[in]     who            The userid to map
 * @param[out]    attrvalsBuffer The buffer to write the response
 * @param[in,out] LastOffset     The current write position in the buffer
 *
 * @retval 1 if successful.
 * @retval 0 on failure.
 */
static int nfs4_encode_acl_special_user(int who, char *attrvalsBuffer,
                                        size_t *LastOffset)
{
  int rc = 0;
  int i;
  u_int utf8len = 0;
  u_int deltalen = 0;

  for (i = 0; i < FSAL_ACE_SPECIAL_EVERYONE; i++)
    {
      if (whostr_2_type_map[i].type == who)
        {
          if(whostr_2_type_map[i].stringlen % 4 == 0)
            deltalen = 0;
          else
            deltalen = 4 - whostr_2_type_map[i].stringlen % 4;

          utf8len = htonl(whostr_2_type_map[i].stringlen + deltalen);
          memcpy((attrvalsBuffer + *LastOffset), &utf8len, sizeof(int));
          *LastOffset += sizeof(int);

          memcpy((attrvalsBuffer + *LastOffset), whostr_2_type_map[i].string,
                 whostr_2_type_map[i].stringlen);
          *LastOffset += whostr_2_type_map[i].stringlen;

          /* Pad with zero to keep xdr alignement */
          if(deltalen != 0)
            memset((attrvalsBuffer + *LastOffset), 0, deltalen);
          *LastOffset += deltalen;

          /* Found a matched one. */
          rc = 1;
          break;
        }
    }

  return rc;
}

/**
 * @brief Encode a group name
 *
 * Encode a string representation of a group id directly into the
 * attribute value buffer.
 *
 * @param[in]     gid            The userid to map
 * @param[out]    attrvalsBuffer The buffer to write the response
 * @param[in,out] LastOffset     The current write position in the buffer
 *
 * @retval 1 if successful.
 * @retval 0 on failure.
 */
static int nfs4_encode_acl_group_name(fsal_gid_t gid, char *attrvalsBuffer,
                                      size_t *LastOffset)
{
  int rc = 0;
  char name[MAXNAMLEN];
  u_int utf8len = 0;
  u_int stringlen = 0;
  u_int deltalen = 0;

  rc = gid2name(name, &gid);
  LogFullDebug(COMPONENT_NFS_V4,
               "encode gid2name = %s, strlen = %llu",
               name, (long long unsigned int)strlen(name));
  if(rc == 0)  /* Failure. */
    {
      /* Encode gid itself without @. */
      sprintf(name, "%u", gid);
    }

  stringlen = strlen(name);
  if(stringlen % 4 == 0)
    deltalen = 0;
  else
    deltalen = 4 - (stringlen % 4);

  utf8len = htonl(stringlen + deltalen);
  memcpy((char *)(attrvalsBuffer + *LastOffset), &utf8len, sizeof(int));
  *LastOffset += sizeof(int);

  memcpy((char *)(attrvalsBuffer + *LastOffset), name, stringlen);
  *LastOffset += stringlen;

  /* Pad with zero to keep xdr alignement */
  if(deltalen != 0)
    memset((char *)(attrvalsBuffer + *LastOffset), 0, deltalen);
  *LastOffset += deltalen;

  return rc;
}

/**
 * @brief Encode a username
 *
 * Encode a string representation of a user id directly into the
 * attribute value buffer.
 *
 * @param[in]     whotype        The type of the userid
 * @param[in]     useride        The userid to map
 * @param[out]    attrvalsBuffer The buffer to write the response
 * @param[in,out] LastOffset     The current write position in the buffer
 *
 * @retval 1 if successful.
 * @retval 0 on failure.
 */
static int nfs4_encode_acl_user_name(int whotype, fsal_uid_t uid,
                                     char *attrvalsBuffer,
                                     size_t *LastOffset)
{
  int rc = 0;
  char name[MAXNAMLEN];
  u_int utf8len = 0;
  u_int stringlen = 0;
  u_int deltalen = 0;

  /* Encode special user first. */
  if (whotype != FSAL_ACE_NORMAL_WHO)
    {
      rc = nfs4_encode_acl_special_user(uid, attrvalsBuffer, LastOffset);
      if(rc == 1)  /* Success. */
        return rc;
    }

  /* Encode normal user or previous user we failed to encode as special user. */
  rc = uid2name(name, &uid);
  LogFullDebug(COMPONENT_NFS_V4,
               "econde uid2name = %s, strlen = %llu",
               name, (long long unsigned int)strlen(name));
  if(rc == 0)  /* Failure. */
    {
      /* Encode uid itself without @. */
      sprintf(name, "%u", uid);
    }

  stringlen = strlen(name);
  if(stringlen % 4 == 0)
    deltalen = 0;
  else
    deltalen = 4 - (stringlen % 4);

  utf8len = htonl(stringlen + deltalen);
  memcpy((char *)(attrvalsBuffer + *LastOffset), &utf8len, sizeof(int));
  *LastOffset += sizeof(int);

  memcpy((char *)(attrvalsBuffer + *LastOffset), name, stringlen);
  *LastOffset += stringlen;

  /* Pad with zero to keep xdr alignement */
  if(deltalen != 0)
    memset((char *)(attrvalsBuffer + *LastOffset), 0, deltalen);
  *LastOffset += deltalen;

  return rc;
}

/**
 * @brief Encode an NFSv4 ACL
 *
 * Encode a file's ACL directly into the attribute value buffer.
 *
 * @param[in]     pattr          FSAL attributes
 * @param[out]    attrvalsBuffer The buffer to write the response
 * @param[in,out] LastOffset     The current write position in the buffer
 *
 * @retval 1 if successful.
 * @retval 0 on failure.
 */
static int nfs4_encode_acl(fsal_attrib_list_t * pattr,
                           char *attrvalsBuffer, size_t *LastOffset)
{
  int rc = 0;
  uint32_t naces, type, flag, access_mask, whotype;
  fsal_ace_t *pace;

  if(pattr->acl)
    {
      LogFullDebug(COMPONENT_NFS_V4,
                   "GATTR: Number of ACEs = %u",
                   pattr->acl->naces);

      /* Encode number of ACEs. */
      naces = htonl(pattr->acl->naces);
      memcpy((attrvalsBuffer + *LastOffset), &naces, sizeof(uint32_t));
      *LastOffset += sizeof(uint32_t);

      /* Encode ACEs. */
      for(pace = pattr->acl->aces; pace < pattr->acl->aces + pattr->acl->naces; pace++)
        {
          LogFullDebug(COMPONENT_NFS_V4,
                       "GATTR: type=0X%x, flag=0X%x, perm=0X%x",
                       pace->type, pace->flag, pace->perm);

          type = htonl(pace->type);
          flag = htonl(pace->flag);
          access_mask = htonl(pace->perm);

          memcpy((attrvalsBuffer + *LastOffset), &type, sizeof(uint32_t));
          *LastOffset += sizeof(uint32_t);

          memcpy((attrvalsBuffer + *LastOffset), &flag, sizeof(uint32_t));
          *LastOffset += sizeof(uint32_t);

          memcpy((attrvalsBuffer + *LastOffset), &access_mask, sizeof(uint32_t));
          *LastOffset += sizeof(uint32_t);

          if(IS_FSAL_ACE_GROUP_ID(*pace))  /* Encode group name. */
            {
              rc = nfs4_encode_acl_group_name(pace->who.gid, attrvalsBuffer, LastOffset);
            }
          else
            {
              if(!IS_FSAL_ACE_SPECIAL_ID(*pace))
                {
                  whotype = FSAL_ACE_NORMAL_WHO;
                }
              else
                whotype = pace->who.uid;

              /* Encode special or normal user name. */
              rc = nfs4_encode_acl_user_name(whotype, pace->who.uid, attrvalsBuffer, LastOffset);
            }

          LogFullDebug(COMPONENT_NFS_V4,
                       "GATTR: special = %u, %s = %u",
                       IS_FSAL_ACE_SPECIAL_ID(*pace),
                       IS_FSAL_ACE_GROUP_ID(*pace) ? "gid" : "uid",
                       IS_FSAL_ACE_GROUP_ID(*pace) ? pace->who.gid : pace->who.uid);

        }
    }
  else
    {
      LogFullDebug(COMPONENT_NFS_V4,
                   "nfs4_encode_acl: no acl available");

      fattr4_acl acl;
      acl.fattr4_acl_len = htonl(0);
      memcpy((attrvalsBuffer + *LastOffset), &acl, sizeof(fattr4_acl));
      *LastOffset += fattr4tab[FATTR4_ACL].size_fattr4;
    }

  return rc;
}
#endif                          /* _USE_NFS4_ACL */

/**
 * @brief Free the resources for a fattr4
 *
 * This function releases the memory used to store the bitmap and
 * values of a fattr4.
 *
 * @param[out] fattr The fattr4 to free.
 */
void nfs4_Fattr_Free(fattr4 *fattr)
{
  if(fattr->attrmask.bitmap4_val != NULL)
    {
      gsh_free(fattr->attrmask.bitmap4_val);
      fattr->attrmask.bitmap4_val = NULL;
    }

  if(fattr->attr_vals.attrlist4_val != NULL)
    {
      gsh_free(fattr->attr_vals.attrlist4_val);
      fattr->attr_vals.attrlist4_val = NULL;
    }
}

/**
 * @brief Encode supported attributes into attribute buffer
 *
 * This function encodes the supported attributes into the attributes
 * buffer.
 *
 * @param[in,out] xdr Stream to which to write the file
 *
 *
 * @retval TRUE on success
 * @retval FALSE on failure
 */

static inline bool_t
encode_supported_attributes(XDR *xdr)
{
     /* The index into the fattr4 table. */
     size_t idx = 0;
     /* Temporary value in which one word is constructed */
     uint32_t temp_word = 0;
     /* Encoded content */
     uint32_t *buffer
          = (uint32_t *) XDR_INLINE(xdr,
                                    (NFS4_ATTRMAP_LEN + 1) *
                                    sizeof(uint32_t));

     if (!buffer) {
          return FALSE;
     }

     *(buffer++) = htonl(NFS4_ATTRMAP_LEN);

     /* Rather than allocating and freeing unnecessarily, encode the
        bitmap directly into the buffer. */

     for (idx = 0; idx <= FATTR4_FS_CHARSET_CAP; ++idx) {
          /* Whenever we hit the end of a word in the bitmap, clear
             the temporary and move to the next one. */
          if ((idx > 0) &&
              (idx % 32 == 0)) {
               *(buffer++) = htonl(temp_word);
               temp_word = 0;
          }

          if (fattr4tab[idx].supported) {
               temp_word |= (1 << (idx % 32));
          }
     }
     /* Write out the incomplete word. */

     *(buffer++) = htonl(temp_word);
     return TRUE;
}

/**
 * @brief Encode type
 *
 * This function encodes the type of a file into the attributes
 * buffer.
 *
 * @param[in,out] xdr    Stream to which to write the attribute
 * @param[in]     type   Type of the file
 *
 * @retval TRUE on success
 * @retval FALSE on failure
 */

static inline bool_t
encode_type(XDR *xdr,
            fsal_nodetype_t type)
{
     nfs_ftype4 nfs_type;

     switch (type) {
     case FSAL_TYPE_FILE:
     case FSAL_TYPE_XATTR:
          nfs_type = NF4REG;
          break;

     case FSAL_TYPE_DIR:
          nfs_type = NF4DIR;
          break;

     case FSAL_TYPE_BLK:
          nfs_type = NF4BLK;
          break;

     case FSAL_TYPE_CHR:
          nfs_type = NF4CHR;
          break;

     case FSAL_TYPE_LNK:
          nfs_type = NF4LNK;
          break;

     case FSAL_TYPE_SOCK:
          nfs_type = NF4SOCK;
          break;

     case FSAL_TYPE_FIFO:
          nfs_type = NF4FIFO;
          break;

     default:
          return FALSE;
     }

     return xdr_nfs_ftype4(xdr, &nfs_type);
}

/**
 * @brief Encode an FSAL time
 *
 * This function encodes an FSAL time as an NFS time on the stream.
 *
 * @param[in,out] xdr    Stream to which to write the attribute
 * @param[in]     time   The time to encode.
 *
 * @retval TRUE on success
 * @retval FALSE on failure
 */

static inline bool_t
xdr_fsal_time(XDR *xdr,
              fsal_time_t *time)
{
     /* An NFSv4 time is the number of*/

     return (xdr_uint64_t(xdr,
                          &time->seconds) &&
             xdr_uint32_t(xdr,
                          &time->nseconds));
} /* xdr_fsal_time */

/**
 * @brief Populate dynamicinfo once
 *
 * This is a convenience wrapper to ensure that dynamicinfo has been
 * populated only when it is required without littering every case
 * statement with the same cut and paste code.
 *
 * @brief[in,out] statfscalled Whether cache_inode_statfs has been called
 * @brief[out]    dynamicinfo  The dynamic FS info
 * @brief[in]     entry        The entry in question
 * @brief[in]     context      FSAL credentials
 */

static inline bool_t
ensure_dynamic(bool_t *statfscalled,
               fsal_dynamicfsinfo_t *dynamicinfo,
               cache_entry_t *entry,
               fsal_op_context_t context)
{
     cache_inode_status_t cache_status;

     if (!*statfscalled) {
          if (cache_inode_statfs(data->current_entry,
                                 dynamicinfo,
                                 context,
                                 &cache_status) !=
             CACHE_INODE_SUCCESS) {
               return FALSE;
          }
     }

     return TRUE;
}


/**
 *
 * @brief Converts FSAL Attributes to NFSv4 Fattr buffer
 *
 * This function allocates and fill a fattr4 structure with the
 * requested attributes.
 *
 * @param[in]  export Export for this file
 * @param[in]  attr   FSAL attributes
 * @param[out] fattr  fattr4, to be freed with nfs4_fattr_free
 * @param[in]  data   NFSv4 compoud data
 * @param[in]  objFH  The NFSv4 filehandle of the object
 * @param[in]  bitmap Bitmap of attributes being requested
 *
 * @retval 0 on successful
 * @retval -1 on failure
 *
 */

int
nfs4_FSALattr_To_Fattr(const exportlist_t *export,
                       fsal_attrib_list_t *attr,
                       fattr4 *fattr,
                       const compound_data_t *data,
                       const nfs_fh4 *objFH,
                       const bitmap4 *bitmap)
{
     /* The buffer into which we write the attributes. */
     char *buff = NULL;
     /* Index of the current word in the attribute mask */
     uint32_t mask_word = 0;
     /* Index of the current bit in the attribute mask */
     unsigned mask_bit = 0;

     /* True if statfs has been called and dynamicinfo has been
        populated. */
     bool_t statfscalled = FALSE;
     /* Pointer to the static filesystem information. */
     fsal_staticfsinfo_t *staticinfo
          = (data ?
             data->pcontext->export_context->fe_static_fs_info :
             NULL);
     /* Dynamic filesystem info */
     fsal_dynamicfsinfo_t dynamicinfo;
     /* Success or failure of encoding the attributes */
     int rc = 0;
     /* The XDR stream for the attributes */
     XDR xdr;

     /* Initiate the XDR stream on the buffer */
     Fattr->attrmask.bitmap4_val = NULL;
     if (!(buff = gsh_calloc(ATTRVALS_BUFFLEN))) {
          rc = -1;
          goto out;
     }
     xdrmem_create(&xdr, buff, ATTRVALS_BUFFLEN, XDR_ENCODE);

     /* Iterate over the bits int he bitmap. */
     for (mask_word  = 0, mask_bit = 0;
          mask_word < bitmap->bitmap4_len;
          (mask_bit == 31 ? mask_bit = 0 : ++mask_bit) || ++mask_word) {
          /* The number of the attribute corresponding to the current
             word and bit. */
          uint32_t attribute_to_set = mask_bit + mask_word * 32;
          /* Whether setting the current attribute was successful. */
          bool_t op_attr_success = FALSE;

          if (!(bitmap.bitmap4_val[mask_word] & (1 << mask_bit))) {
               continue;
          }

          if (attribute_to_set > FATTR4_FS_CHARSET_CAP) {
               rc = -1;
               goto out;
          }

          switch (attribute_to_set) {
          case FATTR4_SUPPORTED_ATTRS:
               op_attr_success =
                    encode_supported_attributes(&xdr);
               break;

          case FATTR4_TYPE:
               op_attr_success
                    = encode_type(&xdr, attr->type)
               break;

          case FATTR4_FH_EXPIRE_TYPE:
          {
               fattr4_fh_expire_type extype
                    = (nfs_param.nfsv4_param.fh_expire ?
                       FH4_VOLATILE_ANY : FH4_PERSISTENT);
               op_attr_success = xdr_fattr4_fh-expire_type(&xdr, &extype);
          }
          break;

          case FATTR4_CHANGE:
               op_attr_success = xdr_changeid4(&xdr, &attr->change);
               break;

          case FATTR4_SIZE:
               op_attr_success
                    = xdr_fattr4_size(&xdr, &attr->filesize);
               break;

          case FATTR4_LINK_SUPPORT:
               op_attr_success
                    = (staticinfo &&
                       xdr_fattr4_link_support(&xdr,
                                               &staticinfo->link_support));
               break;

          case FATTR4_SYMLINK_SUPPORT:
               op_attr_success
                    = (staticinfo &&
                       xdr_fattr4_symlink_support(
                            &xdr,
                            staticinfo->symlink_support));
               break;

          case FATTR4_NAMED_ATTR:
               op_attr_success
                    = (staticinfo &&
                       xdr_fattr4_named_attr(&xdr,
                                             &staticinfo->named_attr));
               break;

          case FATTR4_FSID:
               op_attr_success
                    = xdr_fsid(&xdr,
                               &export->filesystem_id);
               break;

          case FATTR4_UNIQUE_HANDLES:
               op_attr_success
                    = (staticinfo &&
                       xdr_fattr4_unique_handles(
                            &xdr,
                            &staticinfo->unique_handles));
               break;

          case FATTR4_LEASE_TIME:
               op_attr_success
                    = xdr_fattr4_lease_time(
                         nfs_param.nfsv4_param.lease_lifetime);
               break;

          case FATTR4_RDATTR_ERROR:
          {
               fattr4_rdata_error dummy = NFS4_OK;
               /**
                * @todo ACE: This looks suspicious.
                */
               /* By default, READDIR call may use a different value */

               op_attr_success =
                    xdr_fattr4_rdata_error(&xdr,
                                           &dummy);
          }
          break;

          case FATTR4_ACL:
#ifdef _USE_NFS4_ACL
               op_attr_success = nfs4_encode_acl(&xdr, attr);
#else
               /* We don't support ACLs. */
               op_attr_success = FALSE;
#endif
               break;

          case FATTR4_ACLSUPPORT:
          {
               fattr4_aclsupport aclsupport;
#ifdef _USE_NFS4_ACL
               aclsupport = (ACL4_SUPPORT_ALLOW_ACL |
                             ACL4_SUPPORT_DENY_ACL);
#else
               aclsupport = 0;
#endif
               op_attr_success
                    = xdr_fattr4_aclsupport(&xdr, &aclsupport);
               offset += sizeof(fattr4_aclsupport);
               op_attr_success = 1;
          }
          break;

          case FATTR4_ARCHIVE:
          {
               /* Archive flag is not supported */
               fattr4_archive dummy = FALSE;

               op_attr_success
                    = xdr_fattr4_archive(&xdr, &dummy);
          }
          break;

          case FATTR4_CANSETTIME:
               op_attr_success
                    = (staticinfo &&
                       xdr_fattr4_cansettime(&xdr,
                                             &staticinfo->cansettime));
               break;

          case FATTR4_CASE_INSENSITIVE:
               op_attr_success
                    = (staticinfo &&
                       xdr_fattr4_case_insensitive(
                            &xdr,
                            &staticinfo->case_insensitive));
               break;

          case FATTR4_CASE_PRESERVING:
               op_attr_success
                    = (staticinfo &&
                       xdr_fattr4_case_preserving(
                            &xdr,
                            &staticinfo->case_preserving));
               break;

          case FATTR4_CHOWN_RESTRICTED:
               op_attr_success
                    = (staticinfo &&
                       xdr_fattr4_chown_restricted(
                            &xdr,
                            &staticinfo->chown_restricted));
               break;

          case FATTR4_FILEHANDLE:
               if (objFH) {
                    op_attr_success
                         = xdr_nfs_fh4(&xdr, objFH);
               } else {
                    op_attr_success = FALSE;
               }
               break;

        case FATTR4_FILEID:
             op_attr_success = xdr_fattr4_file_id(&xdr,
                                                  &attr->fileid);
             break;

          case FATTR4_FILES_AVAIL:
               op_attr_success
                    = (ensure_dynamic(&statfscalled,
                                      &dynamicinfo,
                                      data->current_entry,
                                      data->pcontext) &&
                       xdr_fattr4_files_avail(&xdr,
                                              &dynamicinfo.avail_files));
               break;

          case FATTR4_FILES_FREE:
               op_attr_success
                    = (ensure_dynamic(&statfscalled,
                                      &dynamicinfo,
                                      data->current_entry,
                                      data->pcontext) &&
                       xdr_fattr4_files_free(&xdr,
                                             &dynamicinfo.free_files));
          break;

          case FATTR4_FILES_TOTAL:
               op_attr_success
                    = (ensure_dynamic(&statfscalled,
                                      &dynamicinfo,
                                      data->current_entry,
                                      data->pcontext) &&
                       xdr_fattr4_files_total(&xdr,
                                              &dynamicinfo.total_files));
          break;

          case FATTR4_HIDDEN:
          {
               bool_t dummy = FALSE;
               op_attr_success =
                    xdr_fattr4_hidden(&xdr, &dummy);
               break;
          }

          case FATTR4_HOMOGENEOUS:
               op_attr_success
                    = (staticinfo &&
                       xdr_fattr4_homogeneous(
                            &xdr,
                            &staticinfo->homogeneous));
               break;

          case FATTR4_MAXFILESIZE:
          {
               fattr4_maxfilesize dummy = FSINFO_MAX_FILESIZE;

               op_attr_success
                    = xdr_fattr4_maxfilesize(&xdr, &dummy);
               break;

          case FATTR4_MAXLINK:
               op_attr_success
                    = (staticinfo &&
                       xdr_fattr4_maxlink(&xdr,
                                          &staticinfo->maxlink)))
               break;

          case FATTR4_MAXNAME:
               op_attr_success
                    = xdr_fattr4_maxname(&xdr,
                                         &staticinfo->maxnamelen);
               break;

          case FATTR4_MAXREAD:
               op_attr_success
                    = xdr_fattr4_maxread(&xdr, &export->MaxRead);
               break;

          case FATTR4_MAXWRITE:
               op_attr_success
                  = xdr_fattr4_maxwrite(&xdr, &export->MaxWrite);
             break;

          case FATTR4_MODE:
          {
               xdr_fattr4_mode file_mode
                    = fsal2unix_mode(attr->mode);
               op_attr_success = xdr_fattr4_mode(&xdr, &file_mode);
          }
          break;

          case FATTR4_NO_TRUNC:
               op_attr_success
                    = xdr_fattr4_no_trunc(&xdr,
                                          staticinfo->no_trunc);
               break;

          case FATTR4_NUMLINKS:
               op_attr_success
                    = xdr_fattr4_numlinks(&xdr, attr->numlinks);
               break;

          case FATTR4_OWNER:
               op_attr_success
                    = nfs4_encode_user(&xdr, attr->owner);
               break;

          case FATTR4_OWNER_GROUP:
               op_attr_success
                    = nfs4_encode_user(&xdr, attr->owner);
               break;

          case FATTR4_QUOTA_AVAIL_HARD:
          {
               /**
                * @todo Not the right answer, actual quotas
                * should be implemented.
                */
               fattr4_quota_avail_hard dummy = NFS_V4_MAX_QUOTA_HARD;
               op_attr_success
                    = xdr_fattr4_quota_avail_hard(&xdr, &dummy);
          }
          break;

          case FATTR4_QUOTA_AVAIL_SOFT:
          {
               /**
                * @todo Not the right answer, actual quotas
                * should be implemented.
                */
               fattr4_quota_avail_soft dummy = NFS_V4_MAX_QUOTA_SOFT;
               op_attr_success = xdr_fattr4_quota_avail_soft(&xdr, &dummy);
          }
          break;

          case FATTR4_QUOTA_USED:
               op_attr_success
                    = xdr_fattr4_quota_used(&xdr, &attr->filesize);
               break;

          case FATTR4_RAWDEV:
               /* fattr4_rawdev is a structure composed of two
                  32 bit integers. */
               op_attr_success
                    = (xdr_uint32_t(&xdr, &attr->rawdev.major) &&
                       xdr_uint32_t(&xdr, &attr->rawdev.minor));
               break;

          case FATTR4_SPACE_AVAIL:
               op_attr_success
                    = (ensure_dynamic(&statfscalled,
                                      &dynamicinfo,
                                      data->current_entry,
                                      data->pcontext) &&
                       xdr_fattr4_space_avail(&xdr,
                                              &dynamicinfo.avail_bytes));
               break;

          case FATTR4_SPACE_FREE:
               op_attr_success
                    = (ensure_dynamic(&statfscalled,
                                      &dynamicinfo,
                                      data->current_entry,
                                      data->pcontext) &&
                       xdr_fattr4_space_free(&xdr,
                                             &dynamicinfo.free_bytes));
               break;

          case FATTR4_SPACE_TOTAL:
               op_attr_success
                    = (ensure_dynamic(&statfscalled,
                                      &dynamicinfo,
                                      data->current_entry,
                                      data->pcontext) &&
                       xdr_fattr4_space_total(&xdr,
                                              &dynamicinfo.total_bytes));
               break;

          case FATTR4_SPACE_USED:
               op_attr_success
                    = xdr_fattr4_space_used(&xdr,
                                            &attr->spaceused);
               break;

          case FATTR4_SYSTEM:
          {
               bool_t dummy = FALSE;
               op_attr_success
                    = xdr_fattr4_system(&xdr, &dummy);
          }
          break;

        case FATTR4_TIME_ACCESS:
             op_attr_success
                  = xdr_fsal_time(&xdr,
                                  &attr->atime);
             break;

          case FATTR4_TIME_DELTA:
          {
               /**
                * @todo ACE: We should support a better (and
                * configurable) granularity.  Fix this up in
                * conjunction with fixing changeid4.
                */
               fattr4_time_delta dummy = {1, 0};
               op_attr_success = xdr_fattr4_time_delta(&xdr, &dummy);
               break;
          }

          case FATTR4_TIME_METADATA:
               op_attr_success
                    = xdr_fsal_time(&xdr,
                                    &attr->ctime);
               break;

          case FATTR4_TIME_MODIFY:
               op_attr_success
                    = xdr_fsal_time(&xdr,
                                    &attr->mtime);
               break;

          case FATTR4_MOUNTED_ON_FILEID:
               op_attr_success
                    = xdr_fattr4_mounted_on_fileid(&xdr,
                                                   &attr->fileid);
               break;

#ifdef _USE_NFS4_1
        case FATTR4_FS_LAYOUT_TYPES:
#ifdef _PNFS_MDS
             op_attr_success
                  = xdr_fattr4_fs_layout_types(&xdr,
                                               &staticinfo->fs_layout_tyeps);
             break;
#endif /* _PNFS_MDS */

#ifdef _PNFS_MDS
          case FATTR4_LAYOUT_BLKSIZE:
               op_attr_success
                    = xdr_fattr4_layout_blksize(&xdr,
                                                &staticinfo->layout_blksize);
               break;
#endif /* _PNFS_MDS */
#endif /* _USE_NFS4_1 */

          default:
               LogWarn(COMPONENT_NFS_V4,
                       "Failure encoding attribute: %s",
                       fattr4tab[attribute_to_set].name);
               op_attr_success = 0;
               break;
          } /* switch(attribute_to_set) */

          if (!op_attr_success) {
               rc = -1;
               goto out;
          }
     } /* for (...) */

     /* We don't return any attributes they didn't ask for and did
        return all the attributes they did ask for. (The protocol
        requires that we return an error if we can't returnt he value
        of a supported attribute.)

        This might be a candidate for a pool since theyr'e so short,
        but since they're freed by the RPC library, it would be
        involved to do that. */
     if (!(Fattr->attrmask.bitmap4_val =
           gsh_malloc(bitmap->bitmap4_len))) {
          goto out;
     }
     Fattr->attrmask.bitmap4_len = bitmap->bitmap4_len;
     memcpy(Fattr->attrmask.bitmap4_val,
            bitmap->bitmap4_val,
            bitmap->bitmap4_len);

     /* Point the attrlist4 at the data we allocated. */
     Fattr->attr_vals.attrlist4_len = offset;
     if (offset) {
          Fattr->attr_vals.attrlist4_val = buff;
     } else {
          Fattr->attr_vals = NULL;
          gsh-free(buff);
     }
out:

     if (rc != 0) {
          buff && gsh_free(buff);
          Fattr->attrmask.bitmap4_val &&
               gsh_free(Fattr->attrmask.bitmap4_val);
     }

     return rc;
} /* nfs4_FSALattr_To_Fattr */

/**
 *
 * nfs3_Sattr_To_FSALattr: Converts NFSv3 Sattr to FSAL Attributes.
 *
 * Converts NFSv3 Sattr to FSAL Attributes.
 *
 * @param pFSAL_attr  [OUT]  computed FSAL attributes.
 * @param psattr      [IN]   NFSv3 sattr to be set.
 * 
 * @return 0 if failed, 1 if successful.
 *
 */
int nfs3_Sattr_To_FSALattr(fsal_attrib_list_t * pFSAL_attr,     /* Out: file attributes */
                           sattr3 * psattr)     /* In: file attributes  */
{
  struct timeval t;

  if(pFSAL_attr == NULL || psattr == NULL)
    return 0;

  pFSAL_attr->asked_attributes = 0;

  if(psattr->mode.set_it == TRUE)
    {
      LogFullDebug(COMPONENT_NFSPROTO,
                   "nfs3_Sattr_To_FSALattr: mode = %o",
                   psattr->mode.set_mode3_u.mode);
      pFSAL_attr->mode = unix2fsal_mode(psattr->mode.set_mode3_u.mode);
      pFSAL_attr->asked_attributes |= FSAL_ATTR_MODE;
    }

  if(psattr->uid.set_it == TRUE)
    {
      LogFullDebug(COMPONENT_NFSPROTO,
                   "nfs3_Sattr_To_FSALattr: uid = %d",
                   psattr->uid.set_uid3_u.uid);
      pFSAL_attr->owner = psattr->uid.set_uid3_u.uid;
      pFSAL_attr->asked_attributes |= FSAL_ATTR_OWNER;
    }

  if(psattr->gid.set_it == TRUE)
    {
      LogFullDebug(COMPONENT_NFSPROTO,
                   "nfs3_Sattr_To_FSALattr: gid = %d",
                   psattr->gid.set_gid3_u.gid);
      pFSAL_attr->group = psattr->gid.set_gid3_u.gid;
      pFSAL_attr->asked_attributes |= FSAL_ATTR_GROUP;
    }

  if(psattr->size.set_it == TRUE)
    {
      LogFullDebug(COMPONENT_NFSPROTO,
                   "nfs3_Sattr_To_FSALattr: size = %lld",
                   psattr->size.set_size3_u.size);
      pFSAL_attr->filesize = (fsal_size_t) psattr->size.set_size3_u.size;
      pFSAL_attr->spaceused = (fsal_size_t) psattr->size.set_size3_u.size;
      /* Both FSAL_ATTR_SIZE and FSAL_ATTR_SPACEUSED are to be managed */
      pFSAL_attr->asked_attributes |= FSAL_ATTR_SIZE;
      pFSAL_attr->asked_attributes |= FSAL_ATTR_SPACEUSED;
    }

  if(psattr->atime.set_it != DONT_CHANGE)
    {
      LogFullDebug(COMPONENT_NFSPROTO,
                   "nfs3_Sattr_To_FSALattr: set=%d atime = %d,%d",
                   psattr->atime.set_it, psattr->atime.set_atime_u.atime.seconds,
                   psattr->atime.set_atime_u.atime.nseconds);
      if(psattr->atime.set_it == SET_TO_CLIENT_TIME)
        {
          pFSAL_attr->atime.seconds = psattr->atime.set_atime_u.atime.seconds;
          pFSAL_attr->atime.nseconds = 0;
        }
      else
        {
          /* Use the server's current time */
          gettimeofday(&t, NULL);

          pFSAL_attr->atime.seconds = t.tv_sec;
          pFSAL_attr->atime.nseconds = 0;
        }
      pFSAL_attr->asked_attributes |= FSAL_ATTR_ATIME;
    }

  if(psattr->mtime.set_it != DONT_CHANGE)
    {
      LogFullDebug(COMPONENT_NFSPROTO,
                   "nfs3_Sattr_To_FSALattr: set=%d mtime = %d",
                   psattr->atime.set_it, psattr->mtime.set_mtime_u.mtime.seconds ) ;
      if(psattr->mtime.set_it == SET_TO_CLIENT_TIME)
        {
          pFSAL_attr->mtime.seconds = psattr->mtime.set_mtime_u.mtime.seconds;
          pFSAL_attr->mtime.nseconds = 0 ;
        }
      else
        {
          /* Use the server's current time */
          gettimeofday(&t, NULL);
          pFSAL_attr->mtime.seconds = t.tv_sec;
          pFSAL_attr->mtime.nseconds = 0 ;
        }
      pFSAL_attr->asked_attributes |= FSAL_ATTR_MTIME;
    }

  return 1;
}                               /* nfs3_Sattr_To_FSALattr */

/**
 * 
 * nfs2_FSALattr_To_Fattr: Converts FSAL Attributes to NFSv2 attributes.
 * 
 * Converts FSAL Attributes to NFSv2 attributes.
 *
 * @param pexport   [IN]  the related export entry.
 * @param FSAL_attr [IN]  pointer to FSAL attributes.
 * @param Fattr     [OUT] pointer to NFSv2 attributes. 
 * 
 * @return 1 if successful, 0 otherwise. 
 *
 */
int nfs2_FSALattr_To_Fattr(exportlist_t * pexport,      /* In: the related export entry */
                           fsal_attrib_list_t * pFSAL_attr,     /* In: file attributes  */
                           fattr2 * pFattr)     /* Out: file attributes */
{
  /* Badly formed arguments */
  if(pFSAL_attr == NULL || pFattr == NULL)
    return 0;

  /* @todo BUGAZOMEU: sanity check on attribute mask (does the FSAL support the attributes required to support NFSv2 ? */

  /* initialize mode */
  pFattr->mode = 0;

  switch (pFSAL_attr->type)
    {
    case FSAL_TYPE_FILE:
      pFattr->type = NFREG;
      pFattr->mode = NFS2_MODE_NFREG;
      break;

    case FSAL_TYPE_DIR:
      pFattr->type = NFDIR;
      pFattr->mode = NFS2_MODE_NFDIR;
      break;

    case FSAL_TYPE_BLK:
      pFattr->type = NFBLK;
      pFattr->mode = NFS2_MODE_NFBLK;
      break;

    case FSAL_TYPE_CHR:
      pFattr->type = NFCHR;
      pFattr->mode = NFS2_MODE_NFCHR;
      break;

    case FSAL_TYPE_FIFO:
      pFattr->type = NFFIFO;
      /** @todo mode mask ? */
      break;

    case FSAL_TYPE_LNK:
      pFattr->type = NFLNK;
      pFattr->mode = NFS2_MODE_NFLNK;
      break;

    case FSAL_TYPE_SOCK:
      pFattr->type = NFSOCK;
      /** @todo mode mask ? */
      break;

    case FSAL_TYPE_XATTR:
    case FSAL_TYPE_JUNCTION:
      pFattr->type = NFBAD;
    }

  pFattr->mode |= fsal2unix_mode(pFSAL_attr->mode);
  pFattr->nlink = pFSAL_attr->numlinks;
  pFattr->uid = pFSAL_attr->owner;
  pFattr->gid = pFSAL_attr->group;

  /* in NFSv2, it only keeps fsid.major, casted into an into an int32 */
  pFattr->fsid = (u_int) (pexport->filesystem_id.major & 0xFFFFFFFFLL);

  LogFullDebug(COMPONENT_NFSPROTO,
               "nfs2_FSALattr_To_Fattr: fsid.major = %#llX (%llu), fsid.minor = %#llX (%llu), nfs2_fsid = %#X (%u)",
               pexport->filesystem_id.major, pexport->filesystem_id.major,
               pexport->filesystem_id.minor, pexport->filesystem_id.minor, pFattr->fsid,
               pFattr->fsid);

  if(pFSAL_attr->filesize > NFS2_MAX_FILESIZE)
    pFattr->size = NFS2_MAX_FILESIZE;
  else
    pFattr->size = pFSAL_attr->filesize;

  pFattr->blocksize = DEV_BSIZE;

  pFattr->blocks = pFattr->size >> 9;   /* dividing by 512 */
  if(pFattr->size % DEV_BSIZE != 0)
    pFattr->blocks += 1;

  if(pFSAL_attr->type == FSAL_TYPE_CHR || pFSAL_attr->type == FSAL_TYPE_BLK)
    pFattr->rdev = pFSAL_attr->rawdev.major;
  else
    pFattr->rdev = 0;

  pFattr->atime.seconds = pFSAL_attr->atime.seconds;
  pFattr->atime.useconds = pFSAL_attr->atime.nseconds / 1000;
  pFattr->mtime.seconds = pFSAL_attr->mtime.seconds;
  pFattr->mtime.useconds = pFSAL_attr->mtime.nseconds / 1000;
  pFattr->ctime.seconds = pFSAL_attr->ctime.seconds;
  pFattr->ctime.useconds = pFSAL_attr->ctime.nseconds / 1000;
  pFattr->fileid = pFSAL_attr->fileid;

  return 1;
}                               /*  nfs2_FSALattr_To_Fattr */

/**
 * 
 * nfs4_SetCompoundExport
 * 
 * This routine fills in the pexport field in the compound data.
 *
 * @param pfh [OUT] pointer to compound data to be used. 
 * 
 * @return NFS4_OK if successfull. Possible errors are NFS4ERR_BADHANDLE and NFS4ERR_WRONGSEC.
 *
 */

int nfs4_SetCompoundExport(compound_data_t * data)
{
  short exportid;

  /* This routine is not related to pseudo fs file handle, do not handle them */
  if(nfs4_Is_Fh_Pseudo(&(data->currentFH)))
    return NFS4_OK;

  /* Get the export id */
  if((exportid = nfs4_FhandleToExportId(&(data->currentFH))) == 0)
    return NFS4ERR_BADHANDLE;

  if((data->pexport = nfs_Get_export_by_id(data->pfullexportlist, exportid)) == NULL)
    return NFS4ERR_BADHANDLE;

  if((data->pexport->options & EXPORT_OPTION_NFSV4) == 0)
    return NFS4ERR_ACCESS;

  if(nfs4_MakeCred(data) != NFS4_OK)
    return NFS4ERR_WRONGSEC;

  return NFS4_OK;
}                               /* nfs4_SetCompoundExport */

/**
 * 
 * nfs4_FhandleToExId
 * 
 * This routine extracts the export id from the filehandle
 *
 * @param fh4p  [IN]  pointer to file handle to be used.
 * @param ExIdp [OUT] pointer to buffer in which found export id will be stored. 
 * 
 * @return TRUE is successful, FALSE otherwise. 
 *
 */
int nfs4_FhandleToExId(nfs_fh4 * fh4p, unsigned short *ExIdp)
{
  file_handle_v4_t *pfhandle4;

  /* Map the filehandle to the correct structure */
  pfhandle4 = (file_handle_v4_t *) (fh4p->nfs_fh4_val);

  /* The function should not be used on a pseudo fhandle */
  if(pfhandle4->pseudofs_flag == TRUE)
    return FALSE;

  *ExIdp = pfhandle4->exportid;
  return TRUE;
}                               /* nfs4_FhandleToExId */

/**** Glue related functions ****/

/**
 *
 * nfs4_stringid_split: Splits a domain stamped name in two different parts.
 *
 * Splits a domain stamped name in two different parts.
 *
 * @param buff [IN] the input string
 * @param uidname [OUT] the extracted uid name
 * @param domainname [OUT] the extracted fomain name
 *
 * @return nothing (void function) 
 *
 */
void nfs4_stringid_split(char *buff, char *uidname, char *domainname)
{
  char *c = NULL;
  unsigned int i = 0;

  for(c = buff, i = 0; *c != '\0'; c++, i++)
    if(*c == '@')
      break;

  strncpy(uidname, buff, i);
  uidname[i] = '\0';
  strcpy(domainname, c);

  LogFullDebug(COMPONENT_NFS_V4,
               "buff = #%s#    uid = #%s#   domain = #%s#",
               buff, uidname, domainname);
}                               /* nfs4_stringid_split */

/**
 *
 * free_utf8: Free's a utf8str that was created by utf8dup
 *
 * @param utf8str [IN]  UTF8 string to be freed
 *
 */
void free_utf8(utf8string * utf8str)
{
  if(utf8str != NULL)
    {
      if(utf8str->utf8string_val != NULL)
        gsh_free(utf8str->utf8string_val);
      utf8str->utf8string_val = 0;
      utf8str->utf8string_len = 0;
    }
}

/**
 *
 * utf8dup: Makes a copy of a utf8str.
 *
 * @param newstr  [OUT] copied UTF8 string
 * @param oldstr  [IN]  input UTF8 string
 *
 * @return -1 if failed, 0 if successful.
 *
 */
int utf8dup(utf8string * newstr, utf8string * oldstr)
{
  if(newstr == NULL)
    return -1;

  newstr->utf8string_len = oldstr->utf8string_len;
  newstr->utf8string_val = NULL;

  if(oldstr->utf8string_len == 0 || oldstr->utf8string_val == NULL)
    return 0;

  newstr->utf8string_val = gsh_malloc(oldstr->utf8string_len);
  if(newstr->utf8string_val == NULL)
    return -1;

  strncpy(newstr->utf8string_val, oldstr->utf8string_val, oldstr->utf8string_len);

  return 0;
}                               /* uft82str */

/**
 *
 * utf82str: converts a UTF8 string buffer into a string descriptor.
 *
 * Converts a UTF8 string buffer into a string descriptor.
 *
 * @param str     [OUT] computed output string
 * @param utf8str [IN]  input UTF8 string
 *
 * @return -1 if failed, 0 if successful.
 *
 */
int utf82str(char *str, int size, utf8string * utf8str)
{
  int copy;

  if(str == NULL)
    return -1;

  if(utf8str == NULL || utf8str->utf8string_len == 0)
    {
      str[0] = '\0';
      return -1;
    }

  if(utf8str->utf8string_len >= size)
    copy = size - 1;
  else
    copy = utf8str->utf8string_len;

  strncpy(str, utf8str->utf8string_val, copy);
  str[copy] = '\0';

  if(copy < utf8str->utf8string_len)
    return -1;

  return 0;
}                               /* uft82str */

/**
 *
 * str2utf8: converts a string buffer into a UTF8 string descriptor.
 *
 * Converts a string buffer into a UTF8 string descriptor.
 *
 * @param str     [IN]  input string
 * @param utf8str [OUT] computed UTF8 string
 *
 * @return -1 if failed, 0 if successful.
 *
 */
int str2utf8(char *str, utf8string * utf8str)
{
  uint_t len;
  char buff[MAXNAMLEN];

  /* The uft8 will probably be sent over XDR, for this reason, its size MUST be a multiple of 32 bits = 4 bytes */
  strcpy(buff, str);
  len = strlen(buff);

  /* BUGAZOMEU: TO BE DONE: use STUFF ALLOCATOR here */
  if(utf8str->utf8string_val == NULL)
    return -1;

  utf8str->utf8string_len = len;
  memcpy(utf8str->utf8string_val, buff, utf8str->utf8string_len);
  return 0;
}                               /* str2utf8 */

/**
 * 
 * nfs4_NextSeqId: compute the next nfsv4 sequence id.
 *
 * Compute the next nfsv4 sequence id.
 *
 * @param seqid [IN] previous sequence number.
 * 
 * @return the requested sequence number.
 *
 */

seqid4 nfs4_NextSeqId(seqid4 seqid)
{
  return ((seqid + 1) % 0xFFFFFFFF);
}                               /* nfs4_NextSeqId */

/**
 *
 * nfs_bitmap4_to_list: convert an attribute's bitmap to a list of attributes.
 *
 * Convert an attribute's bitmap to a list of attributes.
 *
 * @param b     [IN] bitmap to convert.
 * @param plen  [OUT] list's length.
 * @param plval [OUT] list's values.
 *
 * @return nothing (void function)
 *
 */

/*
 * bitmap is usually 2 x uint32_t which makes a uint64_t
 *
 * Structure of the bitmap is as follow
 *
 *                  0         1
 *    +-------+---------+----------+-
 *    | count | 31 .. 0 | 63 .. 32 |
 *    +-------+---------+----------+-
 *
 * One bit is set for every possible attributes. The bits are packed together in a uint32_T (XDR alignment reason probably)
 * As said in the RFC3530, the n-th bit is with the uint32_t #(n/32), and its position with the uint32_t is n % 32
 * Example
 *     1st bit = FATTR4_TYPE            = 1
 *     2nd bit = FATTR4_LINK_SUPPORT    = 5
 *     3rd bit = FATTR4_SYMLINK_SUPPORT = 6
 *
 *     Juste one uint32_t is necessay: 2**1 + 2**5 + 2**6 = 2 + 32 + 64 = 98
 *   +---+----+
 *   | 1 | 98 |
 *   +---+----+
 *
 * Other Example
 *
 *     1st bit = FATTR4_LINK_SUPPORT    = 5
 *     2nd bit = FATTR4_SYMLINK_SUPPORT = 6
 *     3rd bit = FATTR4_MODE            = 33
 *     4th bit = FATTR4_OWNER           = 36
 *
 *     Two uint32_t will be necessary there:
 *            #1 = 2**5 + 2**6 = 32 + 64 = 96
 #            #2 = 2**(33-32) + 2**(36-32) = 2**1 + 2**4 = 2 + 16 = 18 
 *   +---+----+----+
 *   | 2 | 98 | 18 |
 *   +---+----+----+
 *
 */

void nfs4_bitmap4_to_list(bitmap4 * b, uint_t * plen, uint32_t * pval)
{
  uint_t i = 0;
  uint_t val = 0;
  uint_t index = 0;
  uint_t offset = 0;
  uint_t fattr4tabidx=0;
  if(b->bitmap4_len > 0)
    LogFullDebug(COMPONENT_NFS_V4, "Bitmap: Len = %u Val = %u|%u",
                 b->bitmap4_len, b->bitmap4_val[0], b->bitmap4_val[1]);
  else
    LogFullDebug(COMPONENT_NFS_V4, "Bitmap: Len = %u ... ", b->bitmap4_len);

  for(offset = 0; offset < b->bitmap4_len; offset++)
    {
      for(i = 0; i < 32; i++)
        {
          fattr4tabidx = i+32*offset;
#ifdef _USE_NFS4_1
          if (fattr4tabidx > FATTR4_FS_CHARSET_CAP)
#else
          if (fattr4tabidx > FATTR4_MOUNTED_ON_FILEID)
#endif
             goto exit;

          val = 1 << i;         /* Compute 2**i */
          if(b->bitmap4_val[offset] & val)
            pval[index++] = fattr4tabidx;
        }
    }
exit:
  *plen = index;

}                               /* nfs4_bitmap4_to_list */

/**
 * 
 * nfs4_list_to_bitmap4: convert a list of attributes to an attributes's bitmap.
 * 
 * Convert a list of attributes to an attributes's bitmap.
 *
 * @param b [OUT] computed bitmap
 * @param plen [IN] list's length 
 * @param pval [IN] list's array
 *
 * @return nothing (void function).
 *
 */

/* bitmap is usually 2 x uint32_t which makes a uint64_t 
 * bitmap4_len is the number of uint32_t required to keep the bitmap value 
 *
 * Structure of the bitmap is as follow
 *
 *                  0         1
 *    +-------+---------+----------+-
 *    | count | 31 .. 0 | 63 .. 32 | 
 *    +-------+---------+----------+-
 *
 * One bit is set for every possible attributes. The bits are packed together in a uint32_T (XDR alignment reason probably)
 * As said in the RFC3530, the n-th bit is with the uint32_t #(n/32), and its position with the uint32_t is n % 32
 * Example
 *     1st bit = FATTR4_TYPE            = 1
 *     2nd bit = FATTR4_LINK_SUPPORT    = 5
 *     3rd bit = FATTR4_SYMLINK_SUPPORT = 6
 *
 *     Juste one uint32_t is necessay: 2**1 + 2**5 + 2**6 = 2 + 32 + 64 = 98
 *   +---+----+
 *   | 1 | 98 |
 *   +---+----+
 *
 * Other Example
 *
 *     1st bit = FATTR4_LINK_SUPPORT    = 5
 *     2nd bit = FATTR4_SYMLINK_SUPPORT = 6
 *     3rd bit = FATTR4_MODE            = 33
 *     4th bit = FATTR4_OWNER           = 36
 *
 *     Two uint32_t will be necessary there:
 *            #1 = 2**5 + 2**6 = 32 + 64 = 96
 #            #2 = 2**(33-32) + 2**(36-32) = 2**1 + 2**4 = 2 + 16 = 18 
 *   +---+----+----+
 *   | 2 | 98 | 18 |
 *   +---+----+----+
 *
 */

/* This function converts a list of attributes to a bitmap4 structure */
void nfs4_list_to_bitmap4(bitmap4 * b, uint_t * plen, uint32_t * pval)
{
  uint_t i;
  uint_t intpos = 0;
  uint_t bitpos = 0;
  uint_t val = 0;
  /* Both uint32 int the bitmap MUST be allocated */
  b->bitmap4_val[0] = 0;
  b->bitmap4_val[1] = 0;
  b->bitmap4_val[2] = 0;

  for(i = 0; i < *plen; i++)
    {
      intpos = pval[i] / 32;
      bitpos = pval[i] % 32;
      val = 1 << bitpos;
      b->bitmap4_val[intpos] |= val;

      if(intpos == 0)
        b->bitmap4_len = 1;
      else if(intpos == 1)
        b->bitmap4_len = 2;
      else if(intpos == 2)
        b->bitmap4_len = 3;
    }
  LogFullDebug(COMPONENT_NFS_V4, "Bitmap: Len = %u   Val = %u|%u|%u",
               b->bitmap4_len,
               b->bitmap4_len >= 1 ? b->bitmap4_val[0] : 0,
               b->bitmap4_len >= 2 ? b->bitmap4_val[1] : 0,
               b->bitmap4_len >= 3 ? b->bitmap4_val[2] : 0);
}                               /* nfs4_list_to_bitmap4 */

/*
 * Conversion of attributes
 */

/**
 *
 * nfs3_FSALattr_To_PartialFattr: Converts FSAL Attributes to NFSv3 attributes.
 *
 * Fill in the fields in the fattr3 structure which have matching
 * attribute bits set. Caller must explictly specify which bits it expects
 * to avoid misunderstandings.
 *
 * @param FSAL_attr [IN]  pointer to FSAL attributes.
 * @param want      [IN]  attributes which MUST be copied into output
 * @param Fattr     [OUT] pointer to NFSv3 attributes. 
 *
 * @return 1 if successful, 0 otherwise.
 *
 */
int 
nfs3_FSALattr_To_PartialFattr(const fsal_attrib_list_t * FSAL_attr,
                              fsal_attrib_mask_t want,
                              fattr3 * Fattr)
{
  if(FSAL_attr == NULL || Fattr == NULL)
    {
      LogFullDebug(COMPONENT_NFSPROTO,
                   "%s: FSAL_attr=%p, Fattr=%p",
                   __func__, FSAL_attr, Fattr);
      return 0;
    }

  if((FSAL_attr->asked_attributes & want) != want)
    {
      LogEvent(COMPONENT_NFSPROTO,
                   "%s: Caller wants 0x%llx, we only have 0x%llx - missing 0x%llx",
                   __func__, want, FSAL_attr->asked_attributes, 
                   (FSAL_attr->asked_attributes & want) ^ want);
      return 0;
    }

  if(FSAL_attr->asked_attributes & FSAL_ATTR_TYPE)
    {
      switch (FSAL_attr->type)
        {
        case FSAL_TYPE_FIFO:
          Fattr->type = NF3FIFO;
          break;

        case FSAL_TYPE_CHR:
          Fattr->type = NF3CHR;
          break;

        case FSAL_TYPE_DIR:
          Fattr->type = NF3DIR;
          break;

        case FSAL_TYPE_BLK:
          Fattr->type = NF3BLK;
          break;

        case FSAL_TYPE_FILE:
        case FSAL_TYPE_XATTR:
          Fattr->type = NF3REG;
          break;

        case FSAL_TYPE_LNK:
          Fattr->type = NF3LNK;
          break;

        case FSAL_TYPE_SOCK:
          Fattr->type = NF3SOCK;
          break;

        case FSAL_TYPE_JUNCTION:
          /* Should not occur */
          LogFullDebug(COMPONENT_NFSPROTO,
                       "nfs3_FSALattr_To_Fattr: FSAL_attr->type = %d",
                       FSAL_attr->type);
          Fattr->type = 0;
          return 0;

        default:
          LogEvent(COMPONENT_NFSPROTO,
                       "nfs3_FSALattr_To_Fattr: Bogus type = %d",
                       FSAL_attr->type);
          return 0;
        }
    }

  if(FSAL_attr->asked_attributes & FSAL_ATTR_MODE)
    Fattr->mode = fsal2unix_mode(FSAL_attr->mode);
  if(FSAL_attr->asked_attributes & FSAL_ATTR_NUMLINKS)
    Fattr->nlink = FSAL_attr->numlinks;
  if(FSAL_attr->asked_attributes & FSAL_ATTR_OWNER)
    Fattr->uid = FSAL_attr->owner;
  if(FSAL_attr->asked_attributes & FSAL_ATTR_GROUP)
    Fattr->gid = FSAL_attr->group;
  if(FSAL_attr->asked_attributes & FSAL_ATTR_SIZE)
    Fattr->size = FSAL_attr->filesize;
  if(FSAL_attr->asked_attributes & FSAL_ATTR_SPACEUSED)
    Fattr->used = FSAL_attr->spaceused;
  if(FSAL_attr->asked_attributes & FSAL_ATTR_RAWDEV)
    {
      Fattr->rdev.specdata1 = FSAL_attr->rawdev.major;
      Fattr->rdev.specdata2 = FSAL_attr->rawdev.minor;
    }
  if(FSAL_attr->asked_attributes & FSAL_ATTR_FILEID)
    Fattr->fileid = FSAL_attr->fileid;
  if(FSAL_attr->asked_attributes & FSAL_ATTR_ATIME)
    {
      Fattr->atime.seconds = FSAL_attr->atime.seconds;
      Fattr->atime.nseconds = FSAL_attr->atime.nseconds;
    }
  if(FSAL_attr->asked_attributes & FSAL_ATTR_MTIME)
    {
      Fattr->mtime.seconds = FSAL_attr->mtime.seconds;
      Fattr->mtime.nseconds = FSAL_attr->mtime.nseconds;
    }
  if(FSAL_attr->asked_attributes & FSAL_ATTR_CTIME)
    {
      Fattr->ctime.seconds = FSAL_attr->ctime.seconds;
      Fattr->ctime.nseconds = FSAL_attr->ctime.nseconds;
    }

  return 1;
}                         /* nfs3_FSALattr_To_PartialFattr */

/**
 *
 * nfs3_FSALattr_To_Fattr: Converts FSAL Attributes to NFSv3 attributes.
 *
 * Converts FSAL Attributes to NFSv3 attributes.
 * The callee is expecting the full compliment of FSAL attributes to fill
 * in all the fields in the fattr3 structure.
 *
 * @param pexport   [IN]  the related export entry
 * @param FSAL_attr [IN]  pointer to FSAL attributes.
 * @param Fattr     [OUT] pointer to NFSv3 attributes. 
 *
 * @return 1 if successful, 0 otherwise.
 *
 */
int nfs3_FSALattr_To_Fattr(exportlist_t * pexport,      /* In: the related export entry */
                           const fsal_attrib_list_t * FSAL_attr,      /* In: file attributes */
                           fattr3 * Fattr)      /* Out: file attributes */
{
  if(FSAL_attr == NULL || Fattr == NULL)
    {
      LogFullDebug(COMPONENT_NFSPROTO,
                   "nfs3_FSALattr_To_Fattr: FSAL_attr=%p, Fattr=%p",
                   FSAL_attr, Fattr);
      return 0;
    }

  if(!nfs3_FSALattr_To_PartialFattr(FSAL_attr, 
                                    FSAL_ATTR_TYPE| FSAL_ATTR_MODE | FSAL_ATTR_NUMLINKS |
                                    FSAL_ATTR_OWNER | FSAL_ATTR_GROUP | FSAL_ATTR_SIZE |
                                    FSAL_ATTR_SPACEUSED | FSAL_ATTR_RAWDEV |
                                    FSAL_ATTR_ATIME | FSAL_ATTR_MTIME | FSAL_ATTR_CTIME,
                                    Fattr))
    return 0;

  /* in NFSv3, we only keeps fsid.major, casted into an nfs_uint64 */
  Fattr->fsid = (nfs3_uint64) pexport->filesystem_id.major;
  LogFullDebug(COMPONENT_NFSPROTO,
               "%s: fsid.major = %#llX (%llu), fsid.minor = %#llX (%llu), nfs3_fsid = %#llX (%llu)",
               __func__,
               pexport->filesystem_id.major, pexport->filesystem_id.major,
               pexport->filesystem_id.minor, pexport->filesystem_id.minor, Fattr->fsid,
               Fattr->fsid);
  return 1;
}

/**
 * 
 * nfs2_Sattr_To_FSALattr: Converts NFSv2 Set Attributes to FSAL attributes.
 * 
 * Converts NFSv2 Set Attributes to FSAL attributes.
 *
 * @param FSAL_attr [IN]  pointer to FSAL attributes.
 * @param Fattr     [OUT] pointer to NFSv2 set attributes. 
 * 
 * @return 1 if successful, 0 otherwise. 
 *
 */
int nfs2_Sattr_To_FSALattr(fsal_attrib_list_t * pFSAL_attr,     /* Out: file attributes */
                           sattr2 * Fattr)      /* In: file attributes */
{
  struct timeval t;

  FSAL_CLEAR_MASK(pFSAL_attr->asked_attributes);

  if(Fattr->mode != (unsigned int)-1)
    {
      pFSAL_attr->mode = unix2fsal_mode(Fattr->mode);
      FSAL_SET_MASK(pFSAL_attr->asked_attributes, FSAL_ATTR_MODE);
    }

  if(Fattr->uid != (unsigned int)-1)
    {
      pFSAL_attr->owner = Fattr->uid;
      FSAL_SET_MASK(pFSAL_attr->asked_attributes, FSAL_ATTR_OWNER);
    }

  if(Fattr->gid != (unsigned int)-1)
    {
      pFSAL_attr->group = Fattr->gid;
      FSAL_SET_MASK(pFSAL_attr->asked_attributes, FSAL_ATTR_GROUP);
    }

  if(Fattr->size != (unsigned int)-1)
    {
      /* Both FSAL_ATTR_SIZE and FSAL_ATTR_SPACEUSED are to be managed */
      pFSAL_attr->filesize = (fsal_size_t) Fattr->size;
      pFSAL_attr->spaceused = (fsal_size_t) Fattr->size;
      FSAL_SET_MASK(pFSAL_attr->asked_attributes, FSAL_ATTR_SIZE);
      FSAL_SET_MASK(pFSAL_attr->asked_attributes, FSAL_ATTR_SPACEUSED);
    }

  /* if mtime.useconds == 1 millions,
   * this means we must set atime and mtime
   * to server time (NFS Illustrated p. 98)
   */
  if(Fattr->mtime.useconds == 1000000)
    {
      gettimeofday(&t, NULL);

      pFSAL_attr->atime.seconds = pFSAL_attr->mtime.seconds = t.tv_sec;
      pFSAL_attr->atime.nseconds = pFSAL_attr->mtime.nseconds = 0 ;
      FSAL_SET_MASK(pFSAL_attr->asked_attributes, FSAL_ATTR_ATIME);
      FSAL_SET_MASK(pFSAL_attr->asked_attributes, FSAL_ATTR_MTIME);
    }
  else
    {
      /* set atime to client */

      if(Fattr->atime.seconds != (unsigned int)-1)
        {
          pFSAL_attr->atime.seconds = Fattr->atime.seconds;

          if(Fattr->atime.seconds != (unsigned int)-1)
            pFSAL_attr->atime.nseconds = Fattr->atime.useconds * 1000;
          else
            pFSAL_attr->atime.nseconds = 0;     /* ignored */

          FSAL_SET_MASK(pFSAL_attr->asked_attributes, FSAL_ATTR_ATIME);
        }

      /* set mtime to client */

      if(Fattr->mtime.seconds != (unsigned int)-1)
        {
          pFSAL_attr->mtime.seconds = Fattr->mtime.seconds;

          if(Fattr->mtime.seconds != (unsigned int)-1)
            pFSAL_attr->mtime.nseconds = Fattr->mtime.useconds * 1000;
          else
            pFSAL_attr->mtime.nseconds = 0;     /* ignored */

          FSAL_SET_MASK(pFSAL_attr->asked_attributes, FSAL_ATTR_MTIME);
        }
    }

  return 1;
}                               /* nfs2_Sattr_To_FSALattr */

/**
 *
 * nfs4_Fattr_Check_Access: checks if attributes have READ or WRITE access.
 *
 * Checks if attributes have READ or WRITE access.
 *
 * @param Fattr      [IN] pointer to NFSv4 attributes.
 * @param access     [IN] access to be checked, either FATTR4_ATTR_READ or FATTR4_ATTR_WRITE
 *
 * @return 1 if successful, 0 otherwise.
 *
 */

int nfs4_Fattr_Check_Access(fattr4 * Fattr, int access)
{
  unsigned int i = 0;

  uint32_t attrmasklist[FATTR4_MOUNTED_ON_FILEID];      /* List cannot be longer than FATTR4_MOUNTED_ON_FILEID */
  uint32_t attrmasklen = 0;

  /* Parameter sanity check */
  if(Fattr == NULL)
    return 0;

  if(access != FATTR4_ATTR_READ && access != FATTR4_ATTR_WRITE)
    return 0;

  /* Convert the attribute bitmap to an attribute list */
  nfs4_bitmap4_to_list(&(Fattr->attrmask), &attrmasklen, attrmasklist);

  for(i = 0; i < attrmasklen; i++)
    {
#ifdef _USE_NFS4_1
      if(attrmasklist[i] > FATTR4_FS_CHARSET_CAP)
#else
      if(attrmasklist[i] > FATTR4_MOUNTED_ON_FILEID)
#endif
        {
          /* Erroneous value... skip */
          continue;
        }

      if(((int)fattr4tab[attrmasklist[i]].access & access) != access)
        return 0;
    }

  return 1;
}                               /* nfs4_Fattr_Check_Access */

/**
 *
 * nfs4_Fattr_Check_Access_Bitmap: checks if attributes bitmaps have READ or WRITE access.
 *
 * Checks if attributes have READ or WRITE access.
 *
 * @param pbitmap    [IN] pointer to NFSv4 attributes.
 * @param access     [IN] access to be checked, either FATTR4_ATTR_READ or FATTR4_ATTR_WRITE
 *
 * @return 1 if successful, 0 otherwise.
 *
 */

int nfs4_Fattr_Check_Access_Bitmap(bitmap4 * pbitmap, int access)
{
  unsigned int i = 0;
#ifdef _USE_NFS4_1
#define MAXATTR FATTR4_FS_CHARSET_CAP
#else
#define MAXATTR FATTR4_MOUNTED_ON_FILEID
#endif
  uint32_t attrmasklist[MAXATTR];
  uint32_t attrmasklen = 0;

  /* Parameter sanity check */
  if(pbitmap == NULL)
    return 0;

  if(access != FATTR4_ATTR_READ && access != FATTR4_ATTR_WRITE)
    return 0;

  /* Convert the attribute bitmap to an attribute list */
  nfs4_bitmap4_to_list(pbitmap, &attrmasklen, attrmasklist);

  for(i = 0; i < attrmasklen; i++)
    {
      if(attrmasklist[i] > MAXATTR)
        {
          /* Erroneous value... skip */
          continue;
        }

      if(((int)fattr4tab[attrmasklist[i]].access & access) != access)
        return 0;
    }

  return 1;
}                               /* nfs4_Fattr_Check_Access */

/**
 *
 * nfs4_bitmap4_Remove_Unsupported: removes unsupported attributes from bitmap4
 *
 * Removes unsupported attributes from bitmap4
 *
 * @param pbitmap    [IN] pointer to NFSv4 attributes's bitmap.
 *
 * @return 1 if successful, 0 otherwise.
 *
 */
int nfs4_bitmap4_Remove_Unsupported(bitmap4 * pbitmap )
{
  uint_t i = 0;
  uint_t val = 0;
  uint_t offset = 0;
  uint fattr4tabidx = 0;

  uint32_t bitmap_val[3] ;
  bitmap4 bout ;
  int allsupp = 1;

  memset(bitmap_val, 0, 3 * sizeof(uint32_t));

  bout.bitmap4_val = bitmap_val ;
  bout.bitmap4_len = pbitmap->bitmap4_len  ;

  if(pbitmap->bitmap4_len > 0)
    LogFullDebug(COMPONENT_NFS_V4, "Bitmap: Len = %u Val = %u|%u",
                 pbitmap->bitmap4_len, pbitmap->bitmap4_val[0],
                 pbitmap->bitmap4_val[1]);
  else
    LogFullDebug(COMPONENT_NFS_V4, "Bitmap: Len = %u ... ",
                 pbitmap->bitmap4_len);

  for(offset = 0; offset < pbitmap->bitmap4_len; offset++)
    {
      for(i = 0; i < 32; i++)
        {
          fattr4tabidx = i+32*offset;
#ifdef _USE_NFS4_1
          if (fattr4tabidx > FATTR4_FS_CHARSET_CAP)
#else
          if (fattr4tabidx > FATTR4_MOUNTED_ON_FILEID)
#endif
             goto exit;

          val = 1 << i;         /* Compute 2**i */
          if(pbitmap->bitmap4_val[offset] & val)
           {
             if( fattr4tab[fattr4tabidx].supported ) /* keep only supported stuff */
               bout.bitmap4_val[offset] |= val ;
           }
        }
    }

exit:
  memcpy(pbitmap->bitmap4_val, bout.bitmap4_val,
         bout.bitmap4_len * sizeof(uint32_t));

  return allsupp;
}                               /* nfs4_Fattr_Bitmap_Remove_Unsupported */


/**
 *
 * nfs4_Fattr_Supported: Checks if an attribute is supported.
 *
 * Checks if an attribute is supported.
 *
 * @param Fattr      [IN] pointer to NFSv4 attributes.
 *
 * @return 1 if successful, 0 otherwise.
 *
 */

int nfs4_Fattr_Supported(fattr4 * Fattr)
{
  unsigned int i = 0;

  uint32_t attrmasklist[FATTR4_MOUNTED_ON_FILEID];      /* List cannot be longer than FATTR4_MOUNTED_ON_FILEID */
  uint32_t attrmasklen = 0;

  /* Parameter sanity check */
  if(Fattr == NULL)
    return 0;

  /* Convert the attribute bitmap to an attribute list */
  nfs4_bitmap4_to_list(&(Fattr->attrmask), &attrmasklen, attrmasklist);

  for(i = 0; i < attrmasklen; i++)
    {

#ifndef _USE_NFS4_1
      if(attrmasklist[i] > FATTR4_MOUNTED_ON_FILEID)
        {
          /* Erroneous value... skip */
          continue;
        }
#endif

      LogFullDebug(COMPONENT_NFS_V4,
                   "nfs4_Fattr_Supported  ==============> %s supported flag=%u | ",
                   fattr4tab[attrmasklist[i]].name, fattr4tab[attrmasklist[i]].supported);

      if(!fattr4tab[attrmasklist[i]].supported)
        return 0;
    }

  return 1;
}                               /* nfs4_Fattr_Supported */

/**
 *
 * nfs4_Fattr_Supported: Checks if an attribute is supported.
 *
 * Checks if an attribute is supported.
 *
 * @param Fattr      [IN] pointer to NFSv4 attributes.
 *
 * @return 1 if successful, 0 otherwise.
 *
 */

int nfs4_Fattr_Supported_Bitmap(bitmap4 * pbitmap)
{
  unsigned int i = 0;

  uint32_t attrmasklist[FATTR4_MOUNTED_ON_FILEID];      /* List cannot be longer than FATTR4_MOUNTED_ON_FILEID */
  uint32_t attrmasklen = 0;

  /* Parameter sanity check */
  if(pbitmap == NULL)
    return 0;

  /* Convert the attribute bitmap to an attribute list */
  nfs4_bitmap4_to_list(pbitmap, &attrmasklen, attrmasklist);

  for(i = 0; i < attrmasklen; i++)
    {

#ifndef _USE_NFS4_1
      if(attrmasklist[i] > FATTR4_MOUNTED_ON_FILEID)
        {
          /* Erroneous value... skip */
          continue;
        }
#endif
      
      LogFullDebug(COMPONENT_NFS_V4,
                   "nfs4_Fattr_Supported  ==============> %s supported flag=%u",
                   fattr4tab[attrmasklist[i]].name,
                   fattr4tab[attrmasklist[i]].supported);
      if(!fattr4tab[attrmasklist[i]].supported)
        return 0;
    }

  return 1;
}                               /* nfs4_Fattr_Supported */

/**
 *
 * nfs4_Fattr_cmp: compares 2 fattr4 buffers.
 *
 * Compares 2 fattr4 buffers.
 *
 * @param Fattr1      [IN] pointer to NFSv4 attributes.
 * @param Fattr2      [IN] pointer to NFSv4 attributes.
 *
 * @return TRUE if attributes are the same, FALSE otherwise, but -1 if RDATTR_ERROR is set
 *
 */
int nfs4_Fattr_cmp(fattr4 * Fattr1, fattr4 * Fattr2)
{
  uint32_t attrmasklist1[FATTR4_MOUNTED_ON_FILEID];     /* List cannot be longer than FATTR4_MOUNTED_ON_FILEID */
  uint32_t attrmasklen1 = 0;
  u_int LastOffset;
  uint32_t attrmasklist2[FATTR4_MOUNTED_ON_FILEID];     /* List cannot be longer than FATTR4_MOUNTED_ON_FILEID */
  uint32_t attrmasklen2 = 0;
  uint32_t i;
  uint32_t k;
  unsigned int cmp = 0;
  u_int len = 0;
  uint32_t attribute_to_set = 0;

  if(Fattr1 == NULL)
    return FALSE;

  if(Fattr2 == NULL)
    return FALSE;

  if(Fattr1->attrmask.bitmap4_len != Fattr2->attrmask.bitmap4_len)      /* different mask */
    return FALSE;

  /* Convert the attribute bitmap to an attribute list */
  nfs4_bitmap4_to_list(&(Fattr1->attrmask), &attrmasklen1, attrmasklist1);
  nfs4_bitmap4_to_list(&(Fattr2->attrmask), &attrmasklen2, attrmasklist2);

  /* Should not occur, bu this is a sanity check */
  if(attrmasklen1 != attrmasklen2)
    return FALSE;

  for(i = 0; i < attrmasklen1; i++)
    {
      if(attrmasklist1[i] != attrmasklist2[i])
        return 0;

      if(attrmasklist1[i] == FATTR4_RDATTR_ERROR)
        return -1;

      if(attrmasklist2[i] == FATTR4_RDATTR_ERROR)
        return -1;
    }

  cmp = 0;
  LastOffset = 0;
  len = 0;

  for(i = 0; i < attrmasklen1; i++)
    {
      attribute_to_set = attrmasklist1[i];

      LogFullDebug(COMPONENT_NFS_V4,
                   "nfs4_Fattr_cmp ==============> %s",
                   fattr4tab[attribute_to_set].name);

      switch (attribute_to_set)
        {
        case FATTR4_SUPPORTED_ATTRS:
          memcpy(&len, (char *)(Fattr1->attr_vals.attrlist4_val + LastOffset),
                 sizeof(u_int));
          cmp +=
              memcmp((char *)(Fattr1->attr_vals.attrlist4_val + LastOffset),
                     (char *)(Fattr2->attr_vals.attrlist4_val + LastOffset),
                     sizeof(u_int));

          len = htonl(len);
          LastOffset += sizeof(u_int);

          for(k = 0; k < len; k++)
            {
              cmp += memcmp((char *)(Fattr1->attr_vals.attrlist4_val + LastOffset),
                            (char *)(Fattr2->attr_vals.attrlist4_val + LastOffset),
                            sizeof(uint32_t));
              LastOffset += sizeof(uint32_t);
            }

          break;

        case FATTR4_FILEHANDLE:
        case FATTR4_OWNER:
        case FATTR4_OWNER_GROUP:
          memcpy(&len, (char *)(Fattr1->attr_vals.attrlist4_val + LastOffset),
                 sizeof(u_int));
          len = ntohl(len);     /* xdr marshalling on fattr4 */
          cmp += memcmp((char *)(Fattr1->attr_vals.attrlist4_val + LastOffset),
                        (char *)(Fattr2->attr_vals.attrlist4_val + LastOffset),
                        sizeof(u_int));
          LastOffset += sizeof(u_int);
          cmp += memcmp((char *)(Fattr1->attr_vals.attrlist4_val + LastOffset),
                        (char *)(Fattr2->attr_vals.attrlist4_val + LastOffset), len);
          break;

        case FATTR4_TYPE:
        case FATTR4_FH_EXPIRE_TYPE:
        case FATTR4_CHANGE:
        case FATTR4_SIZE:
        case FATTR4_LINK_SUPPORT:
        case FATTR4_SYMLINK_SUPPORT:
        case FATTR4_NAMED_ATTR:
        case FATTR4_FSID:
        case FATTR4_UNIQUE_HANDLES:
        case FATTR4_LEASE_TIME:
        case FATTR4_RDATTR_ERROR:
        case FATTR4_ACL:
        case FATTR4_ACLSUPPORT:
        case FATTR4_ARCHIVE:
        case FATTR4_CANSETTIME:
        case FATTR4_CASE_INSENSITIVE:
        case FATTR4_CASE_PRESERVING:
        case FATTR4_CHOWN_RESTRICTED:
        case FATTR4_FILEID:
        case FATTR4_FILES_AVAIL:
        case FATTR4_FILES_FREE:
        case FATTR4_FILES_TOTAL:
        case FATTR4_FS_LOCATIONS:
        case FATTR4_HIDDEN:
        case FATTR4_HOMOGENEOUS:
        case FATTR4_MAXFILESIZE:
        case FATTR4_MAXLINK:
        case FATTR4_MAXNAME:
        case FATTR4_MAXREAD:
        case FATTR4_MAXWRITE:
        case FATTR4_MIMETYPE:
        case FATTR4_MODE:
        case FATTR4_NO_TRUNC:
        case FATTR4_NUMLINKS:
        case FATTR4_QUOTA_AVAIL_HARD:
        case FATTR4_QUOTA_AVAIL_SOFT:
        case FATTR4_QUOTA_USED:
        case FATTR4_RAWDEV:
        case FATTR4_SPACE_AVAIL:
        case FATTR4_SPACE_FREE:
        case FATTR4_SPACE_TOTAL:
        case FATTR4_SPACE_USED:
        case FATTR4_SYSTEM:
        case FATTR4_TIME_ACCESS:
        case FATTR4_TIME_ACCESS_SET:
        case FATTR4_TIME_BACKUP:
        case FATTR4_TIME_CREATE:
        case FATTR4_TIME_DELTA:
        case FATTR4_TIME_METADATA:
        case FATTR4_TIME_MODIFY:
        case FATTR4_TIME_MODIFY_SET:
        case FATTR4_MOUNTED_ON_FILEID:
          cmp += memcmp((char *)(Fattr1->attr_vals.attrlist4_val + LastOffset),
                        (char *)(Fattr2->attr_vals.attrlist4_val + LastOffset),
                        fattr4tab[attribute_to_set].size_fattr4);
          break;

        default:
          return 0;
          break;
        }
    }
  if(cmp == 0)
    return TRUE;
  else
    return FALSE;
}

#ifdef _USE_NFS4_ACL
static int nfs4_decode_acl_special_user(utf8string *utf8str, int *who)
{
  int i;

  for (i = 0; i < FSAL_ACE_SPECIAL_EVERYONE; i++)
    {
      if(strncmp(utf8str->utf8string_val, whostr_2_type_map[i].string, utf8str->utf8string_len) == 0)
        {
          *who = whostr_2_type_map[i].type;
          return 0;
        }
    }

  return -1;
}

static int nfs4_decode_acl(fsal_attrib_list_t * pFSAL_attr, fattr4 * Fattr, u_int *LastOffset)
{
  fsal_acl_status_t status;
  fsal_acl_data_t acldata;
  fsal_ace_t *pace;
  fsal_acl_t *pacl;
  int len;
  char buffer[MAXNAMLEN];
  utf8string utf8buffer;
  int who;

  /* Decode number of ACEs. */
  memcpy(&(acldata.naces), (char*)(Fattr->attr_vals.attrlist4_val + *LastOffset), sizeof(u_int));
  acldata.naces = ntohl(acldata.naces);
  LogFullDebug(COMPONENT_NFS_V4,
               "SATTR: Number of ACEs = %u",
               acldata.naces);
  *LastOffset += sizeof(u_int);

  /* Allocate memory for ACEs. */
  acldata.aces = (fsal_ace_t *)nfs4_ace_alloc(acldata.naces);
  if(acldata.aces == NULL)
    {
      LogCrit(COMPONENT_NFS_V4,
              "SATTR: Failed to allocate ACEs");
      return NFS4ERR_SERVERFAULT;
    }
  else
    memset(acldata.aces, 0, acldata.naces * sizeof(fsal_ace_t));

  /* Decode ACEs. */
  for(pace = acldata.aces; pace < acldata.aces + acldata.naces; pace++)
    {
      memcpy(&(pace->type), (char*)(Fattr->attr_vals.attrlist4_val + *LastOffset), sizeof(fsal_acetype_t));
      pace->type = ntohl(pace->type);
      LogFullDebug(COMPONENT_NFS_V4,
                   "SATTR: ACE type = 0x%x",
                   pace->type);
      *LastOffset += sizeof(fsal_acetype_t);

      memcpy(&(pace->flag), (char*)(Fattr->attr_vals.attrlist4_val + *LastOffset), sizeof(fsal_aceflag_t));
      pace->flag = ntohl(pace->flag);
      LogFullDebug(COMPONENT_NFS_V4,
                   "SATTR: ACE flag = 0x%x",
                   pace->flag);
      *LastOffset += sizeof(fsal_aceflag_t);

      memcpy(&(pace->perm), (char*)(Fattr->attr_vals.attrlist4_val + *LastOffset), sizeof(fsal_aceperm_t));
      pace->perm = ntohl(pace->perm);
      LogFullDebug(COMPONENT_NFS_V4,
                   "SATTR: ACE perm = 0x%x",
                   pace->perm);
      *LastOffset += sizeof(fsal_aceperm_t);

      /* Find out who type */

      /* Convert name to uid or gid */
      memcpy(&len, (char *)(Fattr->attr_vals.attrlist4_val + *LastOffset), sizeof(u_int));
      len = ntohl(len);        /* xdr marshalling on fattr4 */
      *LastOffset += sizeof(u_int);

      memcpy(buffer, (char *)(Fattr->attr_vals.attrlist4_val + *LastOffset), len);
      buffer[len] = '\0';

      /* Do not forget that xdr_opaque are aligned on 32bit long words */
      while((len % 4) != 0)
        len += 1;

      *LastOffset += len;

      /* Decode users. */
      LogFullDebug(COMPONENT_NFS_V4,
                   "SATTR: owner = %s, len = %d, type = %s",
                   buffer, len,
                   GET_FSAL_ACE_WHO_TYPE(*pace));

      utf8buffer.utf8string_val = buffer;
      utf8buffer.utf8string_len = strlen(buffer);

      if(nfs4_decode_acl_special_user(&utf8buffer, &who) == 0)  /* Decode special user. */
        {
          /* Clear group flag for special users */
          pace->flag &= ~(FSAL_ACE_FLAG_GROUP_ID);
          pace->iflag |= FSAL_ACE_IFLAG_SPECIAL_ID;
          pace->who.uid = who;
          LogFullDebug(COMPONENT_NFS_V4,
                       "SATTR: ACE special who.uid = 0x%x",
                       pace->who.uid);
        }
      else
        {
          if(pace->flag == FSAL_ACE_FLAG_GROUP_ID)  /* Decode group. */
            {
              utf82gid(&utf8buffer, &(pace->who.gid));
              LogFullDebug(COMPONENT_NFS_V4,
                           "SATTR: ACE who.gid = 0x%x",
                           pace->who.gid);
            }
          else  /* Decode user. */
            {
              utf82uid(&utf8buffer, &(pace->who.uid));
              LogFullDebug(COMPONENT_NFS_V4,
                           "SATTR: ACE who.uid = 0x%x",
                           pace->who.uid);
            }
        }

      /* Check if we can map a name string to uid or gid. If we can't, do cleanup
       * and bubble up NFS4ERR_BADOWNER. */
      if((pace->flag == FSAL_ACE_FLAG_GROUP_ID ? pace->who.gid : pace->who.uid) == -1)
        {
          LogFullDebug(COMPONENT_NFS_V4,
                       "SATTR: bad owner");
          nfs4_ace_free(acldata.aces);
          return NFS4ERR_BADOWNER;
        }
    }

  pacl = nfs4_acl_new_entry(&acldata, &status);
  pFSAL_attr->acl = pacl;
  if(pacl == NULL)
    {
      LogCrit(COMPONENT_NFS_V4,
              "SATTR: Failed to create a new entry for ACL");
      return NFS4ERR_SERVERFAULT;
    }
  else
     LogFullDebug(COMPONENT_NFS_V4,
                  "SATTR: Successfully created a new entry for ACL, status = %u",
                  status);

  /* Set new ACL */
  LogFullDebug(COMPONENT_NFS_V4,
               "SATTR: new acl = %p",
               pacl);

  return NFS4_OK;
}
#endif                          /* _USE_NFS4_ACL */

/**
 * 
 * nfs4_attrmap_To_FSAL_attrmask: Converts NFSv4 attribute bitmap to
 * FSAL attribute mask.
 * 
 * Converts NFSv4 attributes buffer to a FSAL attributes structure.
 *
 * @param attrmap  [IN]   pointer to NFSv4 attribute bitmap. 
 * @param attrmask [OUT]  pointer to FSAL attribute mask.
 * 
 * @return NFS4_OK if successful, NFS4ERR codes if not.
 *
 */
int nfs4_attrmap_to_FSAL_attrmask(bitmap4 attrmap, fsal_attrib_mask_t* attrmask)
{
  unsigned int offset = 0;
  unsigned int i = 0;
  char __attribute__ ((__unused__)) funcname[] = "nfs4_FattrToSattr";

  for(offset = 0; offset < attrmap.bitmap4_len; offset++)
    {
      for(i = 0; i < 32; i++)
        {
          if(attrmap.bitmap4_val[offset] & (1 << i)) {
            uint32_t val = i + 32 * offset;
            switch (val)
              {
              case FATTR4_TYPE:
                *attrmask |= FSAL_ATTR_TYPE;
                break;
              case FATTR4_FILEID:
                *attrmask |= FSAL_ATTR_FILEID;
                break;
              case FATTR4_FSID:
                *attrmask |= FSAL_ATTR_FSID;
                break;
              case FATTR4_NUMLINKS:
                *attrmask |= FSAL_ATTR_NUMLINKS;
                break;
              case FATTR4_SIZE:
                *attrmask |= FSAL_ATTR_SIZE;
                break;
              case FATTR4_MODE:
                *attrmask |= FSAL_ATTR_MODE;
                break;
              case FATTR4_OWNER:
                *attrmask |= FSAL_ATTR_OWNER;
                break;
              case FATTR4_OWNER_GROUP:
                *attrmask |= FSAL_ATTR_GROUP;
                break;
              case FATTR4_CHANGE:
                *attrmask |= FSAL_ATTR_CHGTIME;
                break;
              case FATTR4_RAWDEV:
                *attrmask |= FSAL_ATTR_RAWDEV;
                break;
              case FATTR4_SPACE_USED:
                *attrmask |= FSAL_ATTR_SPACEUSED;
                break;
              case FATTR4_TIME_ACCESS:
                *attrmask |= FSAL_ATTR_ATIME;
                break;
              case FATTR4_TIME_METADATA:
                *attrmask |= FSAL_ATTR_CTIME;
                break;
              case FATTR4_TIME_MODIFY:
                *attrmask |= FSAL_ATTR_MTIME;
                break;
              case FATTR4_TIME_ACCESS_SET:
                *attrmask |= FSAL_ATTR_ATIME;
                break;
              case FATTR4_TIME_MODIFY_SET:
                *attrmask |= FSAL_ATTR_MTIME;
                break;
              case FATTR4_FILEHANDLE:
                LogFullDebug(COMPONENT_NFS_V4,
                             "Filehandle attribute requested on readdir!");
                /* pFSAL_attr->asked_attributes |= FSAL_ATTR_FILEHANDLE; */
                break;
#ifdef _USE_NFS4_ACL
              case FATTR4_ACL:
                *attrmask |= FSAL_ATTR_ACL;
                break;
#endif                          /* _USE_NFS4_ACL */
              }
          }
        }
    }
  return NFS4_OK;
}                               /* nfs4_Fattr_To_FSAL_attr */

static int nfstime4_to_fsal_time(fsal_time_t *ts, const char *attrval)
{
  int LastOffset = 0;
  uint64_t seconds;
  uint32_t nseconds;

  memcpy(&seconds, attrval + LastOffset, sizeof(seconds));
  LastOffset += sizeof(seconds) ;

  memcpy(&nseconds, attrval + LastOffset, sizeof(nseconds));
  LastOffset += sizeof( nseconds ) ;

  ts->seconds = (uint32_t) nfs_ntohl64(seconds);
  ts->nseconds = (uint32_t) ntohl(nseconds);

  return LastOffset; 
}

static int settime4_to_fsal_time(fsal_time_t *ts, const char *attrval)
{
  time_how4 how;
  int LastOffset = 0;

  memcpy(&how, attrval + LastOffset , sizeof(how));
  LastOffset += sizeof(how);

  if(ntohl(how) == SET_TO_SERVER_TIME4)
    {
      ts->seconds = time(NULL);   /* Use current server's time */
      ts->nseconds = 0;
    }
  else
    {
        LastOffset += nfstime4_to_fsal_time(ts, attrval + LastOffset);
    }

  return LastOffset;
}

/**
 * 
 * Fattr4_To_FSAL_attr: Converts NFSv4 attributes buffer to a FSAL attributes structure.
 * 
 * Converts NFSv4 attributes buffer to a FSAL attributes structure.
 *
 * NB! If the pointer for the handle is provided the memory is not allocated,
 *     the handle's nfs_fh4_val points inside fattr4. The pointer is valid
 *     as long as fattr4 is valid.
 *
 * @param pFSAL_attr [OUT]  pointer to FSAL attributes.
 * @param Fattr      [IN] pointer to NFSv4 attributes. 
 * @param hdl4       [OUT] optional pointer to return NFSv4 file handle
 * 
 * @return NFS4_OK if successful, NFS4ERR codes if not.
 *
 */
int Fattr4_To_FSAL_attr(fsal_attrib_list_t * pFSAL_attr, fattr4 * Fattr, nfs_fh4 *hdl4)
{
  u_int LastOffset = 0;
  unsigned int i = 0;
  uint32_t attrmasklist[FATTR4_MOUNTED_ON_FILEID];      /* List cannot be longer than FATTR4_MOUNTED_ON_FILEID */
  uint32_t attrmasklen = 0;
  uint32_t attribute_to_set = 0;
  int len;
  char buffer[MAXNAMLEN];
  utf8string utf8buffer;

  fattr4_type attr_type;
  fattr4_fsid attr_fsid;
  fattr4_fileid attr_fileid;
  fattr4_rdattr_error rdattr_error;
  fattr4_size attr_size;
  fattr4_change attr_change;
  fattr4_numlinks attr_numlinks;
  fattr4_rawdev attr_rawdev;
  fattr4_space_used attr_space_used;
#ifdef _USE_NFS4_ACL
  int rc;
#endif

  if(pFSAL_attr == NULL || Fattr == NULL)
    return NFS4ERR_BADXDR;

  /* Check attributes data */
  if((Fattr->attr_vals.attrlist4_val == NULL) ||
     (Fattr->attr_vals.attrlist4_len == 0))
    return NFS4ERR_BADXDR;

  /* Convert the attribute bitmap to an attribute list */
  nfs4_bitmap4_to_list(&(Fattr->attrmask), &attrmasklen, attrmasklist);

  LogFullDebug(COMPONENT_NFS_V4,
               "   nfs4_bitmap4_to_list ====> attrmasklen = %d", attrmasklen);

  /* Init */
  pFSAL_attr->asked_attributes = 0;

  for(i = 0; i < attrmasklen; i++)
    {
      attribute_to_set = attrmasklist[i];

#ifdef _USE_NFS4_1
      if(attrmasklist[i] > FATTR4_FS_CHARSET_CAP)
#else
      if(attrmasklist[i] > FATTR4_MOUNTED_ON_FILEID)
#endif
        {
          /* Erroneous value... skip */
          continue;
        }
      LogFullDebug(COMPONENT_NFS_V4,
                   "=================> nfs4_Fattr_To_FSAL_attr: i=%u attr=%u", i,
                   attrmasklist[i]);
      LogFullDebug(COMPONENT_NFS_V4,
                   "Flag for Operation = %d|%d is ON,  name  = %s  reply_size = %d",
                   attrmasklist[i], fattr4tab[attribute_to_set].val,
                   fattr4tab[attribute_to_set].name,
                   fattr4tab[attribute_to_set].size_fattr4);

      switch (attribute_to_set)
        {
        case FATTR4_TYPE:
          memcpy((char *)&attr_type,
                 (char *)(Fattr->attr_vals.attrlist4_val + LastOffset),
                 sizeof(fattr4_type));

          switch (ntohl(attr_type))
            {
            case NF4REG:
              pFSAL_attr->type = FSAL_TYPE_FILE;
              break;

            case NF4DIR:
              pFSAL_attr->type = FSAL_TYPE_DIR;
              break;

            case NF4BLK:
              pFSAL_attr->type = FSAL_TYPE_BLK;
              break;

            case NF4CHR:
              pFSAL_attr->type = FSAL_TYPE_CHR;
              break;

            case NF4LNK:
              pFSAL_attr->type = FSAL_TYPE_LNK;
              break;

            case NF4SOCK:
              pFSAL_attr->type = FSAL_TYPE_SOCK;
              break;

            case NF4FIFO:
              pFSAL_attr->type = FSAL_TYPE_FIFO;
              break;

            default:
              /* For wanting of a better solution */
              return NFS4ERR_BADXDR;
            }                   /* switch( pattr->type ) */

          pFSAL_attr->asked_attributes |= FSAL_ATTR_TYPE;
          LastOffset += fattr4tab[attribute_to_set].size_fattr4;
          break;

        case FATTR4_FILEID:
          /* The analog to the inode number. RFC3530 says "a number uniquely identifying the file within the filesystem"
           * I use hpss_GetObjId to extract this information from the Name Server's handle */
          memcpy((char *)&attr_fileid,
                 (char *)(Fattr->attr_vals.attrlist4_val + LastOffset),
                 sizeof(fattr4_fileid));
          pFSAL_attr->fileid = nfs_ntohl64(attr_fileid);

          pFSAL_attr->asked_attributes |= FSAL_ATTR_FILEID;
          LastOffset += fattr4tab[attribute_to_set].size_fattr4;

          break;

        case FATTR4_FSID:
          memcpy((char *)&attr_fsid,
                 (char *)(Fattr->attr_vals.attrlist4_val + LastOffset),
                 sizeof(fattr4_fsid));
          pFSAL_attr->fsid.major = nfs_ntohl64(attr_fsid.major);
          pFSAL_attr->fsid.minor = nfs_ntohl64(attr_fsid.minor);

          pFSAL_attr->asked_attributes |= FSAL_ATTR_FSID;
          LastOffset += fattr4tab[attribute_to_set].size_fattr4;

          break;

        case FATTR4_NUMLINKS:
          memcpy((char *)&attr_numlinks,
                 (char *)(Fattr->attr_vals.attrlist4_val + LastOffset),
                 sizeof(fattr4_numlinks));
          pFSAL_attr->numlinks = ntohl(attr_numlinks);

          pFSAL_attr->asked_attributes |= FSAL_ATTR_NUMLINKS;
          LastOffset += fattr4tab[attribute_to_set].size_fattr4;

          break;

        case FATTR4_SIZE:
          memcpy((char *)&attr_size,
                 (char *)(Fattr->attr_vals.attrlist4_val + LastOffset),
                 sizeof(fattr4_size));

          /* Do not forget the XDR marshalling for the fattr4 stuff */
          pFSAL_attr->filesize = nfs_ntohl64(attr_size);

          pFSAL_attr->asked_attributes |= FSAL_ATTR_SIZE;
          LastOffset += fattr4tab[attribute_to_set].size_fattr4;
          LogFullDebug(COMPONENT_NFS_V4,
                       "      SATTR: size seen %zu", (size_t)pFSAL_attr->filesize);
          break;

        case FATTR4_MODE:
          memcpy((char *)&(pFSAL_attr->mode),
                 (char *)(Fattr->attr_vals.attrlist4_val + LastOffset),
                 sizeof(fattr4_mode));

          /* Do not forget the XDR marshalling for the fattr4 stuff */
          pFSAL_attr->mode = ntohl(pFSAL_attr->mode);

          pFSAL_attr->asked_attributes |= FSAL_ATTR_MODE;
          LastOffset += fattr4tab[attribute_to_set].size_fattr4;
          LogFullDebug(COMPONENT_NFS_V4,
                       "      SATTR: We see the mode 0%o", pFSAL_attr->mode);
          break;

        case FATTR4_OWNER:
          memcpy(&len, (char *)(Fattr->attr_vals.attrlist4_val + LastOffset),
                 sizeof(u_int));
          len = ntohl(len);     /* xdr marshalling on fattr4 */
          LastOffset += sizeof(u_int);

          memcpy(buffer, (char *)(Fattr->attr_vals.attrlist4_val + LastOffset), len);
          buffer[len] = '\0';

          /* Do not forget that xdr_opaque are aligned on 32bit long words */
          while((len % 4) != 0)
            len += 1;

          LastOffset += len;

          utf8buffer.utf8string_val = buffer;
          utf8buffer.utf8string_len = strlen(buffer);

          utf82uid(&utf8buffer, &(pFSAL_attr->owner));
          pFSAL_attr->asked_attributes |= FSAL_ATTR_OWNER;

          LogFullDebug(COMPONENT_NFS_V4,
                       "      SATTR: We see the owner %s len = %d", buffer, len);
          LogFullDebug(COMPONENT_NFS_V4,
                       "      SATTR: We see the owner %d", pFSAL_attr->owner);
          break;

        case FATTR4_OWNER_GROUP:
          memcpy(&len, (char *)(Fattr->attr_vals.attrlist4_val + LastOffset),
                 sizeof(u_int));
          len = ntohl(len);
          LastOffset += sizeof(u_int);

          memcpy(buffer, (char *)(Fattr->attr_vals.attrlist4_val + LastOffset), len);
          buffer[len] = '\0';

          /* Do not forget that xdr_opaque are aligned on 32bit long words */
          while((len % 4) != 0)
            len += 1;

          LastOffset += len;

          utf8buffer.utf8string_val = buffer;
          utf8buffer.utf8string_len = strlen(buffer);

          utf82gid(&utf8buffer, &(pFSAL_attr->group));
          pFSAL_attr->asked_attributes |= FSAL_ATTR_GROUP;

          LogFullDebug(COMPONENT_NFS_V4,
                       "      SATTR: We see the owner_group %s len = %d", buffer, len);
          LogFullDebug(COMPONENT_NFS_V4,
                       "      SATTR: We see the owner_group %d", pFSAL_attr->group);
          break;

        case FATTR4_CHANGE:
          memcpy((char *)&attr_change,
                 (char *)(Fattr->attr_vals.attrlist4_val + LastOffset),
                 sizeof(fattr4_change));
          pFSAL_attr->chgtime.seconds = (uint32_t) nfs_ntohl64(attr_change);
          pFSAL_attr->chgtime.nseconds = 0;

          pFSAL_attr->change =  nfs_ntohl64(attr_change);

          pFSAL_attr->asked_attributes |= FSAL_ATTR_CHGTIME;
          pFSAL_attr->asked_attributes |= FSAL_ATTR_CHANGE;
          LastOffset += fattr4tab[attribute_to_set].size_fattr4;

          break;

        case FATTR4_RAWDEV:
          memcpy((char *)&attr_rawdev,
                 (char *)(Fattr->attr_vals.attrlist4_val + LastOffset),
                 sizeof(fattr4_rawdev));
          pFSAL_attr->rawdev.major = (uint32_t) nfs_ntohl64(attr_rawdev.specdata1);
          pFSAL_attr->rawdev.minor = (uint32_t) nfs_ntohl64(attr_rawdev.specdata2);

          pFSAL_attr->asked_attributes |= FSAL_ATTR_RAWDEV;
          LastOffset += fattr4tab[attribute_to_set].size_fattr4;

          break;

        case FATTR4_SPACE_USED:
          memcpy((char *)&attr_space_used,
                 (char *)(Fattr->attr_vals.attrlist4_val + LastOffset),
                 sizeof(fattr4_space_used));
          pFSAL_attr->spaceused = (uint32_t) nfs_ntohl64(attr_space_used);

          pFSAL_attr->asked_attributes |= FSAL_ATTR_SPACEUSED;
          LastOffset += fattr4tab[attribute_to_set].size_fattr4;

          break;

        case FATTR4_TIME_ACCESS:       /* Used only by FSAL_PROXY to reverse convert */
          LastOffset += nfstime4_to_fsal_time(&pFSAL_attr->atime, 
                                              (char *)(Fattr->attr_vals.attrlist4_val + LastOffset));
          pFSAL_attr->asked_attributes |= FSAL_ATTR_ATIME;
          break;

        case FATTR4_TIME_METADATA:     /* Used only by FSAL_PROXY to reverse convert */
          LastOffset += nfstime4_to_fsal_time(&pFSAL_attr->ctime, 
                                              (char *)(Fattr->attr_vals.attrlist4_val + LastOffset));
          pFSAL_attr->asked_attributes |= FSAL_ATTR_CTIME;
          break;

        case FATTR4_TIME_MODIFY:       /* Used only by FSAL_PROXY to reverse convert */
          LastOffset += nfstime4_to_fsal_time(&pFSAL_attr->mtime, 
                                              (char *)(Fattr->attr_vals.attrlist4_val + LastOffset));
          pFSAL_attr->asked_attributes |= FSAL_ATTR_MTIME;
          break;

        case FATTR4_TIME_ACCESS_SET:
          LastOffset += settime4_to_fsal_time(&pFSAL_attr->atime,
                                              Fattr->attr_vals.attrlist4_val + LastOffset);
          pFSAL_attr->asked_attributes |= FSAL_ATTR_ATIME;
          break;

        case FATTR4_TIME_MODIFY_SET:
          LastOffset += settime4_to_fsal_time(&pFSAL_attr->mtime,
                                              Fattr->attr_vals.attrlist4_val + LastOffset);
          pFSAL_attr->asked_attributes |= FSAL_ATTR_MTIME;

          break;

        case FATTR4_FILEHANDLE:
          memcpy(&len, (char *)(Fattr->attr_vals.attrlist4_val + LastOffset),
                 sizeof(u_int));
          len = ntohl(len);
          LastOffset += sizeof(u_int);
          if(hdl4)
            {
               hdl4->nfs_fh4_len = len;
               hdl4->nfs_fh4_val = Fattr->attr_vals.attrlist4_val + LastOffset;
            }
          LastOffset += len;
          LogFullDebug(COMPONENT_NFS_V4,
                       "     SATTR: On a demande le filehandle len =%u", len);
          break;

        case FATTR4_RDATTR_ERROR:
          memcpy((char *)&rdattr_error,
                 (char *)(Fattr->attr_vals.attrlist4_val + LastOffset),
                 sizeof(fattr4_rdattr_error));
          rdattr_error = ntohl(rdattr_error);
          LastOffset += fattr4tab[attribute_to_set].size_fattr4;

          break;

#ifdef _USE_NFS4_ACL
        case FATTR4_ACL:
          if((rc = nfs4_decode_acl(pFSAL_attr, Fattr, &LastOffset)) != NFS4_OK)
            return rc;

          pFSAL_attr->asked_attributes |= FSAL_ATTR_ACL;
          break;
#endif                          /* _USE_NFS4_ACL */

        default:
          LogFullDebug(COMPONENT_NFS_V4,
                       "      SATTR: Attribut no supporte %d name=%s",
                       attribute_to_set, fattr4tab[attribute_to_set].name);
          LastOffset += fattr4tab[attribute_to_set].size_fattr4;
          /* return NFS4ERR_ATTRNOTSUPP ; *//* Should not stop processing */
          break;
        }                       /* switch */
    }                           /* for */

  return NFS4_OK;
}                               /* Fattr4_To_FSAL_attr */

/**
 * 
 * nfs4_Fattr_To_FSAL_attr: Converts NFSv4 attributes buffer to a FSAL attributes structure.
 * 
 * Converts NFSv4 attributes buffer to a FSAL attributes structure.
 *
 * @param pFSAL_attr [OUT]  pointer to FSAL attributes.
 * @param Fattr      [IN] pointer to NFSv4 attributes. 
 * 
 * @return NFS4_OK if successful, NFS4ERR codes if not.
 *
 */
int nfs4_Fattr_To_FSAL_attr(fsal_attrib_list_t * pFSAL_attr, fattr4 * Fattr)
{
  return Fattr4_To_FSAL_attr(pFSAL_attr, Fattr, NULL);
}

/* Error conversion routines */
/**
 * 
 * nfs4_Errno: Converts a cache_inode status to a nfsv4 status.
 * 
 *  Converts a cache_inode status to a nfsv4 status.
 *
 * @param error  [IN] Input cache inode ewrror.
 * 
 * @return the converted NFSv4 status.
 *
 */
nfsstat4 nfs4_Errno(cache_inode_status_t error)
{
  nfsstat4 nfserror= NFS4ERR_INVAL;

  switch (error)
    {
    case CACHE_INODE_SUCCESS:
      nfserror = NFS4_OK;
      break;

    case CACHE_INODE_MALLOC_ERROR:
    case CACHE_INODE_POOL_MUTEX_INIT_ERROR:
    case CACHE_INODE_GET_NEW_LRU_ENTRY:
    case CACHE_INODE_INIT_ENTRY_FAILED:
    case CACHE_INODE_CACHE_CONTENT_EXISTS:
    case CACHE_INODE_CACHE_CONTENT_EMPTY:
      nfserror = NFS4ERR_SERVERFAULT;
      break;

    case CACHE_INODE_UNAPPROPRIATED_KEY:
      nfserror = NFS4ERR_BADHANDLE;
      break;

    case CACHE_INODE_BAD_TYPE:
      nfserror = NFS4ERR_INVAL;
      break;

    case CACHE_INODE_INVALID_ARGUMENT:
      nfserror = NFS4ERR_PERM;
      break;

    case CACHE_INODE_NOT_A_DIRECTORY:
      nfserror = NFS4ERR_NOTDIR;
      break;

    case CACHE_INODE_ENTRY_EXISTS:
      nfserror = NFS4ERR_EXIST;
      break;

    case CACHE_INODE_DIR_NOT_EMPTY:
      nfserror = NFS4ERR_NOTEMPTY;
      break;

    case CACHE_INODE_NOT_FOUND:
      nfserror = NFS4ERR_NOENT;
      break;

    case CACHE_INODE_FSAL_ERROR:
    case CACHE_INODE_INSERT_ERROR:
    case CACHE_INODE_LRU_ERROR:
    case CACHE_INODE_HASH_SET_ERROR:
      nfserror = NFS4ERR_IO;
      break;

    case CACHE_INODE_FSAL_EACCESS:
      nfserror = NFS4ERR_ACCESS;
      break;

    case CACHE_INODE_FSAL_EPERM:
    case CACHE_INODE_FSAL_ERR_SEC:
      nfserror = NFS4ERR_PERM;
      break;

    case CACHE_INODE_NO_SPACE_LEFT:
      nfserror = NFS4ERR_NOSPC;
      break;

    case CACHE_INODE_IS_A_DIRECTORY:
      nfserror = NFS4ERR_ISDIR;
      break;

    case CACHE_INODE_READ_ONLY_FS:
      nfserror = NFS4ERR_ROFS;
      break;

    case CACHE_INODE_IO_ERROR:
      nfserror = NFS4ERR_IO;
      break;

     case CACHE_INODE_NAME_TOO_LONG:
      nfserror = NFS4ERR_NAMETOOLONG;
      break;

    case CACHE_INODE_KILLED:
    case CACHE_INODE_DEAD_ENTRY:
    case CACHE_INODE_FSAL_ESTALE:
      nfserror = NFS4ERR_STALE;
      break;

    case CACHE_INODE_STATE_CONFLICT:
      nfserror = NFS4ERR_PERM;
      break;

    case CACHE_INODE_QUOTA_EXCEEDED:
      nfserror = NFS4ERR_DQUOT;
      break;

    case CACHE_INODE_NOT_SUPPORTED:
      nfserror = NFS4ERR_NOTSUPP;
      break;

    case CACHE_INODE_DELAY:
      nfserror = NFS4ERR_DELAY;
      break;

    case CACHE_INODE_FILE_BIG:
      nfserror = NFS4ERR_FBIG;
      break;

    case CACHE_INODE_STATE_ERROR:
      nfserror = NFS4ERR_BAD_STATEID;
      break;

    case CACHE_INODE_BAD_COOKIE:
      nfserror = NFS4ERR_BAD_COOKIE;
      break;

    case CACHE_INODE_INCONSISTENT_ENTRY:
    case CACHE_INODE_HASH_TABLE_ERROR:
    case CACHE_INODE_CACHE_CONTENT_ERROR:
    case CACHE_INODE_ASYNC_POST_ERROR:
      /* Should not occur */
      nfserror = NFS4ERR_INVAL;
      break;
    }

  return nfserror;
}                               /* nfs4_Errno */

/**
 * 
 * nfs3_Errno: Converts a cache_inode status to a nfsv3 status.
 * 
 *  Converts a cache_inode status to a nfsv3 status.
 *
 * @param error  [IN] Input cache inode ewrror.
 * 
 * @return the converted NFSv3 status.
 *
 */
nfsstat3 nfs3_Errno(cache_inode_status_t error)
{
  nfsstat3 nfserror= NFS3ERR_INVAL;

  switch (error)
    {
    case CACHE_INODE_SUCCESS:
      nfserror = NFS3_OK;
      break;

    case CACHE_INODE_MALLOC_ERROR:
    case CACHE_INODE_POOL_MUTEX_INIT_ERROR:
    case CACHE_INODE_GET_NEW_LRU_ENTRY:
    case CACHE_INODE_UNAPPROPRIATED_KEY:
    case CACHE_INODE_INIT_ENTRY_FAILED:
    case CACHE_INODE_CACHE_CONTENT_EXISTS:
    case CACHE_INODE_CACHE_CONTENT_EMPTY:
    case CACHE_INODE_INSERT_ERROR:
    case CACHE_INODE_LRU_ERROR:
    case CACHE_INODE_HASH_SET_ERROR:
      LogCrit(COMPONENT_NFSPROTO,
              "Error %u converted to NFS3ERR_IO but was set non-retryable",
              error);
      nfserror = NFS3ERR_IO;
      break;

    case CACHE_INODE_INVALID_ARGUMENT:
      nfserror = NFS3ERR_INVAL;
      break;

    case CACHE_INODE_FSAL_ERROR:
    case CACHE_INODE_CACHE_CONTENT_ERROR:
                                         /** @todo: Check if this works by making stress tests */
      LogCrit(COMPONENT_NFSPROTO,
              "Error CACHE_INODE_FSAL_ERROR converted to NFS3ERR_IO but was set non-retryable");
      nfserror = NFS3ERR_IO;
      break;

    case CACHE_INODE_NOT_A_DIRECTORY:
      nfserror = NFS3ERR_NOTDIR;
      break;

    case CACHE_INODE_ENTRY_EXISTS:
      nfserror = NFS3ERR_EXIST;
      break;

    case CACHE_INODE_DIR_NOT_EMPTY:
      nfserror = NFS3ERR_NOTEMPTY;
      break;

    case CACHE_INODE_NOT_FOUND:
      nfserror = NFS3ERR_NOENT;
      break;

    case CACHE_INODE_FSAL_EACCESS:
      nfserror = NFS3ERR_ACCES;
      break;

    case CACHE_INODE_FSAL_EPERM:
    case CACHE_INODE_FSAL_ERR_SEC:
      nfserror = NFS3ERR_PERM;
      break;

    case CACHE_INODE_NO_SPACE_LEFT:
      nfserror = NFS3ERR_NOSPC;
      break;

    case CACHE_INODE_IS_A_DIRECTORY:
      nfserror = NFS3ERR_ISDIR;
      break;

    case CACHE_INODE_READ_ONLY_FS:
      nfserror = NFS3ERR_ROFS;
      break;

    case CACHE_INODE_KILLED:
    case CACHE_INODE_DEAD_ENTRY:
    case CACHE_INODE_FSAL_ESTALE:
      nfserror = NFS3ERR_STALE;
      break;

    case CACHE_INODE_QUOTA_EXCEEDED:
      nfserror = NFS3ERR_DQUOT;
      break;

    case CACHE_INODE_BAD_TYPE:
      nfserror = NFS3ERR_BADTYPE;
      break;

    case CACHE_INODE_NOT_SUPPORTED:
      nfserror = NFS3ERR_NOTSUPP;
      break;

    case CACHE_INODE_DELAY:
      nfserror = NFS3ERR_JUKEBOX;
      break;

    case CACHE_INODE_IO_ERROR:
        LogCrit(COMPONENT_NFSPROTO,
                "Error CACHE_INODE_IO_ERROR converted to NFS3ERR_IO but was set non-retryable");
      nfserror = NFS3ERR_IO;
      break;

    case CACHE_INODE_NAME_TOO_LONG:
      nfserror = NFS3ERR_NAMETOOLONG;
      break;

    case CACHE_INODE_FILE_BIG:
      nfserror = NFS3ERR_FBIG;
      break;

    case CACHE_INODE_BAD_COOKIE:
      nfserror = NFS3ERR_BAD_COOKIE;
      break;

    case CACHE_INODE_INCONSISTENT_ENTRY:
    case CACHE_INODE_HASH_TABLE_ERROR:
    case CACHE_INODE_STATE_CONFLICT:
    case CACHE_INODE_ASYNC_POST_ERROR:
    case CACHE_INODE_STATE_ERROR:
        /* Should not occur */
        LogDebug(COMPONENT_NFSPROTO,
                 "Line %u should never be reached in nfs3_Errno for cache_status=%u",
                 __LINE__, error);
      nfserror = NFS3ERR_INVAL;
      break;
    }

  return nfserror;
}                               /* nfs3_Errno */

/**
 * 
 * nfs2_Errno: Converts a cache_inode status to a nfsv2 status.
 * 
 *  Converts a cache_inode status to a nfsv2 status.
 *
 * @param error  [IN] Input cache inode ewrror.
 * 
 * @return the converted NFSv2 status.
 *
 */
nfsstat2 nfs2_Errno(cache_inode_status_t error)
{
  nfsstat2 nfserror= NFSERR_IO;

  switch (error)
    {
    case CACHE_INODE_SUCCESS:
      nfserror = NFS_OK;
      break;

    case CACHE_INODE_MALLOC_ERROR:
    case CACHE_INODE_POOL_MUTEX_INIT_ERROR:
    case CACHE_INODE_GET_NEW_LRU_ENTRY:
    case CACHE_INODE_UNAPPROPRIATED_KEY:
    case CACHE_INODE_INIT_ENTRY_FAILED:
    case CACHE_INODE_BAD_TYPE:
    case CACHE_INODE_CACHE_CONTENT_EXISTS:
    case CACHE_INODE_CACHE_CONTENT_EMPTY:
    case CACHE_INODE_INSERT_ERROR:
    case CACHE_INODE_LRU_ERROR:
    case CACHE_INODE_HASH_SET_ERROR:
    case CACHE_INODE_INVALID_ARGUMENT:
      LogCrit(COMPONENT_NFSPROTO,
              "Error %u converted to NFSERR_IO but was set non-retryable",
              error);
      nfserror = NFSERR_IO;
      break;

    case CACHE_INODE_NOT_A_DIRECTORY:
      nfserror = NFSERR_NOTDIR;
      break;

    case CACHE_INODE_ENTRY_EXISTS:
      nfserror = NFSERR_EXIST;
      break;

    case CACHE_INODE_FSAL_ERROR:
    case CACHE_INODE_CACHE_CONTENT_ERROR:
      LogCrit(COMPONENT_NFSPROTO,
              "Error CACHE_INODE_FSAL_ERROR converted to NFSERR_IO but was set non-retryable");
      nfserror = NFSERR_IO;
      break;

    case CACHE_INODE_DIR_NOT_EMPTY:
      nfserror = NFSERR_NOTEMPTY;
      break;

    case CACHE_INODE_NOT_FOUND:
      nfserror = NFSERR_NOENT;
      break;

    case CACHE_INODE_FSAL_EACCESS:
      nfserror = NFSERR_ACCES;
      break;

    case CACHE_INODE_NO_SPACE_LEFT:
      nfserror = NFSERR_NOSPC;
      break;

    case CACHE_INODE_FSAL_EPERM:
    case CACHE_INODE_FSAL_ERR_SEC:
      nfserror = NFSERR_PERM;
      break;

    case CACHE_INODE_IS_A_DIRECTORY:
      nfserror = NFSERR_ISDIR;
      break;

    case CACHE_INODE_READ_ONLY_FS:
      nfserror = NFSERR_ROFS;
      break;

    case CACHE_INODE_KILLED:
    case CACHE_INODE_DEAD_ENTRY:
    case CACHE_INODE_FSAL_ESTALE:
      nfserror = NFSERR_STALE;
      break;

    case CACHE_INODE_QUOTA_EXCEEDED:
      nfserror = NFSERR_DQUOT;
      break;

    case CACHE_INODE_IO_ERROR:
      LogCrit(COMPONENT_NFSPROTO,
              "Error CACHE_INODE_IO_ERROR converted to NFSERR_IO but was set non-retryable");
      nfserror = NFSERR_IO;
      break;

    case CACHE_INODE_NAME_TOO_LONG:
      nfserror = NFSERR_NAMETOOLONG;
      break;

    case CACHE_INODE_INCONSISTENT_ENTRY:
    case CACHE_INODE_HASH_TABLE_ERROR:
    case CACHE_INODE_STATE_CONFLICT:
    case CACHE_INODE_ASYNC_POST_ERROR:
    case CACHE_INODE_STATE_ERROR:
    case CACHE_INODE_NOT_SUPPORTED:
    case CACHE_INODE_DELAY:
    case CACHE_INODE_BAD_COOKIE:
    case CACHE_INODE_FILE_BIG:
        /* Should not occur */
      LogDebug(COMPONENT_NFSPROTO,
               "Line %u should never be reached in nfs2_Errno", __LINE__);
      nfserror = NFSERR_IO;
      break;
    }

  return nfserror;
}                               /* nfs2_Errno */

/**
 * 
 * nfs3_AllocateFH: Allocates a buffer to be used for storing a NFSv4 filehandle.
 * 
 * Allocates a buffer to be used for storing a NFSv3 filehandle.
 *
 * @param fh [INOUT] the filehandle to manage.
 * 
 * @return NFS3_OK if successful, NFS3ERR_SERVERFAULT, NFS3ERR_RESOURCE or NFS3ERR_STALE  otherwise.
 *
 */
int nfs3_AllocateFH(nfs_fh3 *fh)
{
  char __attribute__ ((__unused__)) funcname[] = "AllocateFH3";

  if(fh == NULL)
    return NFS3ERR_SERVERFAULT;

  /* Allocating the filehandle in memory */
  fh->data.data_len = sizeof(struct alloc_file_handle_v3);
  if((fh->data.data_val = gsh_malloc(fh->data.data_len)) == NULL)
    {
      LogError(COMPONENT_NFSPROTO, ERR_SYS, ERR_MALLOC, errno);
      return NFS3ERR_SERVERFAULT;
    }

  memset((char *)fh->data.data_val, 0, fh->data.data_len);

  return NFS3_OK;
}                               /* nfs4_AllocateFH */

/**
 * 
 * nfs4_AllocateFH: Allocates a buffer to be used for storing a NFSv4 filehandle.
 * 
 * Allocates a buffer to be used for storing a NFSv4 filehandle.
 *
 * @param fh [INOUT] the filehandle to manage.
 * 
 * @return NFS4_OK if successful, NFS3ERR_SERVERFAULT, NFS4ERR_RESOURCE or NFS4ERR_STALE  otherwise.
 *
 */
int nfs4_AllocateFH(nfs_fh4 * fh)
{
  char __attribute__ ((__unused__)) funcname[] = "AllocateFH4";

  if(fh == NULL)
    return NFS4ERR_SERVERFAULT;

  /* Allocating the filehandle in memory */
  fh->nfs_fh4_len = sizeof(struct alloc_file_handle_v4);
  if((fh->nfs_fh4_val = gsh_malloc(fh->nfs_fh4_len)) == NULL)
    {
      LogError(COMPONENT_NFS_V4, ERR_SYS, ERR_MALLOC, errno);
      return NFS4ERR_RESOURCE;
    }

  memset((char *)fh->nfs_fh4_val, 0, fh->nfs_fh4_len);

  return NFS4_OK;
}                               /* nfs4_AllocateFH */

/**
 *
 * nfs4_MakeCred
 *
 * This routine fills in the pcontext field in the compound data.
 *
 * @param pfh [INOUT] pointer to compound data to be used. NOT YET IMPLEMENTED
 *
 * @return NFS4_OK if successful, NFS4ERR_WRONGSEC otherwise.
 *
 */
int nfs4_MakeCred(compound_data_t * data)
{
  exportlist_client_entry_t related_client;
  struct user_cred user_credentials;

  if (get_req_uid_gid(data->reqp,
                      data->pexport,
                      &user_credentials) == FALSE)
    return NFS4ERR_WRONGSEC;

  LogFullDebug(COMPONENT_DISPATCH,
               "nfs4_MakeCred about to call nfs_export_check_access");
  if(nfs_export_check_access(&data->pworker->hostaddr,
                             data->reqp,
                             data->pexport,
                             nfs_param.core_param.program[P_NFS],
                             nfs_param.core_param.program[P_MNT],
                             data->pworker->ht_ip_stats,
                             ip_stats_pool,
                             &related_client,
                             &user_credentials,
                             FALSE) /* So check_access() doesn't deny based on whether this is a RO export. */
     == FALSE)
    return NFS4ERR_WRONGSEC;

  if(nfs_build_fsal_context(data->reqp,
                            data->pexport,
                            data->pcontext,
                            &user_credentials) == FALSE)
    return NFS4ERR_WRONGSEC;

  return NFS4_OK;
}                               /* nfs4_MakeCred */

/* Create access mask based on given access operation. Both mode and ace4
 * mask are encoded. */
fsal_accessflags_t nfs_get_access_mask(uint32_t op, fsal_attrib_list_t *pattr)
{
  fsal_accessflags_t access_mask = 0;

  switch(op)
    {
      case ACCESS3_READ:
        access_mask |= FSAL_MODE_MASK_SET(FSAL_R_OK);
        if(IS_FSAL_DIR(pattr->type))
          access_mask |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_LIST_DIR);
        else
          access_mask |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_READ_DATA);
      break;

      case ACCESS3_LOOKUP:
        if(!IS_FSAL_DIR(pattr->type))
          break;
        access_mask |= FSAL_MODE_MASK_SET(FSAL_X_OK);
        access_mask |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_LIST_DIR);
      break;

      case ACCESS3_MODIFY:
        access_mask |= FSAL_MODE_MASK_SET(FSAL_W_OK);
        if(IS_FSAL_DIR(pattr->type))
          access_mask |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_DELETE_CHILD);
        else
          access_mask |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_WRITE_DATA);
      break;

      case ACCESS3_EXTEND:
        access_mask |= FSAL_MODE_MASK_SET(FSAL_W_OK);
        if(IS_FSAL_DIR(pattr->type))
          access_mask |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_ADD_FILE |
                                            FSAL_ACE_PERM_ADD_SUBDIRECTORY);
        else
          access_mask |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_APPEND_DATA);
      break;

      case ACCESS3_DELETE:
        if(!IS_FSAL_DIR(pattr->type))
          break;
        access_mask |= FSAL_MODE_MASK_SET(FSAL_W_OK);
        access_mask |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_DELETE_CHILD);
      break;

      case ACCESS3_EXECUTE:
        if(IS_FSAL_DIR(pattr->type))
          break;
        access_mask |= FSAL_MODE_MASK_SET(FSAL_X_OK);
        access_mask |= FSAL_ACE4_MASK_SET(FSAL_ACE_PERM_EXECUTE);
      break;
    }

  return access_mask;
}

void nfs3_access_debug(char *label, uint32_t access)
{
  LogDebug(COMPONENT_NFSPROTO, "%s=%s,%s,%s,%s,%s,%s",
           label,
           FSAL_TEST_MASK(access, ACCESS3_READ) ? "READ" : "-",
           FSAL_TEST_MASK(access, ACCESS3_LOOKUP) ? "LOOKUP" : "-",
           FSAL_TEST_MASK(access, ACCESS3_MODIFY) ? "MODIFY" : "-",
           FSAL_TEST_MASK(access, ACCESS3_EXTEND) ? "EXTEND" : "-",
           FSAL_TEST_MASK(access, ACCESS3_DELETE) ? "DELETE" : "-",
           FSAL_TEST_MASK(access, ACCESS3_EXECUTE) ? "EXECUTE" : "-");
}

void nfs4_access_debug(char *label, uint32_t access, fsal_aceperm_t v4mask)
{
  LogDebug(COMPONENT_NFSPROTO, "%s=%s,%s,%s,%s,%s,%s",
           label,
           FSAL_TEST_MASK(access, ACCESS3_READ) ? "READ" : "-",
           FSAL_TEST_MASK(access, ACCESS3_LOOKUP) ? "LOOKUP" : "-",
           FSAL_TEST_MASK(access, ACCESS3_MODIFY) ? "MODIFY" : "-",
           FSAL_TEST_MASK(access, ACCESS3_EXTEND) ? "EXTEND" : "-",
           FSAL_TEST_MASK(access, ACCESS3_DELETE) ? "DELETE" : "-",
           FSAL_TEST_MASK(access, ACCESS3_EXECUTE) ? "EXECUTE" : "-");

  if(v4mask)
    LogDebug(COMPONENT_NFSPROTO, "v4mask=%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
             FSAL_TEST_MASK(v4mask, FSAL_ACE_PERM_READ_DATA)		 ? 'r':'-',
             FSAL_TEST_MASK(v4mask, FSAL_ACE_PERM_WRITE_DATA)		 ? 'w':'-',
             FSAL_TEST_MASK(v4mask, FSAL_ACE_PERM_EXECUTE)		 ? 'x':'-',
             FSAL_TEST_MASK(v4mask, FSAL_ACE_PERM_ADD_SUBDIRECTORY)    ? 'm':'-',
             FSAL_TEST_MASK(v4mask, FSAL_ACE_PERM_READ_NAMED_ATTR)	 ? 'n':'-',
             FSAL_TEST_MASK(v4mask, FSAL_ACE_PERM_WRITE_NAMED_ATTR) 	 ? 'N':'-',
             FSAL_TEST_MASK(v4mask, FSAL_ACE_PERM_DELETE_CHILD) 	 ? 'p':'-',
             FSAL_TEST_MASK(v4mask, FSAL_ACE_PERM_READ_ATTR)		 ? 't':'-',
             FSAL_TEST_MASK(v4mask, FSAL_ACE_PERM_WRITE_ATTR)		 ? 'T':'-',
             FSAL_TEST_MASK(v4mask, FSAL_ACE_PERM_DELETE)		 ? 'd':'-',
             FSAL_TEST_MASK(v4mask, FSAL_ACE_PERM_READ_ACL) 		 ? 'c':'-',
             FSAL_TEST_MASK(v4mask, FSAL_ACE_PERM_WRITE_ACL)		 ? 'C':'-',
             FSAL_TEST_MASK(v4mask, FSAL_ACE_PERM_WRITE_OWNER)	 ? 'o':'-',
             FSAL_TEST_MASK(v4mask, FSAL_ACE_PERM_SYNCHRONIZE)	 ? 'z':'-');
}

/**
 *
 * nfs4_sanity_check_FH: Do basic checks on a filehandle
 *
 * This function performs basic checks to make sure the supplied
 * filehandle is sane for a given operation.
 *
 * @param data          [IN] Compound_data_t for the operation to check
 * @param required_type [IN] The file type this operation requires.
 *                           Set to 0 to allow any type.
 *
 * @return NFSv4.1 status codes
 */

nfsstat4 nfs4_sanity_check_FH(compound_data_t *data,
                              cache_inode_file_type_t required_type)
{
  /* If there is no FH */
  if(nfs4_Is_Fh_Empty(&(data->currentFH)))
    {
      LogDebug(COMPONENT_FILEHANDLE,
               "nfs4_Is_Fh_Empty failed");
      return NFS4ERR_NOFILEHANDLE;
    }

  /* If the filehandle is invalid */
  if(nfs4_Is_Fh_Invalid(&(data->currentFH)))
    {
      LogDebug(COMPONENT_FILEHANDLE,
               "nfs4_Is_Fh_Invalid failed");
      return NFS4ERR_BADHANDLE;
    }

  /* Tests if the Filehandle is expired (for volatile filehandle) */
  if(nfs4_Is_Fh_Expired(&(data->currentFH)))
    {
      LogDebug(COMPONENT_FILEHANDLE,
               "nfs4_Is_Fh_Expired failed");
      return NFS4ERR_FHEXPIRED;
    }

  /* Check for the correct file type */
  if (required_type)
    {
      if(data->current_filetype != required_type)
        {
          LogDebug(COMPONENT_NFSPROTO,
                   "Wrong file type");

          if(required_type == DIRECTORY)
            return NFS4ERR_NOTDIR;
          if(required_type == SYMBOLIC_LINK)
            return NFS4ERR_INVAL;

          switch (data->current_filetype)
            {
            case DIRECTORY:
              return NFS4ERR_ISDIR;
            default:
              return NFS4ERR_INVAL;
            }
        }
    }

  return NFS4_OK;
}

