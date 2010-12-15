/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010 The Linux Box Corporation
 * All Rights Reserved
 * Contributor: Adam C. Emerson
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
#include <alloca.h>
#include "sal.h"
#include "layouttypes/replayouts.h"

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

deviceaddrinfo* savedptr=0;

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

  thentry->next = NULL;

  rc = pthread_mutex_lock(&deviceidtablemutex);
  if (rc != 0)
    return rc;

  key.pdata = (caddr_t) &(thentry->inode);
  key.len = sizeof(uint64_t);

  rc = HashTable_Get(deviceidtable, &key, &value);
  if (rc == HASHTABLE_ERROR_NO_SUCH_KEY)
    {
      /* No entries for this inode, we're the first */

      thentry->generation = 0;
      value.pdata = (caddr_t) thentry;
      value.len = thentry->entry_size + sizeof(deviceaddrinfo);

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

      cur = (deviceaddrinfo*) value.pdata;

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

  key.pdata = (caddr_t) &(thentry->inode);
  key.len = sizeof(uint64_t);

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

  key.pdata = (caddr_t) &inode;
  key.len = sizeof(uint64_t);
  
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

/* Implements Linux Box replication layout */

fsal_status_t layoutget_repl(cephfsal_handle_t* filehandle,
			     fsal_layouttype_t type,
			     fsal_layoutiomode_t iomode,
			     fsal_off_t offset, fsal_size_t length,
			     fsal_size_t minlength,
			     fsal_layout_t** layouts,
			     int *numlayouts,
			     fsal_boolean_t *return_on_close,
			     cephfsal_op_context_t *context,
			     stateid4* stateid,
			     void* opaque)
{
  if ((!global_spec_info.replication_master) ||
      (global_spec_info.replicas == 0))
    {
      Return(ERR_FSAL_LAYOUT_UNAVAILABLE, 0, INDEX_FSAL_layoutget);
    }

  Return(ERR_FSAL_NOTSUPP, 0, INDEX_FSAL_layoutget);
}

/* Implements NFSv4.1 Files layout */

fsal_status_t layoutget_file(cephfsal_handle_t* filehandle,
			     fsal_layouttype_t type,
			     fsal_layoutiomode_t iomode,
			     fsal_off_t offset, fsal_size_t length,
			     fsal_size_t minlength,
			     fsal_layout_t** layouts,
			     int *numlayouts,
			     fsal_boolean_t *return_on_close,
			     cephfsal_op_context_t *context,
			     stateid4* stateid,
			     void* opaque)
{
  struct stat_precise st;
  char name[255];
  int rc;
  uint64_t su;
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
  uint64_t biggest;
  fsal_handle_t ds_handle;
  struct ceph_file_layout file_layout;

  /* Get the file layout information */

  ceph_ll_file_layout(VINODE(filehandle), &file_layout);
  su = file_layout.fl_stripe_unit;
  
  /* Align the layout to ceph stripe boundaries */

  *numlayouts=1;
  *return_on_close = false;
  offset -= offset % su;

  /* Since the Linux kernel supports a maximum of 4096 as the stripe
     count, we will never return a layout longer than 4096*su */

  biggest = 4096 * su;

  if (minlength > biggest)
    Return(ERR_FSAL_INVAL, 0, INDEX_FSAL_layoutget);
  
  if (length > biggest) 
    length = biggest;

  length += (su - (length % su));

  /* Constants needed to populate anything */
  
  num_osds = ceph_ll_num_osds();
  stripes = (length-offset)/su;

  /* Populate the device info */

  reserved_size = (sizeof(deviceaddrinfo) +
		   sizeof(nfsv4_1_file_layout_ds_addr4) +
		   (sizeof(uint32_t) * stripes) +
		   (sizeof(multipath_list4) * num_osds) +
		   (sizeof(netaddr4) * num_osds) +
		   ADDRLENGTH * num_osds);
		      
  entry = (deviceaddrinfo*) Mem_Alloc(reserved_size);
  if (entry == NULL)
    Return(ERR_FSAL_NOMEM, 0, INDEX_FSAL_layoutget);
  
  memset(entry, reserved_size, 0);

  entry->inode = VINODE(filehandle).ino.val;

  reserved_size -= sizeof(deviceaddrinfo);
  
  entry->entry_size = reserved_size;
  deviceaddr
    = entry->addrinfo
    = (fsal_file_dsaddr_t*) (((void*)entry)+sizeof(deviceaddrinfo));
  deviceaddr->nflda_stripe_indices.nflda_stripe_indices_len=stripes;
  stripe_indices
    = deviceaddr->nflda_stripe_indices.nflda_stripe_indices_val
    = (int*) (((void*)deviceaddr) + sizeof(fsal_file_dsaddr_t));
  
  for (i=0; i < stripes; i++)
    {
      int stripe_osd;

      stripe_osd = ceph_ll_get_stripe_osd(VINODE(filehandle), i,
					  &file_layout);

      if (stripe_osd < 0)
	{
	  Mem_Free(entry);
	  Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_layoutget);
	}
      stripe_indices[i] = stripe_osd;
    }

  deviceaddr->nflda_multipath_ds_list.nflda_multipath_ds_list_len
    = num_osds;
  hostlists
    = deviceaddr->nflda_multipath_ds_list.nflda_multipath_ds_list_val
    = (multipath_list4*) (((void*)stripe_indices) + sizeof(uint32_t) * stripes);

  hosts = (netaddr4*) (((void*)hostlists) + num_osds * sizeof(multipath_list4));

  stringwritepos = (char*) (((void*)hosts) + num_osds * sizeof(netaddr4));

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

  savedptr = entry;

  /* Add the layout to the state for the file */
  
  if (state_add_layout_segment(type, iomode, offset, length,
			       *return_on_close, entry, *stateid) != 0)
    {
      remove_entry(entry);
      Mem_Free(entry);
      Return(ERR_FSAL_DELAY, 0, INDEX_FSAL_layoutget);
    }

  
  /* Build the layout to return to the client */

  reserved_size = (sizeof(fsal_layout_t) +
		   sizeof(layout_content4) +
		   sizeof(fsal_dsfh_t) +
		   NFS4_FHSIZE +
		   64);

  if ((*layouts=(fsal_layout_t*) Mem_Alloc(reserved_size)) == NULL)
    Return(ERR_FSAL_NOMEM, 0, INDEX_FSAL_layoutget);

  (*layouts)->lo_offset = offset;
  (*layouts)->lo_length = length;
  (*layouts)->lo_iomode = iomode;
  (*layouts)->lo_content.loc_body.loc_body_val
    = (char*) ((void*)*layouts) + sizeof(fsal_layout_t);

  reserved_size -= (sizeof(fsal_layout_t) +
		    sizeof(layout_content4));

  fileloc = alloca(sizeof(fsal_filelayout_t) +
		   sizeof(fsal_dsfh_t) +
		   NFS4_FHSIZE);

  memcpy(fileloc->deviceid,
	 &(entry->inode),
	 sizeof(uint64_t));
  memcpy(((void*)(fileloc->deviceid))+sizeof(uint64_t),
	 &(entry->generation),
	 sizeof(uint64_t));

  /* We are returning sparse layouts with commit-through-DS */
  
  fileloc->util = su;

  /* The zeroeth stripe represents first block at the given offset. */

  fileloc->first_stripe_index = 0;

  fileloc->pattern_offset = offset;

  /* We return exactly one filehandle */

  fileloc->fhn = 1;

  fileloc->fhs = (fsal_dsfh_t*) (((void*)fileloc) +
				 sizeof(fsal_filelayout_t));

  fileloc->fhs->nfs_fh4_val= (char*)
    (((void*)fileloc->fhs) + sizeof(fsal_dsfh_t));

  /* Give the client a filehandle that may be sent to the DS */

  /* Fill in object access info so that the DS doesn't have to contact
     the MDS (Also tis solves the lack of a lookup-by-inode in
     current Ceph.) */

  ds_handle = *filehandle;
  ds_handle.data.layout = file_layout;
  ds_handle.data.snapseq = ceph_ll_snap_seq(VINODE(filehandle));
  
  FSALBACK_fh2dshandle(&ds_handle, fileloc->fhs, opaque);

  if (!(encode_lo_content(LAYOUT4_NFSV4_1_FILES,
			  &((*layouts)->lo_content),
			  reserved_size, fileloc)))
    {
      Mem_Free(*layouts);
      remove_entry(entry);
      Mem_Free(entry);
      Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_layoutget);
    }
			

  /* On success, bump the seqid */

  state_layout_inc_state(stateid);

  Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_layoutget);
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
 * \param opaque (input):
 *        Passed to FSALBACK function to create filehandle
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
				 stateid4* stateid,
				 void* opaque)
{
  switch (type)
    {
    case LAYOUT4_NFSV4_1_FILES:
      return layoutget_file(filehandle, type, iomode, offset, length,
			    minlength, layouts,
			    numlayouts,return_on_close, context,
			    stateid, opaque);
      break;
    case LBX_REPLICATION:
      return layoutget_repl(filehandle, type, iomode, offset, length,
			    minlength, layouts,
			    numlayouts,return_on_close, context,
			    stateid, opaque);
      break;
    default:
      Return(ERR_FSAL_UNKNOWN_LAYOUTTYPE, 0, INDEX_FSAL_layoutget);
      break;
    }
}

