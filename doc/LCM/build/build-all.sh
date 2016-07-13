#!/bin/bash

update_wiki () {
    cd "$LCM_DIR"
    STATUS_LOG="$PACKAGE-$ARCH-$TOOL_CHAIN-$BUILD_TYPE-status.log"
    if [[ -f "$STATUS_LOG" && "$WIKI"=="1" ]]; then
	SRC="Albany/doc/LCM/test/$WIKI_TEMPLATE"
	DEST="$LCM_DIR/Albany.wiki/$WIKI_TEMPLATE"
	cp -p "$SRC" "$DEST"
	cd "$LCM_DIR/Trilinos"
	TRILINOS_TAG=`git rev-parse HEAD`
	sed -i -e "s|ttag|$TRILINOS_TAG|g;" "$DEST"
	cd "$LCM_DIR/Albany"
	ALBANY_TAG=`git rev-parse HEAD`
	sed -i -e "s|atag|$ALBANY_TAG|g;" "$DEST"
	MSG="Update latest known good commits"
	cd "$LCM_DIR/Albany.wiki"
	git add "$DEST"
	git commit -m "$MSG"
	git push
	cd "$LCM_DIR"
    fi
}

source ./env-all.sh

cd "$LCM_DIR"

case "$SCRIPT_NAME" in
    build-all.sh)
	;&
    config-all.sh)
	;&
    clean-all.sh)
	;&
    test-all.sh)
	;&
    mail-all.sh)
	;&
    clean-config-all.sh)
	;&
    clean-config-build-all.sh)
	;&
    clean-config-build-test-all.sh)
	;&
    clean-config-build-test-mail-all.sh)
	;&
    config-build-all.sh)
	;&
    config-build-test-all.sh)
	;&
    config-build-test-mail-all.sh)
	;&
    build-test-all.sh)
	;&
    build-test-mail-all.sh)
	;&
    test-mail-all.sh)
	COMMAND="$LCM_DIR/${SCRIPT_NAME%-*}.sh"
	;;
    *)
	echo "Unrecognized script name"
	exit 1
	;;
esac
WIKI_TEMPLATE="LCM-Status:-Last-known-commits-that-work.md"

# Use different variable names for loop counters so they do not
# conflict with the variables defined by the module command.
module purge
for P in $PACKAGES; do
    for A in $ARCHES; do
        for TC in $TOOL_CHAINS; do
            for BT in $BUILD_TYPES; do
                MODULE="$A"-"$TC"-"$BT"
                echo "MODULE: $MODULE"
                module load "$MODULE"
                "$COMMAND" "$P" "$NUM_PROCS"
                # Update wiki after compiling Albany with gcc debug only.
                case "$PACKAGE" in
                    albany)
	                case "$ARCH" in
	                    serial)
		                case "$BUILD_TYPE" in
		                    debug)
			                case "$TOOL_CHAIN" in
			                    gcc)
				                update_wiki
				                ;;
			                    clang)
				                ;;
			                    intel)
				                ;;
			                    pgi)
				                ;;
			                    *)
				                echo "Unrecognized tool chain option"
				                exit 1
				                ;;
			                esac
			                ;;
		                    release)
			                ;;
		                    *)
			                echo "Unrecognized build type option"
			                exit 1
			                ;;
		                esac
		                ;;
	                    openmp)
		                ;;
	                    cuda)
		                ;;
	                    *)
		                echo "Unrecongnized architecture option"
		                exit 1
		                ;;
	                esac
	                ;;
                    trilinos)
	                ;;
                    *)
	                echo "Unrecognized package option"
	                exit 1
	                ;;
                esac
                module purge
            done
        done
    done
done

cd "$LCM_DIR"
