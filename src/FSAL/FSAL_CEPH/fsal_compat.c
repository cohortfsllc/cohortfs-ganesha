/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2010, The Linux Box Corporation
 * Contributor : Adam C. Emerson <aemerson@linuxbox.com>
 *
 * Some portions Copyright CEA/DAM/DIF  (2008)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fsal.h"
#include "fsal_types.h"
#include "fsal_glue.h"
#include "fsal_internal.h"

fsal_status_t WRAP_CEPHFSAL_access(fsal_handle_t * object_handle,        /* IN */
				   fsal_op_context_t * p_context,        /* IN */
				   fsal_accessflags_t access_type,       /* IN */
				   fsal_attrib_list_t *
				   object_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_access((cephfsal_handle_t *) object_handle,
			 (cephfsal_op_context_t *) p_context, access_type,
			 object_attributes);
}

fsal_status_t WRAP_CEPHFSAL_getattrs(fsal_handle_t * p_filehandle,       /* IN */
				     fsal_op_context_t * p_context,      /* IN */
				     fsal_attrib_list_t *
				     p_object_attributes /* IN/OUT */ )
{
  return CEPHFSAL_getattrs((cephfsal_handle_t *) p_filehandle,
			   (cephfsal_op_context_t *) p_context, p_object_attributes);
}

fsal_status_t WRAP_CEPHFSAL_setattrs(fsal_handle_t * p_filehandle,       /* IN */
                                    fsal_op_context_t * p_context,      /* IN */
                                    fsal_attrib_list_t * p_attrib_set,  /* IN */
                                    fsal_attrib_list_t *
                                    p_object_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_setattrs((cephfsal_handle_t *) p_filehandle,
                          (cephfsal_op_context_t *) p_context, p_attrib_set,
                          p_object_attributes);
}

fsal_status_t WRAP_CEPHFSAL_BuildExportContext(fsal_export_context_t * p_export_context, /* OUT */
                                              fsal_path_t * p_export_path,      /* IN */
                                              char *fs_specific_options /* IN */ )
{
  return CEPHFSAL_BuildExportContext((cephfsal_export_context_t *) p_export_context,
                                    p_export_path, fs_specific_options);
}

fsal_status_t WRAP_CEPHFSAL_CleanUpExportContext(fsal_export_context_t * p_export_context)
{
  return CEPHFSAL_CleanUpExportContext((cephfsal_export_context_t *) p_export_context);
}

fsal_status_t WRAP_CEPHFSAL_InitClientContext(fsal_op_context_t * p_thr_context)
{
  return CEPHFSAL_InitClientContext((cephfsal_op_context_t *) p_thr_context);
}

fsal_status_t WRAP_CEPHFSAL_GetClientContext(fsal_op_context_t * p_thr_context,  /* IN/OUT  */
                                            fsal_export_context_t * p_export_context,   /* IN */
                                            fsal_uid_t uid,     /* IN */
                                            fsal_gid_t gid,     /* IN */
                                            fsal_gid_t * alt_groups,    /* IN */
                                            fsal_count_t nb_alt_groups /* IN */ )
{
  return CEPHFSAL_GetClientContext((cephfsal_op_context_t *) p_thr_context,
                                  (cephfsal_export_context_t *) p_export_context, uid, gid,
                                  alt_groups, nb_alt_groups);
}

fsal_status_t WRAP_CEPHFSAL_create(fsal_handle_t * p_parent_directory_handle,    /* IN */
                                  fsal_name_t * p_filename,     /* IN */
                                  fsal_op_context_t * p_context,        /* IN */
                                  fsal_accessmode_t accessmode, /* IN */
                                  fsal_handle_t * p_object_handle,      /* OUT */
                                  fsal_attrib_list_t *
                                  p_object_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_create((cephfsal_handle_t *) p_parent_directory_handle, p_filename,
                        (cephfsal_op_context_t *) p_context, accessmode,
                        (cephfsal_handle_t *) p_object_handle, p_object_attributes);
}

fsal_status_t WRAP_CEPHFSAL_mkdir(fsal_handle_t * p_parent_directory_handle,     /* IN */
                                 fsal_name_t * p_dirname,       /* IN */
                                 fsal_op_context_t * p_context, /* IN */
                                 fsal_accessmode_t accessmode,  /* IN */
                                 fsal_handle_t * p_object_handle,       /* OUT */
                                 fsal_attrib_list_t *
                                 p_object_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_mkdir((cephfsal_handle_t *) p_parent_directory_handle, p_dirname,
                       (cephfsal_op_context_t *) p_context, accessmode,
                       (cephfsal_handle_t *) p_object_handle, p_object_attributes);
}

fsal_status_t WRAP_CEPHFSAL_link(fsal_handle_t * p_target_handle,        /* IN */
                                fsal_handle_t * p_dir_handle,   /* IN */
                                fsal_name_t * p_link_name,      /* IN */
                                fsal_op_context_t * p_context,  /* IN */
                                fsal_attrib_list_t * p_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_link((cephfsal_handle_t *) p_target_handle,
                      (cephfsal_handle_t *) p_dir_handle, p_link_name,
                      (cephfsal_op_context_t *) p_context, p_attributes);
}

fsal_status_t WRAP_CEPHFSAL_mknode(fsal_handle_t * parentdir_handle,     /* IN */
                                  fsal_name_t * p_node_name,    /* IN */
                                  fsal_op_context_t * p_context,        /* IN */
                                  fsal_accessmode_t accessmode, /* IN */
                                  fsal_nodetype_t nodetype,     /* IN */
                                  fsal_dev_t * dev,     /* IN */
                                  fsal_handle_t * p_object_handle,      /* OUT (handle to the created node) */
                                  fsal_attrib_list_t * node_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_mknode((cephfsal_handle_t *) parentdir_handle, p_node_name,
                        (cephfsal_op_context_t *) p_context, accessmode, nodetype, dev,
                        (cephfsal_handle_t *) p_object_handle, node_attributes);
}