/**
 *
 * FSAL_layoutreturn: The NFSv4.1 LAYOUTRETURN operation
 *
 * Free a client specified layout range on a file.
 *
 * \param filehandle (input):
 *        The handle upon which the layout was granted
 * \param type (input):
 *        The layout type
 * \param iomode (input):
 *        The iomode passed by the client (in case it's ANY)
 * \param offset (input):
 *        The offset (specified by the client)
 * \param length (input):
 *        The length (specified by the client)
 * \param context (input):
 *        The authentication context
 * \param nomore (output):
 *        Set to true if the last layout segment has been freed.
 * \param stateid (input/output):
 *        The layout stateid.
 *
 * \return Error codes or ERR_FSAL_NO_ERROR
 *
 */

fsal_status_t CEPHFSAL_layoutreturn(cephfsal_handle_t* filehandle,
				    fsal_layouttype_t type,
				    fsal_layoutiomode_t iomode,
				    fsal_off_t offset,
				    fsal_size_t length,
				    cephfsal_op_context_t* context,
				    bool_t* nomore,
				    stateid4* stateid)
{
  uint64_t layoutcookie = 0;
  bool_t finished = false;
  layoutsegment segment;
  int remaining = 0;
  int rc = 0;
  *nomore = false;

  /* We iterate over all segments returning those falling completely
     within the client's range */
  
  do 
    {
      rc = state_iter_layout_entries(*stateid, &layoutcookie,
				     &finished, &segment);
      
      if (rc != ERR_STATE_NO_ERROR)
	Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_layoutget);

      ++remaining;

      if ((segment.type != type) || /* This should never happen */
	  !(segment.iomode & iomode) || /* iomode should match ro be
					   ANY */
	  (segment.offset < offset) ||
	  ((segment.offset + segment.length) >
	   (offset + length)))
	continue;

      if (remove_entry(segment.layoutdata) != 0)
	Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_layoutreturn);

      if (state_free_layout_segment(*stateid, segment.segid) !=
	  ERR_STATE_NO_ERROR)
	Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_layoutreturn);
      --remaining;
    } while (!finished);
  
  if (!remaining)
    *nomore = true;

  state_layout_inc_state(stateid);
  
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
 * \param offset (input):
 *        Offset into the file of the changed portion
 * \param length (input):
 *        Length of the changed portion
 * \param last_offset (input/output):
 *        Client suggested offset for the length of the file (NULL if
 *        none)/FSAL supplied offset for the length of the file
 * \param time (input/output)
 *        Client suggested modification time/actually adopted.
 * \param stateid (input)
 *        Stateid of the given layout
 * \param layoutupdate (input)
 *        Type specific update data
 *
 * \return Error codes or ERR_FSAL_NO_ERROR
 *
 */

