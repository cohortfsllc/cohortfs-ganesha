/*
 * Copyright (C) 2010 The Linx Box Corporation
 * Contributor : Adam C. Emerson
 *
 * Some Portions Copyright CEA/DAM/DIF  (2008)
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
 *
 * \file    fsal_internal.h
 * \brief   Extern definitions for variables that are
 *          defined in fsal_internal.c.
 *
 */

#include  "fsal.h"
#include  <ceph/libceph.h>
#include  <string.h>
#ifdef _USE_FSAL_MDS
#include "layouttypes/fsal_layout.h"
#endif /* _USE_FSAL_MDS

/* the following variables must not be defined in fsal_internal.c */
#ifndef FSAL_INTERNAL_C

#define ReturnStatus( _st_, _f_ )	Return( (_st_).major, (_st_).minor, _f_ )

/* static filesystem info.
 * read access only.
 */
extern fsal_staticfsinfo_t global_fs_info;

/* Everybody gets to know the server. */
extern fs_specific_initinfo_t global_spec_info;

#endif

int one_shot_compound(unsigned long server, COMPOUND4args args,
		      COMPOUND4res* res);


unsigned long dotted_quad_to_nbo(char* dq);


/**
 *  This function initializes shared variables of the FSAL.
 */
fsal_status_t fsal_internal_init_global(fsal_init_info_t * fsal_info,
                                        fs_common_initinfo_t * fs_common_info);

/**
 *  Increments the number of calls for a function.
 */
void fsal_increment_nbcall(int function_index, fsal_status_t status);

/**
 * Retrieves current thread statistics.
 */
void fsal_internal_getstats(fsal_statistics_t * output_stats);

/**
 *  Used to limit the number of simultaneous calls to Filesystem.
 */
void TakeTokenFSCall();
void ReleaseTokenFSCall();

/**
 * fsal_do_log:
 * Indicates if an FSAL error has to be traced
 * into its log file in the NIV_EVENT level.
 * (in the other cases, return codes are only logged
 * in the NIV_FULL_DEBUG logging lovel).
 */
fsal_boolean_t fsal_do_log(fsal_status_t status);


/**
 *  ReturnCode :
 *  Macro for returning a fsal_status_t without trace nor stats increment.
 */
#define ReturnCode( _code_, _minor_ ) do {                               \
               fsal_status_t _struct_status_ = FSAL_STATUS_NO_ERROR ;\
               (_struct_status_).major = (_code_) ;          \
               (_struct_status_).minor = (_minor_) ;         \
               return (_struct_status_);                     \
              } while(0)


/* All the call to FSAL to be wrapped */
fsal_status_t CEPHFSAL_access(cephfsal_handle_t * p_object_handle,        /* IN */
                             cephfsal_op_context_t * p_context,  /* IN */
                             fsal_accessflags_t access_type,    /* IN */
                             fsal_attrib_list_t * p_object_attributes /* [ IN/OUT ] */ );

fsal_status_t CEPHFSAL_getattrs(cephfsal_handle_t * p_filehandle, /* IN */
                               cephfsal_op_context_t * p_context,        /* IN */
                               fsal_attrib_list_t * p_object_attributes /* IN/OUT */ );

fsal_status_t CEPHFSAL_setattrs(cephfsal_handle_t * p_filehandle, /* IN */
                               cephfsal_op_context_t * p_context,        /* IN */
                               fsal_attrib_list_t * p_attrib_set,       /* IN */
                               fsal_attrib_list_t *
                               p_object_attributes /* [ IN/OUT ] */ );

fsal_status_t CEPHFSAL_BuildExportContext(cephfsal_export_context_t * p_export_context,   /* OUT */
                                         fsal_path_t * p_export_path,   /* IN */
                                         char *fs_specific_options /* IN */ );

fsal_status_t CEPHFSAL_InitClientContext(cephfsal_op_context_t * p_thr_context);

