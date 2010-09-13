/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010 The Linux Box Corporation
 * Contributor: Adam C. Emerson
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
 * \file    fsal_mds.c
 * \brief   MDS realisation for the filesystem abstraction
 *
 * fsal_mds.c: MDS realisation for the filesystem abstraction
 *             Obviously, all of these functions should dispatch
 *             on type if more than one layout type is supported.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_internal.h"
#include "fsal_convert.h"
#include "nfsv41.h"
#include <ceph/libceph.h>
#include <fcntl.h>
#include "HashTable.h"
#include <pthread.h>
#include "layouttypes/layouts.h"
#include "layouttypes/filelayout.h"
#include "stuff_alloc.h"
#include "fsal_types.h"

#define max(a,b)	  \
  ({ typeof (a) _a = (a); \
    typeof (b) _b = (b);  \
    _a > _b ? _a : _b; })


hash_table_t* deviceidtable;
pthread_mutex_t deviceidtablemutex =
  PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP; 

char tcpmark[]="tcp";
char nfsport[]=".8.01";

#define ADDRLENGTH 24 /* six groups of at most three digits, five
			 dots, one null. */

/* Functions for working with the storage of deviceinfo */

/*
 * thentry->inode must be set when this function is called.  It sets
 * inode->generation.
 */

int add_entry(deviceaddrinfo* thentry)
{
  int rc;
  hash_buffer_t key, value;
  deviceaddrinfo* cur;

  thentry->next=NULL;

  rc=pthread_mutex_lock(&deviceidtablemutex);
  if (rc != 0)
    return rc;

  key.pdata=(caddr_t) &(thentry->inode);
  key.len=sizeof(uint64_t);
  rc = HashTable_Get(deviceidtable, &key, &value);
  if (rc == HASHTABLE_ERROR_NO_SUCH_KEY)
    {
      /* No entries for this inode, we're the first */

      thentry->generation=0;
      value.pdata=(caddr_t) thentry;
      value.len=thentry->entry_size=sizeof(deviceaddrinfo);
      rc = HashTable_Test_And_Set(deviceidtable, &key, &value,
				 HASHTABLE_SET_HOW_SET_NO_OVERWRITE);
      if (rc != HASHTABLE_SUCCESS)
	{
	  pthread_mutex_unlock(&deviceidtablemutex);
	  return -1;
	}
    }
  else if (rc != HASHTABLE_SUCCESS)
    {
      pthread_mutex_unlock(&deviceidtablemutex);
      return -1;
    }
  else
    {
      /* Find the last entry and be one after */

      while (cur->next != NULL)
	cur = cur->next;
      
      thentry->generation = cur->generation + 1;
      cur->next = thentry;
    }
  
  pthread_mutex_unlock(&deviceidtablemutex);
  return 0;
}

/*
 * Unlinks an entry from the table but does not deallocate or modify
 * it
 */

int remove_entry(deviceaddrinfo* thentry)
{
  int rc;
  hash_buffer_t key, value;
  deviceaddrinfo* cur;

  rc=pthread_mutex_lock(&deviceidtablemutex);

  if (rc != 0)
    return rc;

  key.pdata=(caddr_t) &(thentry->inode);

  rc = HashTable_Get(deviceidtable, &key, &value);
  if (rc == HASHTABLE_ERROR_NO_SUCH_KEY)
    {
      pthread_mutex_unlock(&deviceidtablemutex);
      return -1;
    }
  if (value.pdata == (caddr_t) thentry && thentry->next == NULL)
    {
      /* Simple case, we're the only entry, delete it */

      rc = HashTable_Del(deviceidtable, &key, NULL, NULL);
      if (rc != HASHTABLE_SUCCESS)
	{
	  pthread_mutex_unlock(&deviceidtablemutex);
	  return -1;
	}
    }
  else if (value.pdata == (caddr_t) thentry)
    {
      /* Replace the head */
      value.pdata=(caddr_t) thentry->next;
      rc = HashTable_Test_And_Set(deviceidtable, &key, &value,
				 HASHTABLE_SET_HOW_SET_OVERWRITE);
      
      if (rc != HASHTABLE_SUCCESS)
	{
	  pthread_mutex_unlock(&deviceidtablemutex);
	  return -1;
	}
    }
  else
    {
      /* Hunt for it */

      cur=(deviceaddrinfo*) value.pdata;

      while (cur->next != thentry &&
	     cur->next != NULL)
	cur = cur->next;

      if (cur->next == NULL)
	{
	  pthread_mutex_unlock(&deviceidtablemutex);
	  return -1;
	}

      cur->next=thentry->next;
    }
  pthread_mutex_unlock(&deviceidtablemutex);
  return 0;
}