fsal_status_t WRAP_CEPHFSAL_opendir(fsal_handle_t * p_dir_handle,        /* IN */
                                   fsal_op_context_t * p_context,       /* IN */
                                   fsal_dir_t * p_dir_descriptor,       /* OUT */
                                   fsal_attrib_list_t *
                                   p_dir_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_opendir((cephfsal_handle_t *) p_dir_handle,
                         (cephfsal_op_context_t *) p_context,
                         (cephfsal_dir_t *) p_dir_descriptor, p_dir_attributes);
}

fsal_status_t WRAP_CEPHFSAL_readdir(fsal_dir_t * p_dir_descriptor,       /* IN */
                                   fsal_cookie_t start_position,        /* IN */
                                   fsal_attrib_mask_t get_attr_mask,    /* IN */
                                   fsal_mdsize_t buffersize,    /* IN */
                                   fsal_dirent_t * p_pdirent,   /* OUT */
                                   fsal_cookie_t * p_end_position,      /* OUT */
                                   fsal_count_t * p_nb_entries, /* OUT */
                                   fsal_boolean_t * p_end_of_dir /* OUT */ )
{
  cephfsal_cookie_t xfscookie;

  memcpy((char *)&xfscookie, (char *)&start_position, sizeof(cephfsal_cookie_t));

  return CEPHFSAL_readdir((cephfsal_dir_t *) p_dir_descriptor, xfscookie, get_attr_mask,
                         buffersize, p_pdirent, (cephfsal_cookie_t *) p_end_position,
                         p_nb_entries, p_end_of_dir);
}

fsal_status_t WRAP_CEPHFSAL_closedir(fsal_dir_t * p_dir_descriptor /* IN */ )
{
  return CEPHFSAL_closedir((cephfsal_dir_t *) p_dir_descriptor);
}

fsal_status_t WRAP_CEPHFSAL_open_by_name(fsal_handle_t * dirhandle,      /* IN */
                                        fsal_name_t * filename, /* IN */
                                        fsal_op_context_t * p_context,  /* IN */
                                        fsal_openflags_t openflags,     /* IN */
                                        fsal_file_t * file_descriptor,  /* OUT */
                                        fsal_attrib_list_t *
                                        file_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_open_by_name((cephfsal_handle_t *) dirhandle, filename,
                              (cephfsal_op_context_t *) p_context, openflags,
                              (cephfsal_file_t *) file_descriptor, file_attributes);
}

fsal_status_t WRAP_CEPHFSAL_open(fsal_handle_t * p_filehandle,   /* IN */
                                fsal_op_context_t * p_context,  /* IN */
                                fsal_openflags_t openflags,     /* IN */
                                fsal_file_t * p_file_descriptor,        /* OUT */
                                fsal_attrib_list_t * p_file_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_open((cephfsal_handle_t *) p_filehandle,
                      (cephfsal_op_context_t *) p_context, openflags,
                      (cephfsal_file_t *) p_file_descriptor, p_file_attributes);
}

fsal_status_t WRAP_CEPHFSAL_read(fsal_file_t * p_file_descriptor,        /* IN */
                                fsal_seek_t * p_seek_descriptor,        /* [IN] */
                                fsal_size_t buffer_size,        /* IN */
                                caddr_t buffer, /* OUT */
                                fsal_size_t * p_read_amount,    /* OUT */
                                fsal_boolean_t * p_end_of_file /* OUT */ )
{
  return CEPHFSAL_read((cephfsal_file_t *) p_file_descriptor, p_seek_descriptor,
                      buffer_size, buffer, p_read_amount, p_end_of_file);
}

fsal_status_t WRAP_CEPHFSAL_write(fsal_file_t * p_file_descriptor,       /* IN */
                                 fsal_seek_t * p_seek_descriptor,       /* IN */
                                 fsal_size_t buffer_size,       /* IN */
                                 caddr_t buffer,        /* IN */
                                 fsal_size_t * p_write_amount /* OUT */ )
{
  return CEPHFSAL_write((cephfsal_file_t *) p_file_descriptor, p_seek_descriptor,
                       buffer_size, buffer, p_write_amount);
}

fsal_status_t WRAP_CEPHFSAL_sync(fsal_file_t* p_file_descriptor /* IN */)
{
  return CEPHFSAL_sync((cephfsal_file_t *) p_file_descriptor);
}

fsal_status_t WRAP_CEPHFSAL_close(fsal_file_t * p_file_descriptor /* IN */ )
{
  return CEPHFSAL_close((cephfsal_file_t *) p_file_descriptor);
}

fsal_status_t WRAP_CEPHFSAL_open_by_fileid(fsal_handle_t * filehandle,   /* IN */
                                          fsal_u64_t fileid,    /* IN */
                                          fsal_op_context_t * p_context,        /* IN */
                                          fsal_openflags_t openflags,   /* IN */
                                          fsal_file_t * file_descriptor,        /* OUT */
                                          fsal_attrib_list_t *
                                          file_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_open_by_fileid((cephfsal_handle_t *) filehandle, fileid,
                                (cephfsal_op_context_t *) p_context, openflags,
                                (cephfsal_file_t *) file_descriptor, file_attributes);
}

fsal_status_t WRAP_CEPHFSAL_close_by_fileid(fsal_file_t * file_descriptor /* IN */ ,
                                           fsal_u64_t fileid)
{
  return CEPHFSAL_close_by_fileid((cephfsal_file_t *) file_descriptor, fileid);
}