fsal_status_t CEPHFSAL_CleanUpExportContext(cephfsal_export_context_t * p_export_context) ;

fsal_status_t CEPHFSAL_GetClientContext(cephfsal_op_context_t * p_thr_context,    /* IN/OUT  */
                                       cephfsal_export_context_t * p_export_context,     /* IN */
                                       fsal_uid_t uid,  /* IN */
                                       fsal_gid_t gid,  /* IN */
                                       fsal_gid_t * alt_groups, /* IN */
                                       fsal_count_t nb_alt_groups /* IN */ );

fsal_status_t CEPHFSAL_create(cephfsal_handle_t * p_parent_directory_handle,      /* IN */
                             fsal_name_t * p_filename,  /* IN */
                             cephfsal_op_context_t * p_context,  /* IN */
                             fsal_accessmode_t accessmode,      /* IN */
                             cephfsal_handle_t * p_object_handle,        /* OUT */
                             fsal_attrib_list_t * p_object_attributes /* [ IN/OUT ] */ );
#ifdef _USE_CBREP
fsal_status_t CEPHFSAL_create_withfh(cephfsal_handle_t * p_parent_directory_handle,      /* IN */
				     cephfsal_handle_t * supplied_file_handle,
				     fsal_name_t * p_filename,  /* IN */
				     cephfsal_op_context_t * p_context,  /* IN */
				     fsal_accessmode_t accessmode,      /* IN */
				     cephfsal_handle_t * p_object_handle,        /* OUT */
				     fsal_attrib_list_t * p_object_attributes /* [ IN/OUT ] */ );
#endif

fsal_status_t CEPHFSAL_mkdir(cephfsal_handle_t * p_parent_directory_handle,       /* IN */
                            fsal_name_t * p_dirname,    /* IN */
                            cephfsal_op_context_t * p_context,   /* IN */
                            fsal_accessmode_t accessmode,       /* IN */
                            cephfsal_handle_t * p_object_handle, /* OUT */
                            fsal_attrib_list_t * p_object_attributes /* [ IN/OUT ] */ );

#ifdef _USE_CBREP
fsal_status_t CEPHFSAL_mkdir_withfh(cephfsal_handle_t * p_parent_directory_handle,       /* IN */
				    cephfsal_handle_t * supplied_file_handle,
				    fsal_name_t * p_dirname,    /* IN */
				    cephfsal_op_context_t * p_context,   /* IN */
				    fsal_accessmode_t accessmode,       /* IN */
				    cephfsal_handle_t * p_object_handle, /* OUT */
				    fsal_attrib_list_t * p_object_attributes /* [ IN/OUT ] */ );
#endif

fsal_status_t CEPHFSAL_link(cephfsal_handle_t * p_target_handle,  /* IN */
                           cephfsal_handle_t * p_dir_handle,     /* IN */
                           fsal_name_t * p_link_name,   /* IN */
                           cephfsal_op_context_t * p_context,    /* IN */
                           fsal_attrib_list_t * p_attributes /* [ IN/OUT ] */ );

fsal_status_t CEPHFSAL_mknode(cephfsal_handle_t * parentdir_handle,       /* IN */
                             fsal_name_t * p_node_name, /* IN */
                             cephfsal_op_context_t * p_context,  /* IN */
                             fsal_accessmode_t accessmode,      /* IN */
                             fsal_nodetype_t nodetype,  /* IN */
                             fsal_dev_t * dev,  /* IN */
                             cephfsal_handle_t * p_object_handle,        /* OUT (handle to the created node) */
                             fsal_attrib_list_t * node_attributes /* [ IN/OUT ] */ );

fsal_status_t CEPHFSAL_opendir(cephfsal_handle_t * p_dir_handle,  /* IN */
                              cephfsal_op_context_t * p_context, /* IN */
                              cephfsal_dir_t * p_dir_descriptor, /* OUT */
                              fsal_attrib_list_t * p_dir_attributes /* [ IN/OUT ] */ );