/*
 * Returns a pointer to an entry specified by inode and generation
 * number, NULL on not found.
 */

deviceaddrinfo* get_entry(uint64_t inode, uint64_t generation)
{
  int rc;
  hash_buffer_t key, value;
  deviceaddrinfo* cur;

  rc=pthread_mutex_lock(&deviceidtablemutex);

  if (rc != 0)
    return NULL;

  key.pdata=(caddr_t) &inode;
  rc = HashTable_Get(deviceidtable, &key, &value);

  if (rc != HASHTABLE_SUCCESS)
    {
      pthread_mutex_unlock(&deviceidtablemutex);
      return NULL;
    }

  cur=(deviceaddrinfo*) value.pdata;

  while (cur != NULL && cur->generation != generation)
    cur = cur->next;

  pthread_mutex_unlock(&deviceidtablemutex);
  return cur;
}
  
/**
 *
 * FSAL_layoutget: The NFSv4.1 LAYOUTGET operation
 *
 * Return a layout for the requested range on the given filehandle.
 *
 * \param filehandle (input):
 *        Handle of the file on which the layout is requested.
 * \param type (input):
 *        The type of layout requested
 * \param iomode (input):
 *        The iomode requested
 * \param offset (input):
 *        The beginning requested
 * \param length (input):
 *        The length requested
 * \param minlength (input):
 *        The minimum length required
 * \param layouts (output):
 *        Pointer to a buffer allocated by this function beginning
 *        with numlayouts layouts.  The following space is to hold
 *        variable-sized structures referenced in the layouts.
 * \param numlayouts (output):
 *        The number of layouts returned
 * \param return_on_close (output):
 *        Return on close flag.  To make this profitable, FSAL_close
 *        would need a means to trigger a layoutrecall.
 * \param context (input):
 *        Credential information
 * \param cbcookie (input):
 *        Opaque, passed to callbacks.
 *
 * \return Error codes or ERR_FSAL_NO_ERROR
 */