fsal_status_t WRAP_CEPHFSAL_static_fsinfo(fsal_handle_t * p_filehandle,  /* IN */
                                         fsal_op_context_t * p_context, /* IN */
                                         fsal_staticfsinfo_t * p_staticinfo /* OUT */ )
{
  return CEPHFSAL_static_fsinfo((cephfsal_handle_t *) p_filehandle,
                               (cephfsal_op_context_t *) p_context, p_staticinfo);
}

fsal_status_t WRAP_CEPHFSAL_dynamic_fsinfo(fsal_handle_t * p_filehandle, /* IN */
                                          fsal_op_context_t * p_context,        /* IN */
                                          fsal_dynamicfsinfo_t * p_dynamicinfo /* OUT */ )
{
  return CEPHFSAL_dynamic_fsinfo((cephfsal_handle_t *) p_filehandle,
                                (cephfsal_op_context_t *) p_context, p_dynamicinfo);
}

fsal_status_t WRAP_CEPHFSAL_Init(fsal_parameter_t * init_info /* IN */ )
{
  return CEPHFSAL_Init(init_info);
}

fsal_status_t WRAP_CEPHFSAL_terminate()
{
  return CEPHFSAL_terminate();
}

fsal_status_t WRAP_CEPHFSAL_test_access(fsal_op_context_t * p_context,   /* IN */
                                       fsal_accessflags_t access_type,  /* IN */
                                       fsal_attrib_list_t * p_object_attributes /* IN */ )
{
  return CEPHFSAL_test_access((cephfsal_op_context_t *) p_context, access_type,
                             p_object_attributes);
}

fsal_status_t WRAP_CEPHFSAL_setattr_access(fsal_op_context_t * p_context,        /* IN */
                                          fsal_attrib_list_t * candidate_attributes,    /* IN */
                                          fsal_attrib_list_t *
                                          object_attributes /* IN */ )
{
  return CEPHFSAL_setattr_access((cephfsal_op_context_t *) p_context, candidate_attributes,
                                object_attributes);
}

fsal_status_t WRAP_CEPHFSAL_rename_access(fsal_op_context_t * pcontext,  /* IN */
                                         fsal_attrib_list_t * pattrsrc, /* IN */
                                         fsal_attrib_list_t * pattrdest)        /* IN */
{
  return CEPHFSAL_rename_access((cephfsal_op_context_t *) pcontext, pattrsrc, pattrdest);
}

fsal_status_t WRAP_CEPHFSAL_create_access(fsal_op_context_t * pcontext,  /* IN */
                                         fsal_attrib_list_t * pattr)    /* IN */
{
  return CEPHFSAL_create_access((cephfsal_op_context_t *) pcontext, pattr);
}

fsal_status_t WRAP_CEPHFSAL_unlink_access(fsal_op_context_t * pcontext,  /* IN */
                                         fsal_attrib_list_t * pattr)    /* IN */
{
  return CEPHFSAL_unlink_access((cephfsal_op_context_t *) pcontext, pattr);
}

fsal_status_t WRAP_CEPHFSAL_link_access(fsal_op_context_t * pcontext,    /* IN */
                                       fsal_attrib_list_t * pattr)      /* IN */
{
  return CEPHFSAL_link_access((cephfsal_op_context_t *) pcontext, pattr);
}

fsal_status_t WRAP_CEPHFSAL_merge_attrs(fsal_attrib_list_t * pinit_attr,
                                       fsal_attrib_list_t * pnew_attr,
                                       fsal_attrib_list_t * presult_attr)
{
  return CEPHFSAL_merge_attrs(pinit_attr, pnew_attr, presult_attr);
}

fsal_status_t WRAP_CEPHFSAL_lookup(fsal_handle_t * p_parent_directory_handle,    /* IN */
                                  fsal_name_t * p_filename,     /* IN */
                                  fsal_op_context_t * p_context,        /* IN */
                                  fsal_handle_t * p_object_handle,      /* OUT */
                                  fsal_attrib_list_t *
                                  p_object_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_lookup((cephfsal_handle_t *) p_parent_directory_handle, p_filename,
                        (cephfsal_op_context_t *) p_context,
                        (cephfsal_handle_t *) p_object_handle, p_object_attributes);
}

fsal_status_t WRAP_CEPHFSAL_lookupPath(fsal_path_t * p_path,     /* IN */
                                      fsal_op_context_t * p_context,    /* IN */
                                      fsal_handle_t * object_handle,    /* OUT */
                                      fsal_attrib_list_t *
                                      p_object_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_lookupPath(p_path, (cephfsal_op_context_t *) p_context,
                            (cephfsal_handle_t *) object_handle, p_object_attributes);
}

fsal_status_t WRAP_CEPHFSAL_lookupJunction(fsal_handle_t * p_junction_handle,    /* IN */
                                          fsal_op_context_t * p_context,        /* IN */
                                          fsal_handle_t * p_fsoot_handle,       /* OUT */
                                          fsal_attrib_list_t *
                                          p_fsroot_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_lookupJunction((cephfsal_handle_t *) p_junction_handle,
                                (cephfsal_op_context_t *) p_context,
                                (cephfsal_handle_t *) p_fsoot_handle, p_fsroot_attributes);
}

fsal_status_t WRAP_CEPHFSAL_lock(fsal_file_t * obj_handle,
                                fsal_lockdesc_t * ldesc, fsal_boolean_t blocking)
{
  return CEPHFSAL_lock((cephfsal_file_t *) obj_handle, (cephfsal_lockdesc_t *) ldesc,
                      blocking);
}

fsal_status_t WRAP_CEPHFSAL_changelock(fsal_lockdesc_t * lock_descriptor,        /* IN / OUT */
                                      fsal_lockparam_t * lock_info /* IN */ )
{
  return CEPHFSAL_changelock((cephfsal_lockdesc_t *) lock_descriptor, lock_info);
}