fsal_status_t CEPHFSAL_readdir(cephfsal_dir_t * p_dir_descriptor, /* IN */
                              cephfsal_cookie_t start_position,  /* IN */
                              fsal_attrib_mask_t get_attr_mask, /* IN */
                              fsal_mdsize_t buffersize, /* IN */
                              fsal_dirent_t * p_pdirent,        /* OUT */
                              cephfsal_cookie_t * p_end_position,        /* OUT */
                              fsal_count_t * p_nb_entries,      /* OUT */
                              fsal_boolean_t * p_end_of_dir /* OUT */ );

fsal_status_t CEPHFSAL_closedir(cephfsal_dir_t * p_dir_descriptor /* IN */ );

fsal_status_t CEPHFSAL_open_by_name(cephfsal_handle_t * dirhandle,        /* IN */
                                   fsal_name_t * filename,      /* IN */
                                   cephfsal_op_context_t * p_context,    /* IN */
                                   fsal_openflags_t openflags,  /* IN */
                                   cephfsal_file_t * file_descriptor,    /* OUT */
                                   fsal_attrib_list_t *
                                   file_attributes /* [ IN/OUT ] */ );

fsal_status_t CEPHFSAL_open(cephfsal_handle_t * p_filehandle,     /* IN */
                           cephfsal_op_context_t * p_context,    /* IN */
                           fsal_openflags_t openflags,  /* IN */
                           cephfsal_file_t * p_file_descriptor,  /* OUT */
                           fsal_attrib_list_t * p_file_attributes /* [ IN/OUT ] */ );

fsal_status_t CEPHFSAL_read(cephfsal_file_t * p_file_descriptor,  /* IN */
                           fsal_seek_t * p_seek_descriptor,     /* [IN] */
                           fsal_size_t buffer_size,     /* IN */
                           caddr_t buffer,      /* OUT */
                           fsal_size_t * p_read_amount, /* OUT */
                           fsal_boolean_t * p_end_of_file /* OUT */ );

fsal_status_t CEPHFSAL_write(cephfsal_file_t * p_file_descriptor, /* IN */
                            fsal_seek_t * p_seek_descriptor,    /* IN */
                            fsal_size_t buffer_size,    /* IN */
                            caddr_t buffer,     /* IN */
                            fsal_size_t * p_write_amount /* OUT */ );

fsal_status_t CEPHFSAL_close(cephfsal_file_t * p_file_descriptor /* IN */ );

fsal_status_t CEPHFSAL_open_by_fileid(cephfsal_handle_t * filehandle,     /* IN */
                                     fsal_u64_t fileid, /* IN */
                                     cephfsal_op_context_t * p_context,  /* IN */
                                     fsal_openflags_t openflags,        /* IN */
                                     cephfsal_file_t * file_descriptor,  /* OUT */
                                     fsal_attrib_list_t *
                                     file_attributes /* [ IN/OUT ] */ );

fsal_status_t CEPHFSAL_close_by_fileid(cephfsal_file_t * file_descriptor /* IN */ ,
                                      fsal_u64_t fileid);

fsal_status_t CEPHFSAL_static_fsinfo(cephfsal_handle_t * p_filehandle,    /* IN */
                                    cephfsal_op_context_t * p_context,   /* IN */
                                    fsal_staticfsinfo_t * p_staticinfo /* OUT */ );

fsal_status_t CEPHFSAL_dynamic_fsinfo(cephfsal_handle_t * p_filehandle,   /* IN */
                                     cephfsal_op_context_t * p_context,  /* IN */
                                     fsal_dynamicfsinfo_t * p_dynamicinfo /* OUT */ );

fsal_status_t CEPHFSAL_Init(fsal_parameter_t * init_info /* IN */ );

fsal_status_t CEPHFSAL_terminate();

fsal_status_t CEPHFSAL_test_access(cephfsal_op_context_t * p_context,     /* IN */
                                  fsal_accessflags_t access_type,       /* IN */
                                  fsal_attrib_list_t * p_object_attributes /* IN */ );

