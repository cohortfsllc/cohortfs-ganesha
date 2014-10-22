add_definitions(
  -D_FILE_OFFSET_BITS=64
)

########### next target ###############

# Build source locations and parameters
IF(COHORT_PREFIX)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${COHORT_PREFIX}/include")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -L${COHORT_PREFIX}/lib")
ELSE()
  message(WARNING "no COHORT prefix")
ENDIF()

SET(fsalcohort_LIB_SRCS
   main.c
   export.c
   handle.c
   mds.c
   ds.c
   internal.c
   internal.h
)

add_library(fsalcohort SHARED ${fsalcohort_LIB_SRCS})

target_link_libraries(fsalcohort cephfs ceph-osd ${SYSTEM_LIBRARIES})

set_target_properties(fsalcohort PROPERTIES VERSION 4.2.0 SOVERSION 4)
install(TARGETS fsalcohort COMPONENT fsal DESTINATION ${FSAL_DESTINATION} )