fsal_status_t WRAP_CEPHFSAL_unlock(fsal_file_t * obj_handle, fsal_lockdesc_t * ldesc)
{
  return CEPHFSAL_unlock((cephfsal_file_t *) obj_handle, (cephfsal_lockdesc_t *) ldesc);
}

fsal_status_t WRAP_CEPHFSAL_getlock(fsal_file_t * obj_handle, fsal_lockdesc_t * ldesc)
{
  return CEPHFSAL_getlock((cephfsal_file_t *) obj_handle, (cephfsal_lockdesc_t *) ldesc);
}

fsal_status_t WRAP_CEPHFSAL_CleanObjectResources(fsal_handle_t * in_fsal_handle)
{
  return CEPHFSAL_CleanObjectResources((cephfsal_handle_t *) in_fsal_handle);
}

fsal_status_t WRAP_CEPHFSAL_set_quota(fsal_path_t * pfsal_path,  /* IN */
                                     int quota_type,    /* IN */
                                     fsal_uid_t fsal_uid,       /* IN */
                                     fsal_quota_t * pquota,     /* IN */
                                     fsal_quota_t * presquota)  /* OUT */
{
  return CEPHFSAL_set_quota(pfsal_path, quota_type, fsal_uid, pquota, presquota);
}

fsal_status_t WRAP_CEPHFSAL_get_quota(fsal_path_t * pfsal_path,  /* IN */
                                     int quota_type,    /* IN */
                                     fsal_uid_t fsal_uid,       /* IN */
                                     fsal_quota_t * pquota)     /* OUT */
{
  return CEPHFSAL_get_quota(pfsal_path, quota_type, fsal_uid, pquota);
}

fsal_status_t WRAP_CEPHFSAL_rcp(fsal_handle_t * filehandle,      /* IN */
                               fsal_op_context_t * p_context,   /* IN */
                               fsal_path_t * p_local_path,      /* IN */
                               fsal_rcpflag_t transfer_opt /* IN */ )
{
  return CEPHFSAL_rcp((cephfsal_handle_t *) filehandle, (cephfsal_op_context_t *) p_context,
                     p_local_path, transfer_opt);
}

fsal_status_t WRAP_CEPHFSAL_rcp_by_fileid(fsal_handle_t * filehandle,    /* IN */
                                         fsal_u64_t fileid,     /* IN */
                                         fsal_op_context_t * p_context, /* IN */
                                         fsal_path_t * p_local_path,    /* IN */
                                         fsal_rcpflag_t transfer_opt /* IN */ )
{
  return CEPHFSAL_rcp_by_fileid((cephfsal_handle_t *) filehandle, fileid,
                               (cephfsal_op_context_t *) p_context, p_local_path,
                               transfer_opt);
}

fsal_status_t WRAP_CEPHFSAL_rename(fsal_handle_t * p_old_parentdir_handle,       /* IN */
                                  fsal_name_t * p_old_name,     /* IN */
                                  fsal_handle_t * p_new_parentdir_handle,       /* IN */
                                  fsal_name_t * p_new_name,     /* IN */
                                  fsal_op_context_t * p_context,        /* IN */
                                  fsal_attrib_list_t * p_src_dir_attributes,    /* [ IN/OUT ] */
                                  fsal_attrib_list_t *
                                  p_tgt_dir_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_rename((cephfsal_handle_t *) p_old_parentdir_handle, p_old_name,
                        (cephfsal_handle_t *) p_new_parentdir_handle, p_new_name,
                        (cephfsal_op_context_t *) p_context, p_src_dir_attributes,
                        p_tgt_dir_attributes);
}

void WRAP_CEPHFSAL_get_stats(fsal_statistics_t * stats,  /* OUT */
                            fsal_boolean_t reset /* IN */ )
{
  return CEPHFSAL_get_stats(stats, reset);
}

fsal_status_t WRAP_CEPHFSAL_readlink(fsal_handle_t * p_linkhandle,       /* IN */
                                    fsal_op_context_t * p_context,      /* IN */
                                    fsal_path_t * p_link_content,       /* OUT */
                                    fsal_attrib_list_t *
                                    p_link_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_readlink((cephfsal_handle_t *) p_linkhandle,
                          (cephfsal_op_context_t *) p_context, p_link_content,
                          p_link_attributes);
}

fsal_status_t WRAP_CEPHFSAL_symlink(fsal_handle_t * p_parent_directory_handle,   /* IN */
                                   fsal_name_t * p_linkname,    /* IN */
                                   fsal_path_t * p_linkcontent, /* IN */
                                   fsal_op_context_t * p_context,       /* IN */
                                   fsal_accessmode_t accessmode,        /* IN (ignored) */
                                   fsal_handle_t * p_link_handle,       /* OUT */
                                   fsal_attrib_list_t *
                                   p_link_attributes /* [ IN/OUT ] */ )
{
  return CEPHFSAL_symlink((cephfsal_handle_t *) p_parent_directory_handle, p_linkname,
                         p_linkcontent, (cephfsal_op_context_t *) p_context, accessmode,
                         (cephfsal_handle_t *) p_link_handle, p_link_attributes);
}

int WRAP_CEPHFSAL_handlecmp(fsal_handle_t * handle1, fsal_handle_t * handle2,
                           fsal_status_t * status)
{
  return CEPHFSAL_handlecmp((cephfsal_handle_t *) handle1, (cephfsal_handle_t *) handle2,
                           status);
}

unsigned int WRAP_CEPHFSAL_Handle_to_HashIndex(fsal_handle_t * p_handle,
                                              unsigned int cookie,
                                              unsigned int alphabet_len,
                                              unsigned int index_size)
{
  return CEPHFSAL_Handle_to_HashIndex((cephfsal_handle_t *) p_handle, cookie, alphabet_len,
                                     index_size);
}

