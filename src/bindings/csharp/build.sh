#!/usr/bin/env bash
set -e

LD_LILBRARY_PATH=`pwd`/..:`pwd`:$LD_LIBRARY_PATH mcs -out:test.exe -reference:PropertiesBindings.dll -reference:LoggerBindings.dll -reference:CdrBindings.dll -reference:CodecsBindings.dll -reference:FosdkBindings.dll example.cs
