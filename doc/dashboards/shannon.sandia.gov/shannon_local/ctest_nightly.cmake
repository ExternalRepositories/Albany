cmake_minimum_required(VERSION 2.8)

#SET(CTEST_TEST_TYPE Nightly)

SET(CTEST_TEST_TYPE Experimental)

# What to build and test
SET(DOWNLOAD_TRILINOS FALSE)
SET(DOWNLOAD_ALBANY FALSE)
SET(BUILD_TRILINOS FALSE)
SET(BUILD_ALBANY TRUE)
SET(CLEAN_BUILD FALSE)

# Begin User inputs:
set( CTEST_SITE             "shannon.sandia.gov" ) # generally the output of hostname
set( CTEST_DASHBOARD_ROOT   "$ENV{TEST_DIRECTORY}" ) # writable path
set( CTEST_SCRIPT_DIRECTORY   "$ENV{SCRIPT_DIRECTORY}" ) # where the scripts live
set( CTEST_CMAKE_GENERATOR  "Unix Makefiles" ) # What is your compilation apps ?
set( CTEST_BUILD_CONFIGURATION  Release) # What type of build do you want ?

set( CTEST_PROJECT_NAME         "Albany" )
set( CTEST_SOURCE_NAME          repos)
set( CTEST_BUILD_NAME           "cuda-nvcc-${CTEST_BUILD_CONFIGURATION}")
set( CTEST_BINARY_NAME          buildAlbany)

SET(PREFIX_DIR /home/gahanse)
SET(MPI_BASE_DIR /home/projects/x86-64/openmpi/1.8.4/gnu/4.7.2/cuda/7.0.28)
SET(INTEL_DIR /opt/intel/mkl/lib/intel64)


SET (CTEST_SOURCE_DIRECTORY "${CTEST_DASHBOARD_ROOT}/${CTEST_SOURCE_NAME}")
SET (CTEST_BINARY_DIRECTORY "${CTEST_DASHBOARD_ROOT}/${CTEST_BINARY_NAME}")

IF(NOT EXISTS "${CTEST_SOURCE_DIRECTORY}")
  FILE(MAKE_DIRECTORY "${CTEST_SOURCE_DIRECTORY}")
ENDIF()

IF(NOT EXISTS "${CTEST_BINARY_DIRECTORY}")
  FILE(MAKE_DIRECTORY "${CTEST_BINARY_DIRECTORY}")
ENDIF()

configure_file(${CTEST_SCRIPT_DIRECTORY}/CTestConfig.cmake
               ${CTEST_SOURCE_DIRECTORY}/CTestConfig.cmake COPYONLY)

# Rougly midnight 0700 UTC
SET(CTEST_NIGHTLY_START_TIME "07:00:00 UTC")
SET (CTEST_CMAKE_COMMAND "${PREFIX_DIR}/bin/cmake")
SET (CTEST_COMMAND "${PREFIX_DIR}/bin/ctest -D ${CTEST_TEST_TYPE}")
SET (CTEST_BUILD_FLAGS "-j16")

find_program(CTEST_GIT_COMMAND NAMES git)