unsigned int WRAP_CEPHFSAL_Handle_to_RBTIndex(fsal_handle_t * p_handle,
                                             unsigned int cookie)
{
  return CEPHFSAL_Handle_to_RBTIndex((cephfsal_handle_t *) p_handle, cookie);
}

fsal_status_t WRAP_CEPHFSAL_DigestHandle(fsal_export_context_t * p_exportcontext,        /* IN */
                                        fsal_digesttype_t output_type,  /* IN */
                                        fsal_handle_t * p_in_fsal_handle,       /* IN */
                                        caddr_t out_buff /* OUT */ )
{
  return CEPHFSAL_DigestHandle((cephfsal_export_context_t *) p_exportcontext, output_type,
                              (cephfsal_handle_t *) p_in_fsal_handle, out_buff);
}

fsal_status_t WRAP_CEPHFSAL_ExpandHandle(fsal_export_context_t * p_expcontext,   /* IN */
                                        fsal_digesttype_t in_type,      /* IN */
                                        caddr_t in_buff,        /* IN */
                                        fsal_handle_t * p_out_fsal_handle /* OUT */ )
{
  return CEPHFSAL_ExpandHandle((cephfsal_export_context_t *) p_expcontext, in_type, in_buff,
                              (cephfsal_handle_t *) p_out_fsal_handle);
}

fsal_status_t WRAP_CEPHFSAL_SetDefault_FSAL_parameter(fsal_parameter_t * out_parameter)
{
  return CEPHFSAL_SetDefault_FSAL_parameter(out_parameter);
}

fsal_status_t WRAP_CEPHFSAL_SetDefault_FS_common_parameter(fsal_parameter_t *
                                                          out_parameter)
{
  return CEPHFSAL_SetDefault_FS_common_parameter(out_parameter);
}

fsal_status_t WRAP_CEPHFSAL_SetDefault_FS_specific_parameter(fsal_parameter_t *
                                                            out_parameter)
{
  return CEPHFSAL_SetDefault_FS_specific_parameter(out_parameter);
}

fsal_status_t WRAP_CEPHFSAL_load_FSAL_parameter_from_conf(config_file_t in_config,
                                                         fsal_parameter_t * out_parameter)
{
  return CEPHFSAL_load_FSAL_parameter_from_conf(in_config, out_parameter);
}

fsal_status_t WRAP_CEPHFSAL_load_FS_common_parameter_from_conf(config_file_t in_config,
                                                              fsal_parameter_t *
                                                              out_parameter)
{
  return CEPHFSAL_load_FS_common_parameter_from_conf(in_config, out_parameter);
}

fsal_status_t WRAP_CEPHFSAL_load_FS_specific_parameter_from_conf(config_file_t in_config,
                                                                fsal_parameter_t *
                                                                out_parameter)
{
  return CEPHFSAL_load_FS_specific_parameter_from_conf(in_config, out_parameter);
}

fsal_status_t WRAP_CEPHFSAL_truncate(fsal_handle_t * p_filehandle,
                                    fsal_op_context_t * p_context,
                                    fsal_size_t length,
                                    fsal_file_t * file_descriptor,
                                    fsal_attrib_list_t * p_object_attributes)
{
  return CEPHFSAL_truncate((cephfsal_handle_t *) p_filehandle,
                          (cephfsal_op_context_t *) p_context, length,
                          (cephfsal_file_t *) file_descriptor, p_object_attributes);
}

fsal_status_t WRAP_CEPHFSAL_unlink(fsal_handle_t * p_parent_directory_handle,    /* IN */
                                  fsal_name_t * p_object_name,  /* IN */
                                  fsal_op_context_t * p_context,        /* IN */
                                  fsal_attrib_list_t *
                                  p_parent_directory_attributes /* [IN/OUT ] */ )
{
  return CEPHFSAL_unlink((cephfsal_handle_t *) p_parent_directory_handle, p_object_name,
                        (cephfsal_op_context_t *) p_context,
                        p_parent_directory_attributes);
}

char *WRAP_CEPHFSAL_GetFSName()
{
  return CEPHFSAL_GetFSName();
}

fsal_status_t WRAP_CEPHFSAL_GetXAttrAttrs(fsal_handle_t * p_objecthandle,        /* IN */
                                         fsal_op_context_t * p_context, /* IN */
                                         unsigned int xattr_id, /* IN */
                                         fsal_attrib_list_t * p_attrs)
{
  return CEPHFSAL_GetXAttrAttrs((cephfsal_handle_t *) p_objecthandle,
                               (cephfsal_op_context_t *) p_context, xattr_id, p_attrs);
}

fsal_status_t WRAP_CEPHFSAL_ListXAttrs(fsal_handle_t * p_objecthandle,   /* IN */
                                      unsigned int cookie,      /* IN */
                                      fsal_op_context_t * p_context,    /* IN */
                                      fsal_xattrent_t * xattrs_tab,     /* IN/OUT */
                                      unsigned int xattrs_tabsize,      /* IN */
                                      unsigned int *p_nb_returned,      /* OUT */
                                      int *end_of_list /* OUT */ )
{
  return CEPHFSAL_ListXAttrs((cephfsal_handle_t *) p_objecthandle, cookie,
                            (cephfsal_op_context_t *) p_context, xattrs_tab,
                            xattrs_tabsize, p_nb_returned, end_of_list);
}

