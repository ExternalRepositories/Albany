#!/bin/bash

#source $1 

TTT=`grep "(Failed)" /home/ikalash/nightlyCDash/nightly_log_64bitT.txt -c`
TTTT=`grep "(Not Run)" /home/ikalash/nightlyCDash/nightly_log_64bitT.txt -c`
TTTTT=`grep "Timeouts" /home/ikalash/nightlyCDash/nightly_log_64bitT.txt -c`

#/bin/mail -s "Albany ($ALBANY_BRANCH): $TTT" "albany-regression@software.sandia.gov" < $ALBOUTDIR/albany_runtests.out
/bin/mail -s "Albany (master, 64bit, AlbanyT only): $TTT tests failed, $TTTT tests not run, $TTTTT timeouts" "ikalash@sandia.gov" < /home/ikalash/nightlyCDash/results_64bitT
/bin/mail -s "Albany (master, 64bit, AlbanyT only): $TTT tests failed, $TTTT tests not run, $TTTTT timeouts" "agsalin@sandia.gov" < /home/ikalash/nightlyCDash/results_64bitT
/bin/mail -s "Albany (master, 64bit, AlbanyT only): $TTT tests failed, $TTTT tests not run, $TTTTT timeouts" "gahanse@sandia.gov" < /home/ikalash/nightlyCDash/results_64bitT
/bin/mail -s "Albany (master, 64bit, AlbanyT only): $TTT tests failed, $TTTT tests not run, $TTTTT timeouts" "onguba@sandia.gov" < /home/ikalash/nightlyCDash/results_64bitT