fsal_status_t CEPHFSAL_layoutget(cephfsal_handle_t* filehandle,
				 fsal_layouttype_t type,
				 fsal_layoutiomode_t iomode,
				 fsal_off_t offset, fsal_size_t length,
				 fsal_size_t minlength,
				 fsal_layout_t** layouts,
				 int *numlayouts,
				 fsal_boolean_t *return_on_close,
				 cephfsal_op_context_t *context,
				 void* cbcookie)
{
  struct stat_precise st;
  char name[255];
  int rc;
  uint32_t su;
  off_t filesize;
  deviceaddrinfo* entry;
  uint64_t stripes;
  int num_osds;
  uint32_t *stripe_indices;
  size_t reserved_size;
  fsal_file_dsaddr_t* deviceaddr;
  uint64_t i;
  multipath_list4* hostlists;
  netaddr4* hosts;
  char* stringwritepos;
  fsal_filelayout_t* fileloc;
  fsal_size_t biggest;
  
  /* Align the layout to ceph stripe boundaries */
  
  su=ceph_ll_stripe_unit(VINODE(filehandle));

  if (su==(uint32_t) -ESTALE)
    {
      Return(ERR_FSAL_STALE, 0, INDEX_FSAL_layoutget);
    }
  
  *numlayouts=1;
  *return_on_close = false;
  offset -= offset % su;

  rc=ceph_ll_getattr_precise(VINODE(filehandle), &st, -1, -1);

  if (rc < 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_layoutget);
  
  filesize=st.st_size;

  /* With the address hack we're using now, we want to put a brake on
     how large a layout someone can request */

  biggest = max((2 * filesize),
		((fsal_size_t) 1 << 0x1e));

  if (minlength > biggest)
    Return(ERR_FSAL_DELAY, 0, INDEX_FSAL_layoutget);
  
  if (length > biggest) 
    length = biggest;

  length -= (length % su);

  /* Constants needed to populate anything */
  
  num_osds=ceph_ll_num_osds();
  stripes=(length-offset)/su;
  

  /* Populate the device info */

  reserved_size = (sizeof(deviceaddrinfo) +
		   sizeof(nfsv4_1_file_layout_ds_addr4) +
		   (sizeof(uint32_t) * stripes) +
		   (sizeof(multipath_list4) * num_osds) +
		   (sizeof(netaddr4) * num_osds) +
		   ADDRLENGTH * num_osds);
		      
  entry = (deviceaddrinfo*) Mem_Alloc(reserved_size);
  if (entry==NULL)
    Return(ERR_FSAL_NOMEM, 0, INDEX_FSAL_layoutget);
  
  entry->inode=VINODE(filehandle).ino.val;
  memset(entry, reserved_size, 0);

  reserved_size -= sizeof(deviceaddrinfo);
  
  entry->entry_size = reserved_size;
  deviceaddr
    = entry->addrinfo
    = (fsal_file_dsaddr_t*) (entry+sizeof(deviceaddrinfo));
  deviceaddr->nflda_stripe_indices.nflda_stripe_indices_len=stripes;
  stripe_indices
    = deviceaddr->nflda_stripe_indices.nflda_stripe_indices_val
    = (int*) (deviceaddr + sizeof(fsal_file_dsaddr_t));
  
  for (i=0; i < stripes; i++)
    {
      int stripe_osd;

      stripe_osd=ceph_ll_get_stripe_osd(VINODE(filehandle), i);

      if (stripe_osd < 0)
	{
	  Mem_Free(entry);
	  Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_layoutget);
	}
      stripe_indices[i]=stripe_osd;
    }

  deviceaddr->nflda_multipath_ds_list.nflda_multipath_ds_list_len
    = num_osds;
  hostlists
    = deviceaddr->nflda_multipath_ds_list.nflda_multipath_ds_list_val
    = (multipath_list4*) (stripe_indices + sizeof(uint32_t) * stripes);

  hosts = (netaddr4*) (hostlists + num_osds * sizeof(multipath_list4));

  stringwritepos = (char*) (hosts + num_osds * sizeof(netaddr4));

  for(i = 0; i < num_osds; i++)
    {
      hostlists[i].multipath_list4_len=1;
      hostlists[i].multipath_list4_val=&(hosts[i]);
      hosts[i].na_r_netid=tcpmark;
      hosts[i].na_r_addr=stringwritepos;
      ceph_ll_osdaddr(i, stringwritepos, ADDRLENGTH);
      strcat(stringwritepos, nfsport);
      stringwritepos += (strlen(stringwritepos)+1);
    }

  if (add_entry(entry) != 0)
    {
      Mem_Free(entry);
      Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_layoutget);
    }

  /* Add the layout to the state for the file */
  
  if (FSALBACK_layout_add_state(type, iomode, offset, length,
				entry, *return_on_close, cbcookie) != 0)
    {
      remove_entry(entry);
      Mem_Free(entry);
      Return(ERR_FSAL_DELAY, 0, INDEX_FSAL_layoutget);
    }

  
  /* Build the layout to return to the client */

  reserved_size=(sizeof(fsal_layout_t) +
		 sizeof(layout_content4) +
		 sizeof(fsal_dsfh_t) +
		 NFS4_FHSIZE +
		 64);

  if ((*layouts=(fsal_layout_t*) Mem_Alloc(reserved_size)) == NULL)
    Return(ERR_FSAL_NOMEM, 0, INDEX_FSAL_layoutget);

  (*layouts)->lo_offset=offset;
  (*layouts)->lo_length=length;
  (*layouts)->lo_iomode=iomode;
  (*layouts)->lo_content.loc_body.loc_body_val
    = (char*) *layouts+sizeof(fsal_layout_t);

  reserved_size -=(sizeof(fsal_layout_t) +
		   sizeof(layout_content4));

  memcpy(fileloc->deviceid,
	 &(entry->inode),
	 sizeof(uint64_t));
  memcpy((fileloc->deviceid)+sizeof(uint64_t),
	 &(entry->generation),
	 sizeof(uint64_t));

  /* We are returning sparse layouts with commit-through-DS */
  
  fileloc->util=su;

  /* The zeroeth stripe represents first block at the given offset. */

  fileloc->first_stripe_index=0;

  fileloc->pattern_offset=offset;

  /* We return exactly one filehandle */

  fileloc->fhn = 1;

  fileloc->fhs = (fsal_dsfh_t*) (fileloc+sizeof(fsal_filelayout_t));

  fileloc->fhs->nfs_fh4_val=
    (char*) (fileloc->fhs+sizeof(fsal_dsfh_t));

  /* Give the client a filehandle that may be sent to the DS */
  
  FSALBACK_fh2dshandle(filehandle, fileloc->fhs, cbcookie);

  if (!(encode_lo_content(LAYOUT4_NFSV4_1_FILES,
			  &((*layouts)->lo_content),
			  reserved_size, fileloc)))
    {
      Mem_Free(*layouts);
      remove_entry(entry);
      Mem_Free(entry);
      Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_layoutget);
    }
			

  /* Obviously, change this when real code is added */

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_layoutget);
}

