#!/usr/bin/env bash
set -ex

INSTALL_DIR=../../../build/install

rm -vf millennium.seqno.cache
LD_LIBRARY_PATH=${INSTALL_DIR}/lib:$LD_LIBRARY_PATH ./lse-test
