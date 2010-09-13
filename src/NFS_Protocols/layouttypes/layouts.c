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
 * \file    layouts.c
 * \brief   Routines for dispatching on layout type
 *
 * layouts.c : Routines for dispatching on layout type
 *
 *
 */

#include "layouttypes/layouts.h"
#include "layouttypes/fsal_layout.h"

/**
 *
 * layoutfuncs: Array of layoutfunctions structures, providing
 *              type->function mapping.  Must be terminated with all
 *              zeroes and nulls lest the loop fall off the end.
 *
 */

int encodefileslayout(layouttype4 type,
		      layout_content4* dest,
		      size_t size,
		      void* source);

int encodefilesdevice(layouttype4 type,
		      device_addr4* dest,
		      size_t destsize,
		      void* source);

layoutfunctions layoutfuncs[]={
  {LAYOUT4_NFSV4_1_FILES,
   encodefileslayout,
   encodefilesdevice},
  {0, NULL, NULL}};

/**
 *
 * layouttypelookup: Look up the functions for a given type
 *
 * @param type [IN] the type to look up
 *
 * @return NULL on failure, else a pointer to the functions structure
 *
 */

layoutfunctions* layouttypelookup(layouttype4 type)
{
  layoutfunctions* index;
  layoutfunctions* result=NULL;

  for (index=layoutfuncs; index->type != 0; index++)
    {
      if (index->type == type)
	{
	  result=index;
	  break;
	}
    }
  return result;
}

/**
 *
 * encode_lo_content: Encodes FSAL layout content as xdr
 *
 * @param type   [IN]  The layout type
 * @param dest   [OUT] The lo_content to fill
 * @param size   [IN]  The space available
 * @param source [IN]  The layout provided by the FSAL
 *
 * @return 0 on success, otherwise not.
 *
 * @see layouttypelookup
 * @see nfs41_op_layoutget
 *
 */

int encode_lo_content(layouttype4 type,
		      layout_content4* dest,
		      size_t size,
		      void* source)
{
  return
    ((layouttypelookup(type)->encode_layout)(type, dest, size, source));
}

/**
 *
 * encode_device: Encodes FSAL layout content as xdr
 *
 * @param type     [IN]  The layout type
 * @param dest     [OUT] Pointer to the device_addr4 to fill
 * @param destsize [IN]  Size allocate for destination
 * @param source   [IN]  The device address returned by the FSAL
 * @param srcsize  [IN]  Total size of the source data
 *
 * @return TRUE on success, otherwise not.
 *
 * @see layouttypelookup
 * @see nfs41_op_layoutget
 *
 */

int encode_device(layouttype4 type,
		  device_addr4* dest,
		  size_t destsize,
		  fsal_devaddr_t* source)
{
  return ((layouttypelookup(type)->encode_device)(type, dest, destsize, source));
}
