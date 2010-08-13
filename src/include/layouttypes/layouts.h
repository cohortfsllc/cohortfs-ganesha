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

#ifndef _LAYOUTS_H
#include <nfsv41.h>

/**
 * \file    layouts.h
 * \brief   Declarations and data types for layout dispatch
 *
 * layouts.h : Declarations and data types for layout dispatch
 *
 *
 */

extern layoutfunctions layoutfuncs[];

typedef struct __layoutfuncs
{
  layouttype4 type;
  int (*encode_layout) (layouttype4, layout_content4,
			size_t, fsal_layout_content*);
  int (*encode_device) (layouttype4, device_addr4,
			size_t, fsal_device_addr_t);
			
} layoutfunctions;

layoutfunctions* layouttypelookup(layouttype4 type);

int encode_lo_content(layouttype4 type,
		      layout_content4* dest,
		      size_t size,
		      fsal_layout_content_t source);
		      
int encode_device(layouttype4 type,
		  device_addr4* dest,
		  size_t destsize,
		  fsal_device_addr_t source);

#endif __LAYOUTS_H
