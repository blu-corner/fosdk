# Front Office SDK [![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=blu-corner_codec&metric=alert_status)](https://sonarcloud.io/dashboard?id=blu-corner_codec) [![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=blu-corner_fosdk&metric=alert_status)](https://sonarcloud.io/dashboard?id=blu-corner_fosdk)
SDK for building exchange connectivity applications in C/C++/Python/Java/C#

## Building

Requires cmake, default install location is build-directory/install

```bash
$ git submodule update --init
$ mkdir build
$ cd build
$ cmake -DTESTS=ON -DPYTHON=ON -DJAVA=ON -DCSHARP=ON ../
$ make
$ make install
```

### Testing

Currently you can run the test-millennium connector via:

```bash
$ export LD_LIBRARY_PATH=`pwd`/install/lib:$LD_LIBRARY_PATH
$ gdb --args ./install/bin/gwc-test
```

On mac you need to use lldb from the xcode tools
```bash
$ export LD_LIBRARY_PATH=`pwd`/install/lib:$LD_LIBRARY_PATH
$ /Applications/Xcode.app/Contents/Developer/usr/bin/lldb -- ./install/bin/gwc-cdr-test
```

### Bindings

Test Python bindings:

```bash
$ cd install/lib/python
$ PYTHONPATH=`pwd` LD_LIBRARY_PATH=`pwd`/../:$LD_LIBRARY_PATH python ../../../../src/bindings/python/example.py
```

Test Java bindings:

```bash
$ cd install/lib/java
$ sh build.sh
$ sh run.sh
```

Test C# bindings:

```bash
$ cd install/lib/csharp
$ sh build.sh
$ sh run.sh
```