fsal_status_t CEPHFSAL_layoutcommit(cephfsal_handle_t* filehandle,
				    fsal_off_t offset,
				    fsal_size_t length,
				    fsal_off_t* newoff,
				    fsal_time_t* newtime,
				    stateid4 stateid,
				    layoutupdate4 layoutupdate,
				    cephfsal_op_context_t* pcontext)
{
  int uid = FSAL_OP_CONTEXT_TO_UID(pcontext);
  int gid = FSAL_OP_CONTEXT_TO_GID(pcontext);
  struct stat_precise stold, stnew;
  int rc = 0;
  int attrmask = 0;

  /* For file layouts, we just update the metadata */
  
  if (!filehandle)
      Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_layoutcommit);

  if (!(newoff || newtime))
    Return(ERR_FSAL_NO_ERROR, 0, INDEX_FSAL_layoutcommit);

  memset(&stnew, 0, sizeof(struct stat_precise));
    
  if ((rc = ceph_ll_getattr_precise(VINODE(filehandle), &stold, uid,
				    gid)) < 0)
    Return(posix2fsal_error(rc), 0, INDEX_FSAL_open);

  if (newoff)
    {
      if (stold.st_size > *newoff + 1)
	*newoff = stold.st_size - 1;
      else
	{
	  attrmask |= CEPH_SETATTR_SIZE;
	  stnew.st_size = *newoff + 1;
	}
    }

  if (newtime)
    {
      if (((newtime->seconds == stold.st_mtime_sec) &&
	   (newtime->nseconds <= stold.st_mtime_micro * 1000)) ||
	  (newtime->seconds < stold.st_mtime_sec))
	{
	  newtime->seconds = stold.st_mtime_sec;
	  newtime->nseconds = stold.st_mtime_micro * 1000;
	}
      else
	{
	  attrmask |= CEPH_SETATTR_MTIME;
	  stnew.st_mtime_sec = newtime->seconds;
	  stnew.st_mtime_micro = newtime->nseconds / 1000;
	}
    }

  if (attrmask)
    if ((rc = ceph_ll_setattr_precise(VINODE(filehandle), &stnew,
				      attrmask, uid, gid)) < 0)
      Return(posix2fsal_error(rc), 0, INDEX_FSAL_open);
	
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
 * \param devaddr (input/output):
 *        Address of device_addr4, body allocated by FSAL
 *
 * \return Error codes or ERR_FSAL_NO_ERROR
 *
 */

fsal_status_t CEPHFSAL_getdeviceinfo(fsal_layouttype_t type,
				     fsal_deviceid_t deviceid,
				     device_addr4* devaddr)
{
  uint64_t inode, generation;
  deviceaddrinfo* entry;
  char* xdrbuff;

  /* Deconstruct the device ID then look it up in the table */

  memcpy(&inode, deviceid, sizeof(uint64_t));
  memcpy(&generation, deviceid+sizeof(uint64_t), sizeof(uint64_t));

  if ((entry=get_entry(inode, generation))==NULL)
    Return(ERR_FSAL_NOENT, 0, INDEX_FSAL_getdeviceinfo);

  if ((xdrbuff=Mem_Alloc(entry->entry_size+64)) == NULL)
    Return(ERR_FSAL_NOMEM, 0, INDEX_FSAL_getdeviceinfo);

  devaddr->da_addr_body.da_addr_body_val=xdrbuff;
  
  if (!(encodefilesdevice(type, devaddr, entry->entry_size+64,
			  entry->addrinfo)))
    {
      Mem_Free(xdrbuff);
      Return(ERR_FSAL_FAULT, 0, INDEX_FSAL_getdeviceinfo);
    }

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
