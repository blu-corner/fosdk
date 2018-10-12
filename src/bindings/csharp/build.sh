#!/usr/bin/env bash
set -e

mcs -out:test.exe -reference:Config.dll -reference:Log.dll -reference:Cdr.dll -reference:Codec.dll -reference:Fosdk.dll example.cs
