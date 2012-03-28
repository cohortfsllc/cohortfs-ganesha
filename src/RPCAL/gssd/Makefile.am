noinst_LTLIBRARIES          = libgssd.la


KPREFIX		= @kprefix@

COMMON_SRCS = \
	context.c \
	context_mit.c \
	context_heimdal.c \
	context_lucid.c \
	context_spkm3.c \
	gss_util.c \
	gss_oids.c \
	err_util.c \
	\
	context.h \
	err_util.h \
	gss_oids.h \
	gss_util.h

libgssd_la_SOURCES = \
	$(COMMON_SRCS) \
	gssd.c \
	gssd_main_loop.c \
	gssd_proc.c \
	krb5_util.c \
	\
	gssd.h \
	krb5_util.h \
	write_bytes.h

libgssd_la_LDADD = $(RPCSECGSS_LIBS) $(GSSGLUE_LIBS) $(KRBLIBS)
libgssd_la_LDFLAGS = $(KRBLDFLAGS)

libgssd_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS) \
	$(RPCSECGSS_CFLAGS) $(GSSGLUE_CFLAGS) $(KRBCFLAGS)


MAINTAINERCLEANFILES = Makefile.in
