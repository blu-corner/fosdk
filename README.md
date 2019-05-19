# Front Office SDK [![Build Status](https://travis-ci.com/blu-corner/fosdk.svg?branch=master)](https://travis-ci.com/blu-corner/fosdk) [![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=blu-corner_fosdk&metric=alert_status)](https://sonarcloud.io/dashboard?id=blu-corner_fosdk)
SDK for building exchange connectivity applications in C/C++/Python/Java/C#

- [Overview](#overview)
    - [Introduction](#introduction)
    - [Getting Started](#getting-started)
    - [Dependencies](#dependencies)
    - [Example Usage](#example-usage)
- [Architecture](#architecture)
- [Supported Venues](#supported-venues)
- [Configuration](#configuration)
- [Usgae](#usage)
    - [Common Types](#common-types)
    - [API walkthrough](#api-walkthough)
- [Examples](#examples)
    - [CDR example](#cdr-example)
    - [Raw packets example](#raw-example)


# Overview

## Introduction

The Front Office SDK (FOSDK) is a collection of order entry connectors and software modules designed 
to facilitate communication with a number of European cash equity venues. It provides an abstracted 
interface via the gwcConnector class to provide generic interfaces for sending orders/cancels or modifies. 

The core is written in C++ with C#/Java and Python support provided through SWIG bindings.

## Getting Started

```bash
$ git submodule update --init
$ mkdir build
$ cd build
$ cmake -DTESTS=ON ../
$ make
$ make install
```

Language bindings can be enabled by passing -DJAVA=on, -DPYTHON=on to cmake. It is possible to build all 
bindings in the same language.

The examples can be built by passing –DEXAMPLES=on

## Dependencies

The only external dependency is SWIG, and is only required when building the
Java, C# or Python bindings. For information on installing SWIG please visit the
[SWIG website](http://www.swig.org). All other dependencies are managed through
git submodules.

## Example usage

Examples can be found in the example directory and will present examples of using both the CDR and raw 
interfaces as appropriate.

# Architecture

The connector is responsible for all session management within the venue, such as logon, logoff, heartbeating, 
sequence number management, and gap recovery.

To send messages a CDR is passed to the API and encoded using the appropriate codec. 
(see https://github.com/blu-corner/cdr for more information on the Common Data Representation)

Sequence number management is maintained via the sbfCache file mechanism, which provides file persistence 
if a process is restarted.

When the connector is started a dispatch thread to notifies the user of new exchange messages or events. 
Two callback delegate classes are used to interface with the client gwcSessionCallbacks for session callbacks 
such as logon/error callbacks and gwcMessageCallbacks for each type of message the exchange sends back. 
For session level messages an onAdmin callback is is called. Application level messages will trigger an appropriate 
callback, if the connector can’t determine a specific message callback to use then the generic onMsg callback will be 
invoked. Typically messages passed to onMsg will not be related to the order life cycle.

The connectors are thread safe, orders/cancels/modifies can be sent form any thread including the dispatch thread.

# Supported Venues

- Millennium
    * Johannesburg
    * Borsa Italiana
    * LSE
    * Turqoise
    * Olso
- Deutsche Borse Xetra
- SIX group SWXOTI
- Euronext Optiq
- FIX

# Configuration

Each connector needs some configuration, such as host IP address, this config is passed into the 
connector using the properties class. The following table summarizes the required config for each class of 
connector

| GWC name    | Property             | Valid values                 | Description                            |
|:------------|:--------------------:| :---------------------------:|:--------------------------------------:|
! millennium  | venue                | lse/oslo/turquoise/borsa/jse | Name of the millennium venue           |
|             | seqno_cache          | file name                    | File where sequence numbers are stored |
|             | real_time_host       | <ip>:<port>                  | Real time connection string            |
|             | recovery_host        | <ip>:<port>                  | Message recovery connection string     |
|             | enable_raw_messages  | True/False                   | Get raw binary messages in callbacks   |
|             |                      |                              |                                        |
| optiq       | host                 | <ip>:<port>                  | Connection string                      |
|             | partition            | Number                       | Matching Engine partition              |
|             | accessId             | Token                        | Exchange client access ID              |
|             | seqno_cache          | file name                    | File where sequence numbers are stored |
|             | enable_raw_messages  | True/False                   | Get raw binary messages in callbacks   |
|             |                      |                              |                                        |
| swx         | host                 | <ip>:<port>                  | Connection string                      | 
|             | seqno_cache          | file name                    | File where sequence numbers are stored |
|             | enable_raw_messages  | True/False                   | Get raw binary messages in callbacks   |
|             |                      |                              |                                        |
| xetra       | host                 | <ip>:<port>                  | Connection string                      |
|             | partition            | Number                       | Matching Engine partition              |
|             | accessId             | Token                        | Exchange cleint access ID              |
|             | applMsgId_cache file | name                         | File where appl msg Ids are stored     |
|             | enable_raw_messages  | True/False                   | Get raw binary messages in callbacks   |

# Usage

## Common Types

The file gwcCommon.h defines the generic order types/sides/time in force (gwcOrderType/gwcSide/gwcTif) 
that can be used via the API gwcOrder interface. Within the connector it will map these generic fields to 
the correct exchange representation. If you do not wish to use this then the actual exchange representation 
can be set explicitly in the CDR object. 

## API walkthrough

The following outlines the high level steps required when using a venue from the FOSDK, please refer to  
lse-cdr-example.cpp.

First define your own delegates inherited from gwcSessionCallbacks and gwcMessageCallbacks these will define 
what your application wants to do on session events and exchange messages. Note that these classes are pure 
virtual so an implementation is needed for each pure virtual method, even if no action is performed in the methods.

In you main application create/config a logger object and create the configuration required, refer to 
configuration tables above.

Create a connector using factory method gwcConnectorFactory::get(), this will automatically allocate and 
load the correct shared object.

Next call init() on the connector, passing the session and message callback objects and properties. This can 
fail is the props are incorrect so check return and look at error if applicable. 

Calling start will being the session logic for the exchange, mostly this will start by making a TCP connection 
and then sending the initial logon request.

A blocking call waitForLogon () is provided to block the main thread until the connector finishes its logon 
process and is ready to accept orders to pass to the exchange.

Note that during the start process the session callback onLoggingOn will be called, if the connector needs a 
username or password these can be added to the CDR at this point to be used to complete logon.

Once the session is logged on any send method can be called and events and exchange messages will be 
dispatched to the user callbacks.

Finally stop () can be called to disconnect from the session.

# Examples

## CDR example

## Raw packets example

