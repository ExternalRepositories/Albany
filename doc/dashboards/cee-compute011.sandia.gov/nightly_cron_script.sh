#!/bin/sh

# Crontab entry
#
# Run at midnight every day
#
# 00 00 * * * /ascldap/users/gahanse/Codes/Albany/doc/dashboards/cee-compute011.sandia.gov/nightly_cron_script.sh

cd /projects/AppComp/nightly/cee-compute011

export LIBRARY_PATH=/sierra/sntools/SDK/compilers/intel/composer_xe_2015.1.133/mkl/lib/intel64:/sierra/sntools/SDK/compilers/intel/composerxe-2011.13.367/tbb/lib/intel64/cc4.1.0_libc2.4_kernel2.6.16.21
export GCC4HOME=/sierra/sntools/SDK/compilers/gcc/4.8.2-RHEL6/bin
export SIERRA_MKL_LIB_PATH=/sierra/sntools/SDK/compilers/intel/composer_xe_2015.1.133/mkl/lib/intel64
export SIERRA_TBB_LIB_PATH=/sierra/sntools/SDK/compilers/intel/composerxe-2011.13.367/tbb/lib/intel64/cc4.1.0_libc2.4_kernel2.6.16.21

export PATH=/projects/albany/bin:/projects/albany/trilinos/MPI_REL/bin:/projects/sierra/linux_rh6/install/utilities/valgrind/3.10.1/bin:/projects/sierra/linux_rh6/install/git/2.0.0/bin:/projects/sierra/linux_rh6/install/git/bin:/projects/viz/CEI/bin:/projects/viz/paraview/bin:/sierra/sntools/SDK/compilers/clang/3.5-RHEL6/bin:/sierra/sntools/SDK/compilers/clang/3.5-RHEL6/bin/scan-build:/sierra/sntools/SDK/compilers/clang/3.5-RHEL6/bin/scan-view:/sierra/sntools/SDK/mpi/openmpi/1.6.4-gcc-4.8.2-RHEL6/bin:/projects/sierra/linux_rh6/install/SNTOOLS_dir/master/sntools/job_scripts/linux_rh6/openmpi:/sierra/sntools/SDK/compilers/gcc/4.8.2-RHEL6/bin:/projects/sierra/linux_rh6/install/Python/2.7/bin:/projects/sierra/linux_rh6/install/SNTOOLS_dir/master/sntools/engine:/projects/sierra/linux_rh6/install/SNTOOLS_dir/master/contrib/bin:/projects/sierra/linux_rh6/install/SNTOOLS_dir/master/contrib:/usr/bin:/bin:/sbin:/usr/sbin:/usr/lib64/qt-3.3/bin:/usr/local/bin:/bin:/usr/bin:/usr/local/sbin:/usr/sbin:/sbin

export LD_LIBRARY_PATH=/sierra/sntools/SDK/compilers/intel/composer_xe_2015.1.133/mkl/lib/intel64:/sierra/sntools/SDK/compilers/clang/3.5-RHEL6/lib:/sierra/sntools/SDK/mpi/openmpi/1.6.4-gcc-4.8.2-RHEL6/lib:/sierra/sntools/SDK/compilers/intel/composerxe-2011.13.367/tbb/lib/intel64/cc4.1.0_libc2.4_kernel2.6.16.21:/sierra/sntools/SDK/compilers/gcc/4.8.2-RHEL6/lib64:/sierra/sntools/SDK/compilers/gcc/4.8.2-RHEL6/lib:/projects/albany/clang/lib

now=$(date +"%m_%d_%Y-%H_%M")
#LOG_FILE=/projects/AppComp/nightly/cee-compute011/nightly_$now
LOG_FILE=/projects/AppComp/nightly/cee-compute011/nightly_log.txt

eval "env  TEST_DIRECTORY=/projects/AppComp/nightly/cee-compute011 SCRIPT_DIRECTORY=/ascldap/users/gahanse/Codes/Albany/doc/dashboards/cee-compute011.sandia.gov /projects/albany/bin/ctest -VV -S /projects/AppComp/nightly/cee-compute011/ctest_nightly.cmake" > $LOG_FILE 2>&1


