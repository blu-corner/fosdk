#!/usr/bin/env bash
set -e

LD_LIBRARY_PATH=`pwd`/../:$LD_LIBRARY_PATH java \
               -cp `pwd`/CdrJNI.jar:`pwd`/CodecJNI.jar:`pwd`/LogJNI.jar:`pwd`/ConfigJNI.jar:`pwd`/FosdkJNI.jar:`pwd` \
               example
