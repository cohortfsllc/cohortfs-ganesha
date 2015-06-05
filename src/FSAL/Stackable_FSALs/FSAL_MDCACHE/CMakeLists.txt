add_definitions(
  -D__USE_GNU
  -D_GNU_SOURCE
)

set( LIB_PREFIX 64)

########### next target ###############

SET(fsalmdcache_LIB_SRCS
   handle.c
   file.c
   xattrs.c
   mdcache_methods.h
   main.c
   export.c
)

add_library(fsalmdcache STATIC ${fsalmdcache_LIB_SRCS})

########### install files ###############
