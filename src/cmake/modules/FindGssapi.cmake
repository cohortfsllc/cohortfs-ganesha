message(WARNING "looking for GSSAPI")

#e.g., Linux
check_include_files(gssapi.h GSSAPI_H)

if (NOT GSSAPI_H)
   # e.g., SunOS
   check_include_files(gssapi/gssapi.h GSSAPI_GSSAPI_H)
endif (NOT GSSAPI_H)

if ((GSSAPI_H) OR (GSSAPI_GSSAPI_H))
   if (CMAKE_SYSTEM_NAME MATCHES "SunOS")
      find_library(LIBGSS gss)
      if (NOT LIBGSS)
      	 message (FATAL_ERROR "Platform is Solaris but libgss.so not found")
	 set(HAVE_GSSAPI_H 0)
      else (NOT LIBGSS)
         set(HAVE_GSSAPI_H 1)
      endif (NOT LIBGSS)
   endif (CMAKE_SYSTEM_NAME MATCHES "SunOS")
else ((GSSAPI_H) OR (GSSAPI_GSSAPI_H))
   set(HAVE_GSSAPI_H 0)
endif ((GSSAPI_H) OR (GSSAPI_GSSAPI_H))
