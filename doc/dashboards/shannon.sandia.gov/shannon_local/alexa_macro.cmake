macro(do_alexa)

# Configure the ReconDriver build 
# Builds everything!
#
####################################################################################################################

SET_PROPERTY (GLOBAL PROPERTY SubProject ReconDriver_CUDA)
SET_PROPERTY (GLOBAL PROPERTY Label ReconDriver_CUDA)

SET(CONFIGURE_OPTIONS
  "-DTrilinos_PREFIX:PATH=${CTEST_BINARY_DIRECTORY}/TrilinosInstall"
  "-DSEACAS_BINARY_DIR:PATH=/home/gahanse/trilinos/host_seacas/bin"
  "-DCUSTOM_CXX_FLAGS:STRING=-Wfatal-errors"
  "-DAlexa_ENABLE_LAMBDAS:BOOL=OFF"
  "-DAlexa_RUN_CUDA_TESTS:BOOL=ON"
  "-DAlexa_MPIEXEC:STRING='${MPI_BASE_DIR}/bin/mpirun -np 1'"
   )
 
if(NOT EXISTS "${CTEST_BINARY_DIRECTORY}/ReconDriver")
  FILE(MAKE_DIRECTORY ${CTEST_BINARY_DIRECTORY}/ReconDriver)
endif()

IF (CLEAN_BUILD)
# Initial cache info
set( CACHE_CONTENTS "
SITE:STRING=${CTEST_SITE}
CMAKE_BUILD_TYPE:STRING=Release
CMAKE_GENERATOR:INTERNAL=${CTEST_CMAKE_GENERATOR}
BUILD_TESTING:BOOL=OFF
PRODUCT_REPO:STRING=${ReconDriver_REPOSITORY_LOCATION}
" )
file(WRITE "${CTEST_BINARY_DIRECTORY}/ReconDriver/CMakeCache.txt" "${CACHE_CONTENTS}")
ENDIF(CLEAN_BUILD)

CTEST_CONFIGURE(
          BUILD "${CTEST_BINARY_DIRECTORY}/ReconDriver"
          SOURCE "${CTEST_SOURCE_DIRECTORY}/ReconDriver"
          OPTIONS "${CONFIGURE_OPTIONS}"
          RETURN_VALUE HAD_ERROR
          APPEND
)

IF(CTEST_DO_SUBMIT)
  CTEST_SUBMIT(PARTS Configure
               RETURN_VALUE  S_HAD_ERROR
  )

  if(S_HAD_ERROR)
    message(FATAL_ERROR "Cannot submit ReconDriver configure results!")
  endif()
ENDIF()

if(HAD_ERROR)
	message(FATAL_ERROR "Cannot configure ReconDriver build!")
endif()

move_xml_file ("*Configure.xml" "Configure_ReconDriver.xml")

#
# Build ReconDriver
#
###################################################################################################################

SET(CTEST_BUILD_TARGET all)

MESSAGE("\nBuilding target: '${CTEST_BUILD_TARGET}' ...\n")

CTEST_BUILD(
          BUILD "${CTEST_BINARY_DIRECTORY}/ReconDriver"
          RETURN_VALUE  HAD_ERROR
          NUMBER_ERRORS  BUILD_LIBS_NUM_ERRORS
          APPEND
)

IF(CTEST_DO_SUBMIT)
  CTEST_SUBMIT(PARTS Build
               RETURN_VALUE  S_HAD_ERROR
  )

  if(S_HAD_ERROR)
        message(FATAL_ERROR "Cannot submit ReconDriver build results!")
  endif()
ENDIF(CTEST_DO_SUBMIT)

if(HAD_ERROR)
	message(FATAL_ERROR "Cannot build ReconDriver!")
endif()

move_xml_file ("*Build.xml" "Build_ReconDriver.xml")

#
# Run ReconDriver tests
#
##################################################################################################################

CTEST_TEST(
              BUILD "${CTEST_BINARY_DIRECTORY}/ReconDriver"
#              PARALLEL_LEVEL "${CTEST_PARALLEL_LEVEL}"
#              INCLUDE_LABEL "^${TRIBITS_PACKAGE}$"
              #NUMBER_FAILED  TEST_NUM_FAILED
)

IF(CTEST_DO_SUBMIT)
  CTEST_SUBMIT(PARTS Test
               RETURN_VALUE  HAD_ERROR
  )

  if(HAD_ERROR)
    message(FATAL_ERROR "Cannot submit ReconDriver test results!")
  endif()
ENDIF(CTEST_DO_SUBMIT)

endmacro(do_alexa)