fsal_status_t CEPHFSAL_setattr_access(cephfsal_op_context_t * p_context,  /* IN */
                                     fsal_attrib_list_t * candidate_attributes, /* IN */
                                     fsal_attrib_list_t * object_attributes /* IN */ );

fsal_status_t CEPHFSAL_rename_access(cephfsal_op_context_t * pcontext,    /* IN */
                                    fsal_attrib_list_t * pattrsrc,      /* IN */
                                    fsal_attrib_list_t * pattrdest) /* IN */ ;

fsal_status_t CEPHFSAL_create_access(cephfsal_op_context_t * pcontext,    /* IN */
                                    fsal_attrib_list_t * pattr) /* IN */ ;

fsal_status_t CEPHFSAL_unlink_access(cephfsal_op_context_t * pcontext,    /* IN */
                                    fsal_attrib_list_t * pattr) /* IN */ ;

fsal_status_t CEPHFSAL_link_access(cephfsal_op_context_t * pcontext,      /* IN */
                                  fsal_attrib_list_t * pattr) /* IN */ ;

fsal_status_t CEPHFSAL_merge_attrs(fsal_attrib_list_t * pinit_attr,
                                  fsal_attrib_list_t * pnew_attr,
                                  fsal_attrib_list_t * presult_attr);

fsal_status_t CEPHFSAL_lookup(cephfsal_handle_t * p_parent_directory_handle,      /* IN */
                             fsal_name_t * p_filename,  /* IN */
                             cephfsal_op_context_t * p_context,  /* IN */
                             cephfsal_handle_t * p_object_handle,        /* OUT */
                             fsal_attrib_list_t * p_object_attributes /* [ IN/OUT ] */ );

fsal_status_t CEPHFSAL_lookupPath(fsal_path_t * p_path,  /* IN */
                                 cephfsal_op_context_t * p_context,      /* IN */
                                 cephfsal_handle_t * object_handle,      /* OUT */
                                 fsal_attrib_list_t *
                                 p_object_attributes /* [ IN/OUT ] */ );

fsal_status_t CEPHFSAL_lookupJunction(cephfsal_handle_t * p_junction_handle,      /* IN */
                                     cephfsal_op_context_t * p_context,  /* IN */
                                     cephfsal_handle_t * p_fsoot_handle, /* OUT */
                                     fsal_attrib_list_t *
                                     p_fsroot_attributes /* [ IN/OUT ] */ );

fsal_status_t CEPHFSAL_lock(cephfsal_file_t * obj_handle,
                           cephfsal_lockdesc_t * ldesc, fsal_boolean_t blocking);

fsal_status_t CEPHFSAL_changelock(cephfsal_lockdesc_t * lock_descriptor,  /* IN / OUT */
                                 fsal_lockparam_t * lock_info /* IN */ );

fsal_status_t CEPHFSAL_unlock(cephfsal_file_t * obj_handle, cephfsal_lockdesc_t * ldesc);

fsal_status_t CEPHFSAL_getlock(cephfsal_file_t * obj_handle, cephfsal_lockdesc_t * ldesc);

fsal_status_t CEPHFSAL_CleanObjectResources(cephfsal_handle_t * in_fsal_handle);

fsal_status_t CEPHFSAL_set_quota(fsal_path_t * pfsal_path,       /* IN */
                                int quota_type, /* IN */
                                fsal_uid_t fsal_uid,    /* IN */
                                fsal_quota_t * pquota,  /* IN */
                                fsal_quota_t * presquota);      /* OUT */

fsal_status_t CEPHFSAL_get_quota(fsal_path_t * pfsal_path,       /* IN */
                                int quota_type, /* IN */
                                fsal_uid_t fsal_uid,    /* IN */
                                fsal_quota_t * pquota); /* OUT */

