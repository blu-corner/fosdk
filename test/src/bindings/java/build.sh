#!/usr/bin/env bash
set -e

javac \
    -cp `pwd`/CdrJNI.jar:`pwd`/CodecJNI.jar:`pwd`/LoggerJNI.jar:`pwd`/PropertiesJNI.jar:`pwd`/FosdkJNI.jar:`pwd` \
    example.java
