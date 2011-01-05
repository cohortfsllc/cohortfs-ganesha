/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010 The Linux Box, Inc.
 * Contributor : Adam C. Emerson <aemerson@linuxbox.com>
 *
 * Portions copyright CEA/DAM/DIF  (2008)
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
 * ------------- 
 */

/**
 * \file    fsal_tools.c
 * \brief   miscelaneous FSAL tools that can be called from outside.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"
#include "fsal_convert.h"
#include "config_parsing.h"
#include <string.h>

extern int h_errno;

/* case unsensitivity */
#define STRCMP   strcasecmp
#define low32m( a ) ( (unsigned int)a )

char *CEPHFSAL_GetFSName()
{
  return "CEPH";
}

/** 
 * FSAL_handlecmp:
 * Compare 2 handles.
 *
 * \param handle1 (input):
 *        The first handle to be compared.
 * \param handle2 (input):
 *        The second handle to be compared.
 * \param status (output):
 *        The status of the compare operation.
 *
 * \return - 0 if handles are the same.
 *         - A non null value else.
 *         - Segfault if status is a NULL pointer.
 */

int CEPHFSAL_handlecmp(cephfsal_handle_t * handle1,
		       cephfsal_handle_t * handle2,
		       fsal_status_t * status)
{

  fsal_u64_t fileid1, fileid2;

  *status = FSAL_STATUS_NO_ERROR;

  if(!handle1 || !handle2)
    {
      status->major = ERR_FSAL_FAULT;
      return -1;
    }

  if ((VINODE(handle1).ino.val == VINODE(handle2).ino.val) &&
      (VINODE(handle1).snapid.val == VINODE(handle2).snapid.val))
    return 0;
  else
    return 1;
}

/**
 * FSAL_Handle_to_HashIndex
 * This function is used for hashing a FSAL handle
 * in order to dispatch entries into the hash table array.
 *
 * \param p_handle      The handle to be hashed
 * \param cookie        Makes it possible to have different hash value for the
 *                      same handle, when cookie changes.
 * \param alphabet_len  Parameter for polynomial hashing algorithm
 * \param index_size    The range of hash value will be [0..index_size-1]
 *
 * \return The hash value
 */

unsigned int CEPHFSAL_Handle_to_HashIndex(cephfsal_handle_t * p_handle,
					  unsigned int cookie,
					  unsigned int alphabet_len,
					  unsigned int index_size)
{

  /* XXX Come up with a better hash */
  return (unsigned int)
    ((VINODE(p_handle).ino.val + VINODE(p_handle).snapid.val) %
     index_size); 
}

/*
 * FSAL_Handle_to_RBTIndex
 * This function is used for generating a RBT node ID
 * in order to identify entries into the RBT.
 *
 * \param p_handle      The handle to be hashed
 * \param cookie        Makes it possible to have different hash value for the
 *                      same handle, when cookie changes.
 *
 * \return The hash value
 */

unsigned int CEPHFSAL_Handle_to_RBTIndex(cephfsal_handle_t * p_handle,
					 unsigned int cookie)
{
  /* Come up with tastier hash */
  return (unsigned int)(0xABCD1234 ^ VINODE(p_handle).ino.val ^
			VINODE(p_handle).snapid.val ^ cookie);

}

/**
 * FSAL_DigestHandle :
 *  Convert an fsal_handle_t to a buffer
 *  to be included into NFS handles,
 *  or another digest.
 *
 * \param output_type (input):
 *        Indicates the type of digest to do.
 * \param in_fsal_handle (input):
 *        The handle to be converted to digest.
 * \param out_buff (output):
 *        The buffer where the digest is to be stored.
 *
 * \return The major code is ERR_FSAL_NO_ERROR is no error occured.
 *         Else, it is a non null value.
 */