# Point at the public Repo
SET(Trilinos_REPOSITORY_LOCATION software.sandia.gov:/git/Trilinos)
SET(SCOREC_REPOSITORY_LOCATION https://github.com/SCOREC/core.git)
SET(Albany_REPOSITORY_LOCATION https://github.com/gahansen/Albany.git)

IF (CLEAN_BUILD)

# Initial cache info
set( CACHE_CONTENTS "
SITE:STRING=${CTEST_SITE}
CMAKE_BUILD_TYPE:STRING=Release
CMAKE_GENERATOR:INTERNAL=${CTEST_CMAKE_GENERATOR}
BUILD_TESTING:BOOL=OFF
PRODUCT_REPO:STRING=${Albany_REPOSITORY_LOCATION}
" )

ctest_empty_binary_directory( "${CTEST_BINARY_DIRECTORY}" )
file(WRITE "${CTEST_BINARY_DIRECTORY}/CMakeCache.txt" "${CACHE_CONTENTS}")

ENDIF()

SET(TRILINOS_HOME "${CTEST_SOURCE_DIRECTORY}/Trilinos")

IF (DOWNLOAD_TRILINOS)
#
# Get the Trilinos repo
#
#########################################################################################################

set(CTEST_CHECKOUT_COMMAND)

if(NOT EXISTS "${TRILINOS_HOME}")
  EXECUTE_PROCESS(COMMAND "${CTEST_GIT_COMMAND}" 
    clone ${Trilinos_REPOSITORY_LOCATION} ${TRILINOS_HOME}
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    RESULT_VARIABLE HAD_ERROR)
  
   message(STATUS "out: ${_out}")
   message(STATUS "err: ${_err}")
   message(STATUS "res: ${HAD_ERROR}")
   if(HAD_ERROR)
	message(FATAL_ERROR "Cannot clone Trilinos repository!")
   endif()
endif()

if(NOT EXISTS "${TRILINOS_HOME}/SCOREC")
  EXECUTE_PROCESS(COMMAND "${CTEST_GIT_COMMAND}"
    clone ${SCOREC_REPOSITORY_LOCATION} ${TRILINOS_HOME}/SCOREC
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    RESULT_VARIABLE HAD_ERROR)

   message(STATUS "out: ${_out}")
   message(STATUS "err: ${_err}")
   message(STATUS "res: ${HAD_ERROR}")
   if(HAD_ERROR)
    message(FATAL_ERROR "Cannot checkout SCOREC repository!")
   endif()
endif()

ENDIF()

IF (DOWNLOAD_ALBANY)

#
# Get ALBANY
#
##########################################################################################################

if(NOT EXISTS "${CTEST_SOURCE_DIRECTORY}/Albany")
  EXECUTE_PROCESS(COMMAND "${CTEST_GIT_COMMAND}" 
    clone ${Albany_REPOSITORY_LOCATION} ${CTEST_SOURCE_DIRECTORY}/Albany
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    RESULT_VARIABLE HAD_ERROR)
  
   message(STATUS "out: ${_out}")
   message(STATUS "err: ${_err}")
   message(STATUS "res: ${HAD_ERROR}")
   if(HAD_ERROR)
	message(FATAL_ERROR "Cannot clone Albany repository!")
   endif()

endif()

ENDIF()

CTEST_START(${CTEST_TEST_TYPE})

# Read the build number from the TAG file
#
# I'm sure this is a CTest variable, but can't deduce it from Google at least
file(STRINGS "${CTEST_BINARY_DIRECTORY}/Testing/TAG" build_number LIMIT_COUNT 1)

IF(DOWNLOAD_TRILINOS)

#
# Update Trilinos
#
###########################################################################################################

SET_PROPERTY (GLOBAL PROPERTY SubProject Trilinos_CUVM)
SET_PROPERTY (GLOBAL PROPERTY Label Trilinos_CUVM)

set(CTEST_UPDATE_COMMAND "${CTEST_GIT_COMMAND}")
CTEST_UPDATE(SOURCE "${TRILINOS_HOME}" RETURN_VALUE count)
message("Found ${count} changed files")

IF(count LESS 0)
        message(FATAL_ERROR "Cannot update Trilinos!")
endif()

# Save a copy of the Trilinos update to post to the CDash site. Why can't this be done 
# in an easier way???

if(EXISTS "${CTEST_BINARY_DIRECTORY}/Testing/${build_number}/Update.xml")

  EXECUTE_PROCESS( COMMAND cp Update.xml Update_Trilinos.xml
                 WORKING_DIRECTORY ${CTEST_BINARY_DIRECTORY}/Testing/${build_number}
               )
endif()

# Get the SCOREC tools

set(CTEST_UPDATE_COMMAND "${CTEST_GIT_COMMAND}")
CTEST_UPDATE(SOURCE "${TRILINOS_HOME}/SCOREC" RETURN_VALUE count)
message("Found ${count} changed files")

IF(count LESS 0)
        message(FATAL_ERROR "Cannot update SCOREC tools!")
endif()

ENDIF()

IF(DOWNLOAD_ALBANY)

#
# Update Albany
#
##############################################################################################################

SET_PROPERTY (GLOBAL PROPERTY SubProject Albany_CUVM)
SET_PROPERTY (GLOBAL PROPERTY Label Albany_CUVM)

set(CTEST_UPDATE_COMMAND "${CTEST_GIT_COMMAND}")
CTEST_UPDATE(SOURCE "${CTEST_SOURCE_DIRECTORY}/Albany" RETURN_VALUE count)
message("Found ${count} changed files")

IF(count LESS 0)
        message(FATAL_ERROR "Cannot update Albany!")
endif()

ENDIF()

#
# Set the common Trilinos config options
#
#######################################################################################################################

SET_PROPERTY (GLOBAL PROPERTY SubProject Trilinos_CUVM)
SET_PROPERTY (GLOBAL PROPERTY Label Trilinos_CUVM)

SET(CONFIGURE_OPTIONS
  "-Wno-dev"
  "-DTrilinos_CONFIGURE_OPTIONS_FILE:FILEPATH=${TRILINOS_HOME}/sampleScripts/AlbanySettings.cmake"
  "-DTrilinos_EXTRA_REPOSITORIES:STRING=SCOREC"
  "-DTrilinos_ENABLE_SCOREC:BOOL=ON"
  "-DSCOREC_DISABLE_STRONG_WARNINGS:BOOL=ON"
  "-DCMAKE_BUILD_TYPE:STRING=NONE"
  "-DCMAKE_CXX_COMPILER:FILEPATH=${CTEST_SCRIPT_DIRECTORY}/nvcc_wrapper"
  "-DCMAKE_C_COMPILER:FILEPATH=mpicc"
  "-DCMAKE_Fortran_COMPILER:FILEPATH=mpifort"
  "-DCMAKE_CXX_FLAGS:STRING='-w -DNDEBUG'"
  "-DCMAKE_C_FLAGS:STRING='-O3 -w -DNDEBUG'"
  "-DCMAKE_Fortran_FLAGS:STRING='-O3 -w -DNDEBUG'"
  "-DTrilinos_ENABLE_EXPLICIT_INSTANTIATION:BOOL=OFF"
#
  "-DRythmos_ENABLE_DEBUG:BOOL=ON"
  "-DTrilinos_ENABLE_Kokkos:BOOL=ON"
  "-DPhalanx_KOKKOS_DEVICE_TYPE:STRING=CUDA"
  "-DPhalanx_INDEX_SIZE_TYPE:STRING=INT"
  "-DPhalanx_SHOW_DEPRECATED_WARNINGS:BOOL=OFF"
  "-DKokkos_ENABLE_Serial:BOOL=ON"
  "-DKokkos_ENABLE_OpenMP:BOOL=OFF"
  "-DKokkos_ENABLE_Pthread:BOOL=OFF"
  "-DKokkos_ENABLE_Cuda:BOOL=ON"
  "-DTPL_ENABLE_CUDA:BOOL=ON"
  "-DTPL_ENABLE_CUSPARSE:BOOL=ON"
  "-DKokkos_ENABLE_Cuda_UVM:BOOL=ON"
#
  "-DTPL_ENABLE_MPI:BOOL=ON"
  "-DMPI_BASE_DIR:PATH=${MPI_BASE_DIR}"
  "-DTPL_ENABLE_Pthread:BOOL=OFF"
#
  "-DTPL_ENABLE_Boost:BOOL=ON"
  "-DTPL_ENABLE_BoostLib:BOOL=ON"
  "-DTPL_ENABLE_BoostAlbLib:BOOL=ON"
  "-DBoost_INCLUDE_DIRS:PATH=${PREFIX_DIR}/include"
  "-DBoost_LIBRARY_DIRS:PATH=${PREFIX_DIR}/lib"
  "-DBoostLib_INCLUDE_DIRS:PATH=${PREFIX_DIR}/include"
  "-DBoostLib_LIBRARY_DIRS:PATH=${PREFIX_DIR}/lib"
  "-DBoostAlbLib_INCLUDE_DIRS:PATH=${PREFIX_DIR}/include"
  "-DBoostAlbLib_LIBRARY_DIRS:PATH=${PREFIX_DIR}/lib"
#
  "-DTPL_ENABLE_Netcdf:STRING=ON"
  "-DNetcdf_INCLUDE_DIRS:PATH=${PREFIX_DIR}/include"
  "-DNetcdf_LIBRARY_DIRS:PATH=${PREFIX_DIR}/lib"
#
  "-DTPL_ENABLE_HDF5:STRING=ON"
  "-DHDF5_INCLUDE_DIRS:PATH=${PREFIX_DIR}/include"
  "-DHDF5_LIBRARY_DIRS:PATH=${PREFIX_DIR}/lib"
#
  "-DTPL_ENABLE_Zlib:STRING=ON"
  "-DZlib_INCLUDE_DIRS:PATH=${PREFIX_DIR}/include"
  "-DZlib_LIBRARY_DIRS:PATH=${PREFIX_DIR}/lib"
#
  "-DTPL_ENABLE_BLAS:BOOL=ON"
  "-DTPL_ENABLE_LAPACK:BOOL=ON"
  "-DBLAS_LIBRARY_DIRS:FILEPATH=${INTEL_DIR}"
  "-DTPL_BLAS_LIBRARIES:STRING='-L${INTEL_DIR} -lmkl_intel_lp64 -lmkl_sequential -lmkl_core'"
  "-DLAPACK_LIBRARY_NAMES:STRING=''"
#
  "-DTPL_ENABLE_ParMETIS:STRING=ON"
  "-DParMETIS_INCLUDE_DIRS:PATH=${PREFIX_DIR}/include"
  "-DParMETIS_LIBRARY_DIRS:PATH=${PREFIX_DIR}/lib"
#
  "-DTPL_ENABLE_SuperLU:STRING=ON"
  "-DSuperLU_INCLUDE_DIRS:PATH=${PREFIX_DIR}/SuperLU_4.3/include"
  "-DSuperLU_LIBRARY_DIRS:PATH=${PREFIX_DIR}/SuperLU_4.3/lib"
#
  "-DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF"
  "-DTrilinos_VERBOSE_CONFIGURE:BOOL=OFF"
#
  "-DTPL_ENABLE_ParMETIS:STRING=ON"
  "-DParMETIS_INCLUDE_DIRS:PATH=${PREFIX_DIR}/include"
  "-DParMETIS_LIBRARY_DIRS:PATH=${PREFIX_DIR}/lib"
#
  "-DTrilinos_EXTRA_LINK_FLAGS='-L${PREFIX_DIR}/lib -lnetcdf -lhdf5_hl -lhdf5 -lz'"
  "-DCMAKE_INSTALL_PREFIX:PATH=${CTEST_BINARY_DIRECTORY}/TrilinosInstall"
#
  "-DTrilinos_ENABLE_Moertel:BOOL=OFF"
  "-DTrilinos_ENABLE_TriKota:BOOL=OFF"
  "-DSEACAS_ENABLE_SEACASSVDI:BOOL=OFF"
  "-DTrilinos_ENABLE_SEACASFastq:BOOL=OFF"
  "-DTrilinos_ENABLE_SEACASBlot:BOOL=OFF"
  "-DTrilinos_ENABLE_SEACASPLT:BOOL=OFF"
  "-DTrilinos_ENABLE_STK:BOOL=ON"
  "-DTrilinos_ENABLE_STKClassic:BOOL=OFF"
  "-DTrilinos_ENABLE_STKTopology:BOOL=ON"
  "-DTrilinos_ENABLE_STKMesh:BOOL=ON"
  "-DTrilinos_ENABLE_STKIO:BOOL=ON"
  "-DTrilinos_ENABLE_STKTransfer:BOOL=ON"
  "-DTPL_ENABLE_X11:BOOL=OFF"
  "-DTPL_ENABLE_Matio:BOOL=OFF"
  "-DTrilinos_ENABLE_ThreadPool:BOOL=OFF"
  "-DZoltan_ENABLE_ULONG_IDS:BOOL=OFF"
  "-DTeuchos_ENABLE_LONG_LONG_INT:BOOL=OFF"
  "-DTrilinos_ENABLE_Teko:BOOL=OFF"
  "-DTrilinos_ENABLE_MueLu:BOOL=OFF"
#
  "-DTrilinos_ENABLE_SEACAS:BOOL=ON"
  "-DTrilinos_ENABLE_SEACASIoss:BOOL=ON"
  "-DTrilinos_ENABLE_SEACASExodus:BOOL=ON"
  )

IF(BUILD_TRILINOS)

#
# Configure the Trilinos build
#
###############################################################################################################

if(NOT EXISTS "${CTEST_BINARY_DIRECTORY}/TriBuild")
  FILE(MAKE_DIRECTORY ${CTEST_BINARY_DIRECTORY}/TriBuild)
endif()

CTEST_CONFIGURE(
          BUILD "${CTEST_BINARY_DIRECTORY}/TriBuild"
          SOURCE "${TRILINOS_HOME}"
          OPTIONS "${CONFIGURE_OPTIONS}"
          RETURN_VALUE HAD_ERROR
          APPEND
)

if(HAD_ERROR)
	message(FATAL_ERROR "Cannot configure Trilinos build!")
endif()

# Save a copy of the Trilinos configure to post to the CDash site. Why can't this be done 
# in an easier way???

if(EXISTS "${CTEST_BINARY_DIRECTORY}/Testing/${build_number}/Configure.xml")

  EXECUTE_PROCESS( COMMAND cp Configure.xml Configure_Trilinos.xml
                 WORKING_DIRECTORY ${CTEST_BINARY_DIRECTORY}/Testing/${build_number}
               )
endif()

SET(CTEST_BUILD_TARGET install)

MESSAGE("\nBuilding target: '${CTEST_BUILD_TARGET}' ...\n")

CTEST_BUILD(
          BUILD "${CTEST_BINARY_DIRECTORY}/TriBuild"
          RETURN_VALUE  HAD_ERROR
          NUMBER_ERRORS  BUILD_LIBS_NUM_ERRORS
          APPEND
)

if(HAD_ERROR)
	message(FATAL_ERROR "Cannot build Trilinos!")
endif()

# Save a copy of the Trilinos build to post to the CDash site. Why can't this be done 
# in an easier way???

if(EXISTS "${CTEST_BINARY_DIRECTORY}/Testing/${build_number}/Build.xml")

  EXECUTE_PROCESS( COMMAND cp Build.xml Build_Trilinos.xml
                 WORKING_DIRECTORY ${CTEST_BINARY_DIRECTORY}/Testing/${build_number}
               )
endif()

ENDIF()

IF (BUILD_ALBANY)

# Configure the ALBANY build 
#
####################################################################################################################

SET_PROPERTY (GLOBAL PROPERTY SubProject Albany_CUVM)
SET_PROPERTY (GLOBAL PROPERTY Label Albany_CUVM)

SET(CONFIGURE_OPTIONS
  "-DALBANY_TRILINOS_DIR:PATH=${CTEST_BINARY_DIRECTORY}/TrilinosInstall"
  "-DENABLE_LCM:BOOL=ON"
  "-DENABLE_LCM_SPECULATIVE:BOOL=OFF"
  "-DENABLE_HYDRIDE:BOOL=OFF"
  "-DENABLE_SCOREC:BOOL=ON"
  "-DENABLE_SG_MP:BOOL=OFF"
  "-DENABLE_FELIX:BOOL=ON"
  "-DENABLE_AERAS:BOOL=OFF"
  "-DENABLE_QCAD:BOOL=OFF"
  "-DENABLE_MOR:BOOL=OFF"
  "-DENABLE_ATO:BOOL=ON"
  "-DENABLE_SEE:BOOL=ON"
  "-DENABLE_ASCR:BOOL=OFF"
  "-DENABLE_CHECK_FPE:BOOL=OFF"
  "-DENABLE_LAME:BOOL=OFF"
  "-DENABLE_ALBANY_EPETRA_EXE:BOOL=ON"
  "-DENABLE_KOKKOS_UNDER_DEVELOPMENT:BOOL=ON"
   )
 
if(NOT EXISTS "${CTEST_BINARY_DIRECTORY}/Albany")
  FILE(MAKE_DIRECTORY ${CTEST_BINARY_DIRECTORY}/Albany)
endif()

CTEST_CONFIGURE(
          BUILD "${CTEST_BINARY_DIRECTORY}/Albany"
          SOURCE "${CTEST_SOURCE_DIRECTORY}/Albany"
          OPTIONS "${CONFIGURE_OPTIONS}"
          RETURN_VALUE HAD_ERROR
          APPEND
)

if(HAD_ERROR)
	message(FATAL_ERROR "Cannot configure Albany build!")
endif()

#
# Build Albany
#
###################################################################################################################

SET(CTEST_BUILD_TARGET all)

MESSAGE("\nBuilding target: '${CTEST_BUILD_TARGET}' ...\n")

CTEST_BUILD(
          BUILD "${CTEST_BINARY_DIRECTORY}/Albany"
          RETURN_VALUE  HAD_ERROR
          TARGET "Albany"
          TARGET "AlbanyT"
          NUMBER_ERRORS  BUILD_LIBS_NUM_ERRORS
          APPEND
)

if(HAD_ERROR)
	message(FATAL_ERROR "Cannot build Albany!")
endif()

if(BUILD_LIBS_NUM_ERRORS GREATER 0)
    message(FATAL_ERROR "Encountered build errors in Albany build. Exiting!")
endif()

#
# Run Albany tests
#
##################################################################################################################

CTEST_TEST(
              BUILD "${CTEST_BINARY_DIRECTORY}/Albany"
#              PARALLEL_LEVEL "${CTEST_PARALLEL_LEVEL}"
#              INCLUDE_LABEL "^${TRIBITS_PACKAGE}$"
              INCLUDE_LABEL "CUDA_TEST"
              #NUMBER_FAILED  TEST_NUM_FAILED
)

ENDIF()


# Done!!!

