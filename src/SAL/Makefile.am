AM_CFLAGS                     = $(SAL_CFLAGS) $(SEC_CFLAGS)


noinst_LTLIBRARIES            = libsal.la


libsal_la_SOURCES = sal_glue.c    \
		    sal_general.c

if USE_SAL_LOCAL
SUBDIRS = LOCAL
endif

new: clean all

doc:
	doxygen ./doxygen.conf
	rep=`grep OUTPUT_DIRECTORY doxygen.conf | grep share  | awk -F '=' '{print $$2;}'` ; cd $$rep/latex ; make