fsal_status_t CEPHFSAL_rcp(cephfsal_handle_t * filehandle,        /* IN */
                          cephfsal_op_context_t * p_context,     /* IN */
                          fsal_path_t * p_local_path,   /* IN */
                          fsal_rcpflag_t transfer_opt /* IN */ );

fsal_status_t CEPHFSAL_rcp_by_fileid(cephfsal_handle_t * filehandle,      /* IN */
                                    fsal_u64_t fileid,  /* IN */
                                    cephfsal_op_context_t * p_context,   /* IN */
                                    fsal_path_t * p_local_path, /* IN */
                                    fsal_rcpflag_t transfer_opt /* IN */ );

fsal_status_t CEPHFSAL_rename(cephfsal_handle_t * p_old_parentdir_handle, /* IN */
                             fsal_name_t * p_old_name,  /* IN */
                             cephfsal_handle_t * p_new_parentdir_handle, /* IN */
                             fsal_name_t * p_new_name,  /* IN */
                             cephfsal_op_context_t * p_context,  /* IN */
                             fsal_attrib_list_t * p_src_dir_attributes, /* [ IN/OUT ] */
                             fsal_attrib_list_t * p_tgt_dir_attributes /* [ IN/OUT ] */ );

void CEPHFSAL_get_stats(fsal_statistics_t * stats,       /* OUT */
                       fsal_boolean_t reset /* IN */ );

fsal_status_t CEPHFSAL_readlink(cephfsal_handle_t * p_linkhandle, /* IN */
                               cephfsal_op_context_t * p_context,        /* IN */
                               fsal_path_t * p_link_content,    /* OUT */
                               fsal_attrib_list_t * p_link_attributes /* [ IN/OUT ] */ );

fsal_status_t CEPHFSAL_symlink(cephfsal_handle_t * p_parent_directory_handle,     /* IN */
                              fsal_name_t * p_linkname, /* IN */
                              fsal_path_t * p_linkcontent,      /* IN */
                              cephfsal_op_context_t * p_context, /* IN */
                              fsal_accessmode_t accessmode,     /* IN (ignored) */
                              cephfsal_handle_t * p_link_handle, /* OUT */
                              fsal_attrib_list_t * p_link_attributes /* [ IN/OUT ] */ );

#ifdef _USE_CBREP
fsal_status_t CEPHFSAL_symlink_withfh(cephfsal_handle_t * p_parent_directory_handle,     /* IN */
				      cephfsal_handle_t * supplied_file_handle,
				      fsal_name_t * p_linkname, /* IN */
				      fsal_path_t * p_linkcontent,      /* IN */
				      cephfsal_op_context_t * p_context, /* IN */
				      fsal_accessmode_t accessmode,     /* IN (ignored) */
				      cephfsal_handle_t * p_link_handle, /* OUT */
				      fsal_attrib_list_t * p_link_attributes /* [ IN/OUT ] */ );
#endif

int CEPHFSAL_handlecmp(cephfsal_handle_t * handle1, cephfsal_handle_t * handle2,
                      fsal_status_t * status);

unsigned int CEPHFSAL_Handle_to_HashIndex(cephfsal_handle_t * p_handle,
                                         unsigned int cookie,
                                         unsigned int alphabet_len,
                                         unsigned int index_size);

unsigned int CEPHFSAL_Handle_to_RBTIndex(cephfsal_handle_t * p_handle, unsigned int cookie);

fsal_status_t CEPHFSAL_DigestHandle(cephfsal_export_context_t * p_expcontext,     /* IN */
                                   fsal_digesttype_t output_type,       /* IN */
                                   cephfsal_handle_t * p_in_fsal_handle, /* IN */
                                   caddr_t out_buff /* OUT */ );

fsal_status_t CEPHFSAL_ExpandHandle(cephfsal_export_context_t * p_expcontext,     /* IN */
                                   fsal_digesttype_t in_type,   /* IN */
                                   caddr_t in_buff,     /* IN */
                                   cephfsal_handle_t * p_out_fsal_handle /* OUT */ );