fsal_status_t CEPHFSAL_DigestHandle(cephfsal_export_context_t * p_expcontext,   /* IN */
				    fsal_digesttype_t output_type,  /* IN */
				    cephfsal_handle_t * in_fsal_handle, /* IN */
				    caddr_t out_buff        /* OUT */
    )
{
  int ino32;

  /* sanity checks */
  if(!in_fsal_handle || !out_buff || !p_expcontext)
    ReturnCode(ERR_FSAL_FAULT, 0);

  switch (output_type)
    {
      /* Digested Handles */
    case FSAL_DIGEST_NFSV2:
      if (sizeof(VINODE(in_fsal_handle)) > FSAL_DIGEST_SIZE_HDLV2)
	ReturnCode(ERR_FSAL_TOOSMALL, 0);
    case FSAL_DIGEST_NFSV3:
      if(sizeof(VINODE(in_fsal_handle)) > FSAL_DIGEST_SIZE_HDLV3)
	ReturnCode(ERR_FSAL_TOOSMALL, 0);
      memcpy(out_buff, &VINODE(in_fsal_handle),
	     sizeof(VINODE(in_fsal_handle)));
      break;
    case FSAL_DIGEST_NFSV4:
      if(sizeof(VINODE(in_fsal_handle)) > FSAL_DIGEST_SIZE_HDLV4)
	ReturnCode(ERR_FSAL_TOOSMALL, 0);
      memcpy(out_buff, &(in_fsal_handle->data),
	     sizeof(in_fsal_handle->data));
      break;

      /* Integer IDs */
      
    case FSAL_DIGEST_FILEID2:
      if (sizeof(VINODE(in_fsal_handle).ino.val) > FSAL_DIGEST_SIZE_FILEID2)
	ReturnCode(ERR_FSAL_TOOSMALL, 0);
    case FSAL_DIGEST_FILEID3:
      if(sizeof(VINODE(in_fsal_handle).ino.val) > FSAL_DIGEST_SIZE_FILEID3)
	ReturnCode(ERR_FSAL_TOOSMALL, 0);
    case FSAL_DIGEST_FILEID4:
      if(sizeof(VINODE(in_fsal_handle).ino.val) > FSAL_DIGEST_SIZE_FILEID4)
	ReturnCode(ERR_FSAL_TOOSMALL, 0);
      *((uint64_t* )out_buff)=VINODE(in_fsal_handle).ino.val;
	 
      break;

    default:
      ReturnCode(ERR_FSAL_SERVERFAULT, 0);
    }


  ReturnCode(ERR_FSAL_NO_ERROR, 0);

}                               /* FSAL_DigestHandle */

/**
 * FSAL_ExpandHandle :
 *  Convert a buffer extracted from NFS handles
 *  to an FSAL handle.
 *
 * \param in_type (input):
 *        Indicates the type of digest to be expanded.
 * \param in_buff (input):
 *        Pointer to the digest to be expanded.
 * \param out_fsal_handle (output):
 *        The handle built from digest.
 *
 * \return The major code is ERR_FSAL_NO_ERROR is no error occured.
 *         Else, it is a non null value.
 */
fsal_status_t CEPHFSAL_ExpandHandle(cephfsal_export_context_t * p_expcontext,   /* IN */
				    fsal_digesttype_t in_type,      /* IN */
				    caddr_t in_buff,        /* IN */
				    cephfsal_handle_t * out_fsal_handle /* OUT */
    )
{

  /* sanity checks */
  if(!out_fsal_handle || !in_buff)
    ReturnCode(ERR_FSAL_FAULT, 0);

  memset(out_fsal_handle, sizeof(cephfsal_handle_t), 0);

  switch (in_type)
   {
    case FSAL_DIGEST_NFSV2:
    case FSAL_DIGEST_NFSV3:
      memcpy(&(VINODE(out_fsal_handle)), in_buff,
	     sizeof(VINODE(out_fsal_handle)));
      break;
    case FSAL_DIGEST_NFSV4:
      memcpy(&(out_fsal_handle->data), in_buff,
	     sizeof(out_fsal_handle->data));
      break;

    default:
      /* Invalid input digest type. */
      ReturnCode(ERR_FSAL_INVAL, 0);
    }

  ReturnCode(ERR_FSAL_NO_ERROR, 0);
}

/**
 * Those routines set the default parameters
 * for FSAL init structure.
 * \return ERR_FSAL_NO_ERROR (no error),
 *         ERR_FSAL_FAULT (null pointer given as parameter),
 *         ERR_FSAL_SERVERFAULT (unexpected error)
 */
