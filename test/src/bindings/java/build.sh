#!/usr/bin/env bash
set -e

javac \
    -cp `pwd`/CdrJNI.jar:`pwd`/CodecJNI.jar:`pwd`/LogJNI.jar:`pwd`/ConfigJNI.jar:`pwd`/FosdkJNI.jar:`pwd` \
    example.java