/**
 *
 * FSAL_layoutreturn: The NFSv4.1 LAYOUTRETURN operation
 *
 * Free a layout on a given file.  This routine will always be
 * passed exactly a layout granted by LAYOUTGET (retrieved from the
 * state table.)
 *
 * \param filehandle (input):
 *        The handle upon which the layout was granted
 * \param type (input):
 *        The layout type
 * \param passed_iomode (input):
 *        The iomode passed by the client (in case it's ANY)
 * \param passed_offset (input):
 *        The offset (specified by the client)
 * \param passed_length (input):
 *        The length (specified by the client)
 * \param found_iomode (input):
 *        The iomode (of the layout found in the state table)
 * \param found_offset (input):
 *        The offset (of the layout found in the state table)
 * \param found_length (input):
 *        The length (of the layout found in the state table)
 * \param ldata (input):
 *        FSAL-specific layout data
 * \param context (input):
 *        The authentication context
 * \param cbcookie (input):
 *        Opaque to be passed to callbacks
 *
 * \return Error codes or ERR_FSAL_NO_ERROR
 *
 */

fsal_status_t CEPHFSAL_layoutreturn(cephfsal_handle_t* filehandle,
				    fsal_layouttype_t type,
				    fsal_layoutiomode_t passed_iomode,
				    fsal_off_t passed_offset,
				    fsal_size_t passed_length,
				    fsal_size_t found_iomode,
				    fsal_off_t found_offset,
				    fsal_size_t found_length,
				    cephfsal_layoutdata_t ldata,
				    fsal_op_context_t* context,
				    void* cbcookie)
{
  /* Perform FSAL specific tasks to release the layout */

  if ((passed_offset > found_offset) ||
      (passed_length < found_length))
    Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_layoutreturn);
    
  if (remove_entry(ldata) != 0)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_layoutreturn);

  if (FSALBACK_layout_remove_state(cbcookie) != 0)
    Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_layoutreturn);
  
  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_layoutreturn);
}