fsal_status_t CEPHFSAL_SetDefault_FSAL_parameter(fsal_parameter_t * out_parameter);

fsal_status_t CEPHFSAL_SetDefault_FS_common_parameter(fsal_parameter_t * out_parameter);

fsal_status_t CEPHFSAL_SetDefault_FS_specific_parameter(fsal_parameter_t * out_parameter);

fsal_status_t CEPHFSAL_load_FSAL_parameter_from_conf(config_file_t in_config,
                                                    fsal_parameter_t * out_parameter);

fsal_status_t CEPHFSAL_load_FS_common_parameter_from_conf(config_file_t in_config,
                                                         fsal_parameter_t *
                                                         out_parameter);

fsal_status_t CEPHFSAL_load_FS_specific_parameter_from_conf(config_file_t in_config,
                                                           fsal_parameter_t *
                                                           out_parameter);

fsal_status_t CEPHFSAL_truncate(cephfsal_handle_t * p_filehandle, /* IN */
                               cephfsal_op_context_t * p_context,        /* IN */
                               fsal_size_t length,      /* IN */
                               cephfsal_file_t * file_descriptor,        /* Unused in this FSAL */
                               fsal_attrib_list_t *
                               p_object_attributes /* [ IN/OUT ] */ );

fsal_status_t CEPHFSAL_unlink(cephfsal_handle_t * p_parent_directory_handle,      /* IN */
                             fsal_name_t * p_object_name,       /* IN */
                             cephfsal_op_context_t * p_context,  /* IN */
                             fsal_attrib_list_t *
                             p_parent_directory_attributes /* [IN/OUT ] */ );

char *CEPHFSAL_GetFSName();

fsal_status_t CEPHFSAL_GetXAttrAttrs(cephfsal_handle_t * p_objecthandle,  /* IN */
                                    cephfsal_op_context_t * p_context,   /* IN */
                                    unsigned int xattr_id,      /* IN */
                                    fsal_attrib_list_t * p_attrs);

fsal_status_t CEPHFSAL_ListXAttrs(cephfsal_handle_t * p_objecthandle,     /* IN */
                                 unsigned int cookie,   /* IN */
                                 cephfsal_op_context_t * p_context,      /* IN */
                                 fsal_xattrent_t * xattrs_tab,  /* IN/OUT */
                                 unsigned int xattrs_tabsize,   /* IN */
                                 unsigned int *p_nb_returned,   /* OUT */
                                 int *end_of_list /* OUT */ );

fsal_status_t CEPHFSAL_GetXAttrValueById(cephfsal_handle_t * p_objecthandle,      /* IN */
                                        unsigned int xattr_id,  /* IN */
                                        cephfsal_op_context_t * p_context,       /* IN */
                                        caddr_t buffer_addr,    /* IN/OUT */
                                        size_t buffer_size,     /* IN */
                                        size_t * p_output_size /* OUT */ );

fsal_status_t CEPHFSAL_GetXAttrIdByName(cephfsal_handle_t * p_objecthandle,       /* IN */
                                       const fsal_name_t * xattr_name,  /* IN */
                                       cephfsal_op_context_t * p_context,        /* IN */
                                       unsigned int *pxattr_id /* OUT */ );

fsal_status_t CEPHFSAL_GetXAttrValueByName(cephfsal_handle_t * p_objecthandle,    /* IN */
                                          const fsal_name_t * xattr_name,       /* IN */
                                          cephfsal_op_context_t * p_context,     /* IN */
                                          caddr_t buffer_addr,  /* IN/OUT */
                                          size_t buffer_size,   /* IN */
                                          size_t * p_output_size /* OUT */ );

fsal_status_t CEPHFSAL_SetXAttrValue(cephfsal_handle_t * p_objecthandle,  /* IN */
                                    const fsal_name_t * xattr_name,     /* IN */
                                    cephfsal_op_context_t * p_context,   /* IN */
                                    caddr_t buffer_addr,        /* IN */
                                    size_t buffer_size, /* IN */
                                    int create /* IN */ );