fsal_status_t WRAP_CEPHFSAL_GetXAttrValueById(fsal_handle_t * p_objecthandle,    /* IN */
                                             unsigned int xattr_id,     /* IN */
                                             fsal_op_context_t * p_context,     /* IN */
                                             caddr_t buffer_addr,       /* IN/OUT */
                                             size_t buffer_size,        /* IN */
                                             size_t * p_output_size /* OUT */ )
{
  return CEPHFSAL_GetXAttrValueById((cephfsal_handle_t *) p_objecthandle, xattr_id,
                                   (cephfsal_op_context_t *) p_context, buffer_addr,
                                   buffer_size, p_output_size);
}

fsal_status_t WRAP_CEPHFSAL_GetXAttrIdByName(fsal_handle_t * p_objecthandle,     /* IN */
                                            const fsal_name_t * xattr_name,     /* IN */
                                            fsal_op_context_t * p_context,      /* IN */
                                            unsigned int *pxattr_id /* OUT */ )
{
  return CEPHFSAL_GetXAttrIdByName((cephfsal_handle_t *) p_objecthandle, xattr_name,
                                  (cephfsal_op_context_t *) p_context, pxattr_id);
}

fsal_status_t WRAP_CEPHFSAL_GetXAttrValueByName(fsal_handle_t * p_objecthandle,  /* IN */
                                               const fsal_name_t * xattr_name,  /* IN */
                                               fsal_op_context_t * p_context,   /* IN */
                                               caddr_t buffer_addr,     /* IN/OUT */
                                               size_t buffer_size,      /* IN */
                                               size_t * p_output_size /* OUT */ )
{
  return CEPHFSAL_GetXAttrValueByName((cephfsal_handle_t *) p_objecthandle, xattr_name,
                                     (cephfsal_op_context_t *) p_context, buffer_addr,
                                     buffer_size, p_output_size);
}

fsal_status_t WRAP_CEPHFSAL_SetXAttrValue(fsal_handle_t * p_objecthandle,        /* IN */
                                         const fsal_name_t * xattr_name,        /* IN */
                                         fsal_op_context_t * p_context, /* IN */
                                         caddr_t buffer_addr,   /* IN */
                                         size_t buffer_size,    /* IN */
                                         int create /* IN */ )
{
  return CEPHFSAL_SetXAttrValue((cephfsal_handle_t *) p_objecthandle, xattr_name,
                               (cephfsal_op_context_t *) p_context, buffer_addr,
                               buffer_size, create);
}

fsal_status_t WRAP_CEPHFSAL_SetXAttrValueById(fsal_handle_t * p_objecthandle,    /* IN */
                                             unsigned int xattr_id,     /* IN */
                                             fsal_op_context_t * p_context,     /* IN */
                                             caddr_t buffer_addr,       /* IN */
                                             size_t buffer_size /* IN */ )
{
  return CEPHFSAL_SetXAttrValueById((cephfsal_handle_t *) p_objecthandle, xattr_id,
                                   (cephfsal_op_context_t *) p_context, buffer_addr,
                                   buffer_size);
}

fsal_status_t WRAP_CEPHFSAL_RemoveXAttrById(fsal_handle_t * p_objecthandle,      /* IN */
                                           fsal_op_context_t * p_context,       /* IN */
                                           unsigned int xattr_id)       /* IN */
{
  return CEPHFSAL_RemoveXAttrById((cephfsal_handle_t *) p_objecthandle,
                                 (cephfsal_op_context_t *) p_context, xattr_id);
}

fsal_status_t WRAP_CEPHFSAL_getextattrs(fsal_handle_t * p_filehandle, /* IN */
                                       fsal_op_context_t * p_context,        /* IN */
                                       fsal_extattrib_list_t * p_object_attributes /* OUT */)
{
  return CEPHFSAL_getextattrs( (cephfsal_handle_t *)p_filehandle,
                                (cephfsal_op_context_t *) p_context, p_object_attributes ) ;
}

fsal_status_t WRAP_CEPHFSAL_RemoveXAttrByName(fsal_handle_t * p_objecthandle,    /* IN */
                                             fsal_op_context_t * p_context,     /* IN */
                                             const fsal_name_t * xattr_name)    /* IN */
{
  return CEPHFSAL_RemoveXAttrByName((cephfsal_handle_t *) p_objecthandle,
                                   (cephfsal_op_context_t *) p_context, xattr_name);
}

#ifdef _USE_FSALMDS
fsal_status_t WRAP_CEPHFSAL_layoutget(fsal_handle_t* filehandle,
				      fsal_layouttype_t type,
				      fsal_layoutiomode_t iomode,
				      fsal_off_t offset, fsal_size_t length,
				      fsal_size_t minlength,
				      fsal_layout_t** layouts,
				      int *numlayouts,
				      fsal_boolean_t *return_on_close,
				      fsal_op_context_t *context,
				      stateid4* stateid,
				      void* opaque)
{
  return CEPHFSAL_layoutget((cephfsal_handle_t *) filehandle,
			    type, iomode, offset, length,
			    minlength, layouts,
			    numlayouts,
			    return_on_close,
			    (cephfsal_op_context_t *) context,
			    stateid,
			    opaque);
}

fsal_status_t WRAP_CEPHFSAL_layoutreturn(fsal_handle_t* filehandle,
					 fsal_layouttype_t type,
					 fsal_layoutiomode_t iomode,
					 fsal_off_t offset,
					 fsal_size_t length,
					 fsal_op_context_t* context,
					 bool_t* nomore,
					 stateid4* stateid)
{
  return CEPHFSAL_layoutreturn((cephfsal_handle_t*) filehandle,
			       type, iomode, offset,
			       length, (cephfsal_op_context_t*)context,
			       nomore, stateid);
}