/**
 *
 * FSAL_layoutcommit: The NFSv4.1 LAYOUTCOMMIT operation
 *
 * Commit changes made on the DSs to the MDS
 *
 * \param filehandle (input):
 *        The filehandle in question
 * \param type (input):
 *        The layout type
 * \param layout (input):
 *        The layout itself
 * \param layout_length (input):
 *        Length of the layout
 * \param offset (input):
 *        Offset into the file of the changed portion
 * \param length (input):
 *        Length of the changed portion
 * \param newoff (input/output):
 *        Client suggested offset for the length of the file (NULL if
 *        none)/
 *        FSAL supplied offset for the length of the file (if changed
 *        true)
 * \param changed (output)
 *        True if MDS returns a new file length
 * \param newtime (input)
 *        Client suggested modification time
 *
 * \return Error codes or ERR_FSAL_NO_ERROR
 *
 */

fsal_status_t CEPHFSAL_layoutcommit(fsal_handle_t* filehandle,
				    fsal_layouttype_t type,
				    char* layout,
				    size_t layout_length,
				    fsal_off_t offset,
				    fsal_size_t length,
				    fsal_off_t* newoff,
				    fsal_boolean_t* changed,
				    fsal_time_t* newtime)
{
  /* Commit data */
  if (newtime)
    {
      /* Check that time does not run backward, follow suggestion if
	 we wish */
    }

  if (newoff)
    {
      /* File may only grow, follow suggestion if we wish */
    }

  if (0) /* Some test for a new filesize */
    {
      *changed=1;
    }
  else
    {
      *changed=0;
    }

  /* Obviously, change this when real code is added */

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_layoutcommit);
}

/**
 *
 * FSAL_getdeviceinfo: The NFSv4.1 GETDEVICEINFO operation
 *
 * Look up the address for a given deviceid
 *
 * \param type (input):
 *        The layout type
 * \param deviceid (input):
 *        The device ID to look up
 * \param buff (output):
 *        Address of buffer allocated by the FSAL.
 *
 * \return Error codes or ERR_FSAL_NO_ERROR
 *
 */

fsal_status_t CEPHFSAL_getdeviceinfo(fsal_layouttype_t type,
				     fsal_deviceid_t deviceid,
				     char** buff)
{
  uint64_t inode, generation;
  deviceaddrinfo* entry;

  /* Deconstruct the device ID then look it up in the table */

  memcpy(&inode, deviceid, sizeof(uint64_t));
  memcpy(&generation, deviceid+sizeof(uint64_t), sizeof(uint64_t));

  if ((entry=get_entry(inode, generation))==NULL)
    Return(ERR_FSAL_NOENT, 0, INDEX_FSAL_getdeviceinfo);

  if ((*buff=Mem_Alloc(entry->entry_size)) == NULL)
    Return(ERR_FSAL_NOMEM, 0, INDEX_FSAL_getdeviceinfo);

  memcpy(*buff, entry->addrinfo, entry->entry_size);

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_getdeviceinfo);
}

/**
 *
 * FSAL_getdevicelist: The NFSv4.1 GETDEVICELIST operation
 *
 * Return all deviceids for a given filesystem
 *
 * \param filehandle (input):
 *        Handle of a file on the filesystem in question
 * \param type (input):
 *        The layout type
 * \param numdevices (input/output):
 *        Number of devices requested/returned
 * \param cookie (input/output):
 *        Cookie passed by client/returned by MDS.  Will be zero to
 *        start from the beginning.
 * \param eof (output):
 *        True if all devices have been returned
 * \param buff (output):
 *        Buffer to hold device list (as an array)
 * \param size_t (intput/output):
 *        Amount of memory allocated/used for buff
 *
 * \return Error codes or ERR_FSAL_NO_ERROR
 *
 */

fsal_status_t CEPHFSAL_getdevicelist(fsal_handle_t* filehandle,
				     fsal_layouttype_t type,
				     int* numdevices,
				     uint64_t* cookie,
				     fsal_boolean_t* eof,
				     void* buff,
				     size_t* bufflen)
{
  /* Populate buff with devices */

  *numdevices=0;
  *eof=true;
  
  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_getdevicelist);
}
