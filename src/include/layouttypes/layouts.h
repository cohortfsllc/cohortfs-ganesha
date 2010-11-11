/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010 The Linux Box Corporation
 * All Rights Reserved
 *
 * Contributor: Adam C. Emerson
 */

#ifndef _LAYOUTS_H
#include <nfsv41.h>
#include "layouttypes/fsal_layout.h"

/**
 * \file    layouts.h
 * \brief   Declarations and data types for layout dispatch
 *
 * layouts.h : Declarations and data types for layout dispatch
 *
 *
 */

typedef struct __layoutfuncs
{
  layouttype4 type;
  int (*encode_layout) (layouttype4, layout_content4*, size_t, caddr_t);
  int (*encode_device) (layouttype4, device_addr4*, size_t, caddr_t);
} layoutfunctions;

extern layoutfunctions layoutfuncs[];


layoutfunctions* layouttypelookup(layouttype4 type);

int encode_lo_content(layouttype4 type,
		      layout_content4* dest,
		      size_t size,
		      void*);
		      
int encode_device(layouttype4 type,
		  device_addr4* dest,
		  size_t length,
		  void* source);

#endif /* __LAYOUTS_H */