fsal_status_t WRAP_CEPHFSAL_layoutcommit(fsal_handle_t* filehandle,
					 fsal_off_t offset,
					 fsal_size_t length,
					 fsal_off_t* newoff,
					 fsal_time_t* newtime,
					 stateid4 stateid,
					 layoutupdate4 layoutupdate,
					 fsal_op_context_t* pcontext)
{
  CEPHFSAL_layoutcommit((cephfsal_handle_t*)filehandle, offset,
			length, newoff, newtime, stateid,
			layoutupdate, (cephfsal_op_context_t*) pcontext);
}

fsal_status_t WRAP_CEPHFSAL_getdeviceinfo(fsal_layouttype_t type,
					  fsal_deviceid_t id,
					  device_addr4* devaddr)
{
  return CEPHFSAL_getdeviceinfo(type, id, devaddr);
}

fsal_status_t WRAP_CEPHFSAL_getdevicelist(fsal_handle_t* filehandle,
					  fsal_layouttype_t type,
					  int *numdevices,
					  uint64_t *cookie,
					  fsal_boolean_t* eof,
					  void* buff,
					  size_t* len)
{
  return CEPHFSAL_getdevicelist((cephfsal_handle_t*) filehandle,
				type, numdevices, cookie, eof, buff,
				len);
}
#endif /* _USE_FSALMDS */

#ifdef _USE_FSALDS
fsal_status_t WRAP_CEPHFSAL_ds_read(fsal_handle_t * filehandle,     /*  IN  */
				    fsal_seek_t * seek_descriptor,  /* [IN] */
				    fsal_size_t buffer_size,        /*  IN  */
				    caddr_t buffer,                 /* OUT  */
				    fsal_size_t * read_amount,      /* OUT  */
				    fsal_boolean_t * end_of_file    /* OUT  */
    )
{
  return CEPHFSAL_ds_read((cephfsal_handle_t *) filehandle, seek_descriptor,
			  buffer_size, buffer, read_amount, end_of_file);
}

fsal_status_t WRAP_CEPHFSAL_ds_write(fsal_handle_t * filehandle,      /* IN */
				     fsal_seek_t * seek_descriptor,   /* IN */
				     fsal_size_t buffer_size,         /* IN */
				     caddr_t buffer,                  /* IN */
				     fsal_size_t * write_amount,      /* OUT */
				     fsal_boolean_t stable_flag       /* IN */
    )
{
  return CEPHFSAL_ds_write((cephfsal_handle_t *) filehandle,
			   seek_descriptor, buffer_size, buffer,
			   write_amount, stable_flag);
}

fsal_status_t WRAP_CEPHFSAL_ds_commit(fsal_handle_t * filehandle,     /* IN */
				      fsal_off_t offset,
				      fsal_size_t length)
{
  return CEPHFSAL_ds_commit((cephfsal_handle_t *) filehandle, offset,
			    length);
}
#endif /* _USE_FSALDS */

