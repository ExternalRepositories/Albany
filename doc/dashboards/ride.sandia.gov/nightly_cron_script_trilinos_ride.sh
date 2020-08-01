#!/bin/csh

BASE_DIR=/home/projects/albany/ride
cd $BASE_DIR

unset http_proxy
unset https_proxy

LOG_FILE=$BASE_DIR/nightly_log_rideTrilinos.txt

source clean-up.sh
source convert-cmake-to-cdash.sh  
source create-new-cdash-cmake-script.sh

eval "env  TEST_DIRECTORY=$BASE_DIR SCRIPT_DIRECTORY=$BASE_DIR ctest -VV -S $BASE_DIR/ctest_nightly_trilinos.cmake" > $LOG_FILE 2>&1