fsal_status_t CEPHFSAL_SetXAttrValueById(cephfsal_handle_t * p_objecthandle,      /* IN */
                                        unsigned int xattr_id,  /* IN */
                                        cephfsal_op_context_t * p_context,       /* IN */
                                        caddr_t buffer_addr,    /* IN */
                                        size_t buffer_size /* IN */ );

fsal_status_t CEPHFSAL_RemoveXAttrById(cephfsal_handle_t * p_objecthandle,        /* IN */
                                      cephfsal_op_context_t * p_context, /* IN */
                                      unsigned int xattr_id) /* IN */ ;

fsal_status_t CEPHFSAL_RemoveXAttrByName(cephfsal_handle_t * p_objecthandle,      /* IN */
                                        cephfsal_op_context_t * p_context,       /* IN */
                                        const fsal_name_t * xattr_name) /* IN */ ;

unsigned int CEPHFSAL_GetFileno(fsal_file_t * pfile);

fsal_status_t CEPHFSAL_getextattrs(cephfsal_handle_t * p_filehandle, /* IN */
				   cephfsal_op_context_t * p_context,        /* IN */
				   fsal_extattrib_list_t * p_object_attributes /* OUT */) ;

#ifdef _USE_FSALMDS
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
				 stateid4* ostateid,
				 void* opaque);

fsal_status_t CEPHFSAL_layoutreturn(cephfsal_handle_t* filehandle,
				    fsal_layouttype_t type,
				    fsal_layoutiomode_t iomode,
				    fsal_off_t offset,
				    fsal_size_t length,
				    cephfsal_op_context_t* context,
				    bool_t* nomore,
				    stateid4* stateid);

fsal_status_t CEPHFSAL_layoutcommit(cephfsal_handle_t* filehandle,
				    fsal_off_t offset,
				    fsal_size_t length,
				    fsal_off_t* newoff,
				    fsal_time_t* newtime,
				    stateid4 stateid,
				    layoutupdate4 layoutupdate,
				    cephfsal_op_context_t* pcontext);

fsal_status_t CEPHFSAL_getdeviceinfo(fsal_layouttype_t type,
				     fsal_deviceid_t id,
				     device_addr4* devaddr);

fsal_status_t CEPHFSAL_getdevicelist(fsal_handle_t* filehandle,
				     fsal_layouttype_t type,
				     int *numdevices,
				     uint64_t *cookie,
				     fsal_boolean_t* eof,
				     void* buff,
				     size_t* len);
#endif /* _USE_FSALMDS */

#ifdef _USE_FSALDS

fsal_status_t CEPHFSAL_ds_read(cephfsal_handle_t * filehandle,     /*  IN  */
			       fsal_seek_t * seek_descriptor,  /* [IN] */
			       fsal_size_t buffer_size,        /*  IN  */
			       caddr_t buffer,                 /* OUT  */
			       fsal_size_t * read_amount,      /* OUT  */
			       fsal_boolean_t * end_of_file    /* OUT  */
    );

fsal_status_t CEPHFSAL_ds_write(cephfsal_handle_t * filehandle,      /* IN */
				fsal_seek_t * seek_descriptor,   /* IN */
				fsal_size_t buffer_size,         /* IN */
				caddr_t buffer,                  /* IN */
				fsal_size_t * write_amount,      /* OUT */
				fsal_boolean_t stable_flag       /* IN */
    );

fsal_status_t CEPHFSAL_ds_commit(cephfsal_handle_t * filehandle,     /* IN */
				 fsal_off_t offset,
				 fsal_size_t length);
fsal_status_t CEPHFSAL_crc32(fsal_handle_t* filehandle,
			     uint32_t* crc,
			     cephfsal_op_context_t* context);

#endif /* _USE_FSALDS */