fsal_functions_t fsal_ceph_functions = {
  .fsal_access = WRAP_CEPHFSAL_access,
  .fsal_getattrs = WRAP_CEPHFSAL_getattrs,
  .fsal_setattrs = WRAP_CEPHFSAL_setattrs,
  .fsal_buildexportcontext = WRAP_CEPHFSAL_BuildExportContext,
  .fsal_cleanupexportcontext = WRAP_CEPHFSAL_CleanUpExportContext,
  .fsal_initclientcontext = WRAP_CEPHFSAL_InitClientContext,
  .fsal_getclientcontext = WRAP_CEPHFSAL_GetClientContext,
  .fsal_create = WRAP_CEPHFSAL_create,
  .fsal_mkdir = WRAP_CEPHFSAL_mkdir,
  .fsal_link = WRAP_CEPHFSAL_link,
  .fsal_mknode = WRAP_CEPHFSAL_mknode,
  .fsal_opendir = WRAP_CEPHFSAL_opendir,
  .fsal_readdir = WRAP_CEPHFSAL_readdir,
  .fsal_closedir = WRAP_CEPHFSAL_closedir,
  .fsal_open_by_name = WRAP_CEPHFSAL_open_by_name,
  .fsal_open = WRAP_CEPHFSAL_open,
  .fsal_read = WRAP_CEPHFSAL_read,
  .fsal_write = WRAP_CEPHFSAL_write,
  .fsal_sync = WRAP_CEPHFSAL_sync,
  .fsal_close = WRAP_CEPHFSAL_close,
  .fsal_open_by_fileid = WRAP_CEPHFSAL_open_by_fileid,
  .fsal_close_by_fileid = WRAP_CEPHFSAL_close_by_fileid,
  .fsal_static_fsinfo = WRAP_CEPHFSAL_static_fsinfo,
  .fsal_dynamic_fsinfo = WRAP_CEPHFSAL_dynamic_fsinfo,
  .fsal_init = WRAP_CEPHFSAL_Init,
  .fsal_terminate = WRAP_CEPHFSAL_terminate,
  .fsal_test_access = WRAP_CEPHFSAL_test_access,
  .fsal_setattr_access = WRAP_CEPHFSAL_setattr_access,
  .fsal_rename_access = WRAP_CEPHFSAL_rename_access,
  .fsal_create_access = WRAP_CEPHFSAL_create_access,
  .fsal_unlink_access = WRAP_CEPHFSAL_unlink_access,
  .fsal_link_access = WRAP_CEPHFSAL_link_access,
  .fsal_merge_attrs = WRAP_CEPHFSAL_merge_attrs,
  .fsal_lookup = WRAP_CEPHFSAL_lookup,
  .fsal_lookuppath = WRAP_CEPHFSAL_lookupPath,
  .fsal_lookupjunction = WRAP_CEPHFSAL_lookupJunction,
  .fsal_lock = WRAP_CEPHFSAL_lock,
  .fsal_changelock = WRAP_CEPHFSAL_changelock,
  .fsal_unlock = WRAP_CEPHFSAL_unlock,
  .fsal_getlock = WRAP_CEPHFSAL_getlock,
  .fsal_cleanobjectresources = WRAP_CEPHFSAL_CleanObjectResources,
  .fsal_set_quota = WRAP_CEPHFSAL_set_quota,
  .fsal_get_quota = WRAP_CEPHFSAL_get_quota,
  .fsal_rcp = WRAP_CEPHFSAL_rcp,
  .fsal_rcp_by_fileid = WRAP_CEPHFSAL_rcp_by_fileid,
  .fsal_rename = WRAP_CEPHFSAL_rename,
  .fsal_get_stats = WRAP_CEPHFSAL_get_stats,
  .fsal_readlink = WRAP_CEPHFSAL_readlink,
  .fsal_symlink = WRAP_CEPHFSAL_symlink,
  .fsal_handlecmp = WRAP_CEPHFSAL_handlecmp,
  .fsal_handle_to_hashindex = WRAP_CEPHFSAL_Handle_to_HashIndex,
  .fsal_handle_to_rbtindex = WRAP_CEPHFSAL_Handle_to_RBTIndex,
  .fsal_digesthandle = WRAP_CEPHFSAL_DigestHandle,
  .fsal_expandhandle = WRAP_CEPHFSAL_ExpandHandle,
  .fsal_setdefault_fsal_parameter = WRAP_CEPHFSAL_SetDefault_FSAL_parameter,
  .fsal_setdefault_fs_common_parameter = WRAP_CEPHFSAL_SetDefault_FS_common_parameter,
  .fsal_setdefault_fs_specific_parameter = WRAP_CEPHFSAL_SetDefault_FS_specific_parameter,
  .fsal_load_fsal_parameter_from_conf = WRAP_CEPHFSAL_load_FSAL_parameter_from_conf,
  .fsal_load_fs_common_parameter_from_conf =
      WRAP_CEPHFSAL_load_FS_common_parameter_from_conf,
  .fsal_load_fs_specific_parameter_from_conf =
      WRAP_CEPHFSAL_load_FS_specific_parameter_from_conf,
  .fsal_truncate = WRAP_CEPHFSAL_truncate,
  .fsal_unlink = WRAP_CEPHFSAL_unlink,
  .fsal_getfsname = WRAP_CEPHFSAL_GetFSName,
  .fsal_getxattrattrs = WRAP_CEPHFSAL_GetXAttrAttrs,
  .fsal_listxattrs = WRAP_CEPHFSAL_ListXAttrs,
  .fsal_getxattrvaluebyid = WRAP_CEPHFSAL_GetXAttrValueById,
  .fsal_getxattridbyname = WRAP_CEPHFSAL_GetXAttrIdByName,
  .fsal_getxattrvaluebyname = WRAP_CEPHFSAL_GetXAttrValueByName,
  .fsal_setxattrvalue = WRAP_CEPHFSAL_SetXAttrValue,
  .fsal_setxattrvaluebyid = WRAP_CEPHFSAL_SetXAttrValueById,
  .fsal_removexattrbyid = WRAP_CEPHFSAL_RemoveXAttrById,
  .fsal_removexattrbyname = WRAP_CEPHFSAL_RemoveXAttrByName,
  .fsal_getextattrs = WRAP_CEPHFSAL_getextattrs,
  .fsal_getfileno = CEPHFSAL_GetFileno
};

fsal_const_t fsal_ceph_consts = {
  .fsal_handle_t_size = sizeof(cephfsal_handle_t),
  .fsal_op_context_t_size = sizeof(cephfsal_op_context_t),
  .fsal_export_context_t_size = sizeof(cephfsal_export_context_t),
  .fsal_file_t_size = sizeof(cephfsal_file_t),
  .fsal_cookie_t_size = sizeof(cephfsal_cookie_t),
  .fsal_lockdesc_t_size = sizeof(cephfsal_lockdesc_t),
  .fsal_cred_t_size = sizeof(cephfsal_cred_t),
  .fs_specific_initinfo_t_size = sizeof(cephfsal_specific_initinfo_t),
  .fsal_dir_t_size = sizeof(cephfsal_dir_t)
};

#ifdef _USE_FSALMDS
fsal_mdsfunctions_t fsal_ceph_mdsfunctions = {
  .fsal_layoutget = WRAP_CEPHFSAL_layoutget,
  .fsal_layoutreturn = WRAP_CEPHFSAL_layoutreturn,
  .fsal_layoutcommit = WRAP_CEPHFSAL_layoutcommit,
  .fsal_getdeviceinfo = WRAP_CEPHFSAL_getdeviceinfo,
  .fsal_getdevicelist = WRAP_CEPHFSAL_getdevicelist
};
#endif

#ifdef _USE_FSALDS
fsal_dsfunctions_t fsal_ceph_dsfunctions = {
  .fsal_ds_read = WRAP_CEPHFSAL_ds_read,
  .fsal_ds_write = WRAP_CEPHFSAL_ds_write,
  .fsal_ds_commit = WRAP_CEPHFSAL_ds_commit
};

#endif /* _USE_FSALDS */

fsal_functions_t FSAL_GetFunctions(void)
{
  return fsal_ceph_functions;
}                               /* FSAL_GetFunctions */

fsal_const_t FSAL_GetConsts(void)
{
  return fsal_ceph_consts;
}                               /* FSAL_GetConsts */

#ifdef _USE_FSALMDS
fsal_mdsfunctions_t FSAL_GetMDSFunctions(void)
{
  return fsal_ceph_mdsfunctions;
}
#endif /* _USE_FSALMDS */

#ifdef _USE_FSALDS
fsal_dsfunctions_t FSAL_GetDSFunctions(void)
{
  return fsal_ceph_dsfunctions;
}
#endif /* _USE_FSALDS */