fsal_status_t CEPHFSAL_SetDefault_FSAL_parameter(fsal_parameter_t * out_parameter)
{


  /* defensive programming... */
  if(out_parameter == NULL)
    ReturnCode(ERR_FSAL_FAULT, 0);

  /* init max FS calls = unlimited */
  out_parameter->fsal_info.max_fs_calls = 0;

  ReturnCode(ERR_FSAL_NO_ERROR, 0);

}

fsal_status_t CEPHFSAL_SetDefault_FS_common_parameter(fsal_parameter_t * out_parameter)
{
  /* defensive programming... */
  if(out_parameter == NULL)
    ReturnCode(ERR_FSAL_FAULT, 0);

  /* set default values for all parameters of fs_common_info */

  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, maxfilesize);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, maxlink);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, maxnamelen);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, maxpathlen);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, no_trunc);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, chown_restricted);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, case_insensitive);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, case_preserving);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, fh_expire_type);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, link_support);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, symlink_support);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, named_attr);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, unique_handles);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, lease_time);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, acl_support);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, cansettime);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, homogenous);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, supported_attrs);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, maxread);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, maxwrite);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, umask);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, auth_exportpath_xdev);
  FSAL_SET_INIT_DEFAULT(out_parameter->fs_common_info, xattr_access_rights);

  ReturnCode(ERR_FSAL_NO_ERROR, 0);

}

fsal_status_t CEPHFSAL_SetDefault_FS_specific_parameter(fsal_parameter_t * out_parameter)
{
  /* defensive programming... */
  if(out_parameter == NULL)
    ReturnCode(ERR_FSAL_FAULT, 0);

  memset(&(out_parameter->fs_specific_info), 0,
	 sizeof(out_parameter->fs_specific_info));

  strcpy(out_parameter->fs_specific_info.cephserver, "localhost");
  
  ReturnCode(ERR_FSAL_NO_ERROR, 0);

}

/**
 * FSAL_load_FSAL_parameter_from_conf,
 * FSAL_load_FS_common_parameter_from_conf,
 * FSAL_load_FS_specific_parameter_from_conf:
 *
 * Those functions initialize the FSAL init parameter
 * structure from a configuration structure.
 *
 * \param in_config (input):
 *        Structure that represents the parsed configuration file.
 * \param out_parameter (ouput)
 *        FSAL initialization structure filled according
 *        to the configuration file given as parameter.
 *
 * \return ERR_FSAL_NO_ERROR (no error) ,
 *         ERR_FSAL_NOENT (missing a mandatory stanza in config file),
 *         ERR_FSAL_INVAL (invalid parameter),
 *         ERR_FSAL_SERVERFAULT (unexpected error)
 *         ERR_FSAL_FAULT (null pointer given as parameter),
 */

/* load FSAL init info */

fsal_status_t CEPHFSAL_load_FSAL_parameter_from_conf(config_file_t in_config,
						     fsal_parameter_t * out_parameter)
{
  int err;
  int var_max, var_index;
  char *key_name;
  char *key_value;
  config_item_t block;

  int DebugLevel = -1;
  char *LogFile = NULL;

  block = config_FindItemByName(in_config, CONF_LABEL_FSAL);

  /* cannot read item */

  if(block == NULL)
    ReturnCode(ERR_FSAL_NOENT, 0);
  else if(config_ItemType(block) != CONFIG_ITEM_BLOCK)
    ReturnCode(ERR_FSAL_INVAL, 0);

  /* read variable for fsal init */

  var_max = config_GetNbItems(block);

  for(var_index = 0; var_index < var_max; var_index++)
    {
      config_item_t item;

      item = config_GetItemByIndex(block, var_index);

      err = config_GetKeyValue(item, &key_name, &key_value);
      if(err)
        {
          ReturnCode(ERR_FSAL_SERVERFAULT, err);
        }

      if(!STRCMP(key_name, "DebugLevel"))
        {
          DebugLevel = ReturnLevelAscii(key_value);

          if(DebugLevel == -1)
            {
              ReturnCode(ERR_FSAL_INVAL, -1);
            }

        }
      else if(!STRCMP(key_name, "LogFile"))
        {

          LogFile = key_value;

        }
      else if(!STRCMP(key_name, "Max_FS_calls"))
        {

          int maxcalls = s_read_int(key_value);

          if(maxcalls < 0)
            {
              ReturnCode(ERR_FSAL_INVAL, 0);
            }

          out_parameter->fsal_info.max_fs_calls = (unsigned int)maxcalls;

        }
      else
        {
          ReturnCode(ERR_FSAL_INVAL, 0);
        }

    }


  ReturnCode(ERR_FSAL_NO_ERROR, 0);

}                               /* FSAL_load_FSAL_parameter_from_conf */

