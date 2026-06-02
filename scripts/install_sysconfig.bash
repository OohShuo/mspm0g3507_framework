#!/bin/bash

INSTALL_SCRIPT=./scripts/sysconfig-1.27.1_4634-setup.run
INSTALL_DIR=$(pwd)/tools/sysconfig

$INSTALL_SCRIPT --prefix $INSTALL_DIR --mode unattended --unattendedmodeui none

rm -r $INSTALL_DIR/install_logs $INSTALL_DIR/tests

rm "${INSTALL_DIR}/TI sysconfig.desktop" $INSTALL_DIR/uninstall \
    $INSTALL_DIR/uninstall.dat $INSTALL_DIR/update.ini
