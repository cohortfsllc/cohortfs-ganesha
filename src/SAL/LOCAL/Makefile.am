AM_CFLAGS                     = $(SAL_CFLAGS) $(SEC_CFLAGS)


noinst_LTLIBRARIES          = libsallocal.la

libsallocal_la_SOURCES = sal_init.c           \
			 sal_internal.c       \
			 sal_internal.h       \
			 sal_share.c          \
			 sal_delegation.c     \
			 sal_dir_delegation.c \
			 sal_lock.c           \
			 sal_layout.c         \
			 sal_tools.c          \
			 sal_compat.c          \
                        ../../include/sal.h
new: clean all

doc:
	doxygen ./doxygen.conf
	rep=`grep OUTPUT_DIRECTORY doxygen.conf | grep share  | awk -F '=' '{print $$2;}'` ; cd $$rep/latex ; make