/* load general filesystem configuration options */

fsal_status_t CEPHFSAL_load_FS_common_parameter_from_conf(config_file_t in_config,
							  fsal_parameter_t * out_parameter)
{
  int err;
  int var_max, var_index;
  char *key_name;
  char *key_value;
  config_item_t block;

  block = config_FindItemByName(in_config, CONF_LABEL_FS_COMMON);

  /* cannot read item */
  if(block == NULL)
    {
      ReturnCode(ERR_FSAL_NOENT, 0);
    }
  else if(config_ItemType(block) != CONFIG_ITEM_BLOCK)
    {
      ReturnCode(ERR_FSAL_INVAL, 0);
    }

  ReturnCode(ERR_FSAL_NO_ERROR, 0);

}                               /* FSAL_load_FS_common_parameter_from_conf */

/* load specific filesystem configuration options */

fsal_status_t CEPHFSAL_load_FS_specific_parameter_from_conf(config_file_t in_config,
							    fsal_parameter_t * out_parameter)
{
  int err;
  int blk_index;
  int var_max, var_index;
  char *key_name;
  char *key_value;
  config_item_t block;

  block = config_FindItemByName(in_config, CONF_LABEL_FS_SPECIFIC);

  /* cannot read item */
  if(block == NULL)
    {
      ReturnCode(ERR_FSAL_NOENT, 0);
    }
  else if(config_ItemType(block) != CONFIG_ITEM_BLOCK)
    {
      ReturnCode(ERR_FSAL_INVAL, 0);
    }

  /* makes an iteration on the (key, value) couplets */

  var_max = config_GetNbItems(block);

  for(var_index = 0; var_index < var_max; var_index++)
    {
      config_item_t item;

      item = config_GetItemByIndex(block, var_index);

      err = config_GetKeyValue(item, &key_name, &key_value);
      if(err)
        {
          ReturnCode(ERR_FSAL_SERVERFAULT, err);
        }

      /* what parameter is it ? */

      if(!STRCMP(key_name, "cephserver"))
        {
          strncpy(out_parameter->fs_specific_info.cephserver,
		  key_value, FSAL_MAX_NAME_LEN);
        }
#ifdef _USE_CBREP
      else if (!STRCMP(key_name, "replica_servers"))
	{
	  /* This is quick and bad and dirty and must be fixed later.
	     Most notably it should support IPv6, right now it
	     doesn't.  This isn't really an issue since in production
	     we won't have a list of replicas in the Ganesha config
	     file, this is only to let us run our proof of concept. */
	  
	  char hostnamebuf[FSAL_MAX_NAME_LEN*MAXREP];
	  char* host = NULL;
	  char* saveptr = NULL;
	  int replicas = 0;
	  struct hostent* he;
	  
	  memset(hostnamebuf, 0, sizeof(hostnamebuf));
	  strncpy(hostnamebuf, key_value, sizeof(hostnamebuf));

	  host = strtok_r(hostnamebuf, ", \t\n\r", &saveptr);
	  while (host)
	    {
	      strncpy(out_parameter->fs_specific_info.replica_servers[replicas],
		      host,
		      16);
	      replicas++;
	      host = strtok_r(NULL, ", \t\n\r", &saveptr);
	    }
	  out_parameter->fs_specific_info.replicas = replicas;
	}
      else if (!STRCMP(key_name, "replication_master"))
	{
          switch (StrToBoolean(key_value))
            {
            case 1:
	      out_parameter->fs_specific_info.replication_master = true;
              break;

            case 0:
	      out_parameter->fs_specific_info.replication_master = true;
              break;
            default:           /* error */
              {
                LogCrit(COMPONENT_CONFIG,
                     "NFS READ CEPH ERROR: Invalid value for %s (%s): TRUE or FALSE expected.",
			key_name, key_value);
                continue;
              }
            }
	}
#endif
      else
	{
	  ReturnCode(ERR_FSAL_INVAL, 0);
	}
    }
  ReturnCode(ERR_FSAL_NO_ERROR, 0);
}
