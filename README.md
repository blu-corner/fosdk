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
| millennium  | venue                | lse/oslo/turquoise/borsa/jse | Name of the millennium venue           |
|             | seqno_cache          | file name                    | File where sequence numbers are stored |
|             | real_time_host       | ip:port                      | Real time connection string            |
|             | recovery_host        | ip:port                      | Message recovery connection string     |
|             | enable_raw_messages  | True/False                   | Get raw binary messages in callbacks   |
|             |                      |                              |                                        |
| optiq       | host                 | ip:port                      | Connection string                      |
|             | partition            | Number                       | Matching Engine partition              |
|             | accessId             | Token                        | Exchange client access ID              |
|             | seqno_cache          | file name                    | File where sequence numbers are stored |
|             | enable_raw_messages  | True/False                   | Get raw binary messages in callbacks   |
|             |                      |                              |                                        |
| swx         | host                 | ip:port                      | Connection string                      | 
|             | seqno_cache          | file name                    | File where sequence numbers are stored |
|             | enable_raw_messages  | True/False                   | Get raw binary messages in callbacks   |
|             |                      |                              |                                        |
| eti         | venue                | xetra/eurex                  | Name of the eti venue                  |
|             | host                 | ip:port                      | Connection string                      |
|             | applMsgId_cache      | name                         | File where appl msg Ids are stored     |
|             | enable_raw_messages  | True/False                   | Get raw binary messages in callbacks   |

# Usage

## Common Types

The file gwcCommon.h defines the generic order types/sides/time in force (gwcOrderType/gwcSide/gwcTif) 
that can be used via the API gwcOrder interface. Within the connector it will map these generic fields to 
the correct exchange representation. If you do not wish to use this then the actual exchange representation 
can be set explicitly in the CDR object. 

### gwcSide

```cpp
typedef enum
{
    GWC_SIDE_BUY = '1',
    GWC_SIDE_SELL = '2',
    GWC_SIDE_BUY_MINUS = '3',
    GWC_SIDE_SELL_PLUS = '4',
    GWC_SIDE_SELL_SHORT = '5',
    GWC_SIDE_SELL_SHORT_EXEMPT = '6',
    GWC_SIDE_UNDISCLOSED = '7',
    GWC_SIDE_CROSS = '8',
    GWC_SIDE_CROSS_SHORT = '9',
    GWC_SIDE_CROSS_SHORT_EXEMPT = 'A',
    GWC_SIDE_AS_DEFINED = 'B',
    GWC_SIDE_OPPOSITE = 'C',
    GWC_SIDE_SUBSCRIBE = 'D',
    GWC_SIDE_REDEEM = 'E',
    GWC_SIDE_LEND = 'F',
    GWC_SIDE_BORROW = 'G',
    GWC_SIDE_SELL_UNDISCLOSED = 'H',
} gwcSide;
```

### gwcOrderType

```cpp
typedef enum
{
    GWC_ORDER_TYPE_MARKET = '1',
    GWC_ORDER_TYPE_LIMIT = '2',
    GWC_ORDER_TYPE_STOP = '3',
    GWC_ORDER_TYPE_STOP_LIMIT = '4',
    GWC_ORDER_TYPE_MARKET_ON_CLOSE = '5',
    GWC_ORDER_TYPE_WITH_OR_WITHOUT = '6',
    GWC_ORDER_TYPE_LIMIT_OR_BETTER = '7',
    GWC_ORDER_TYPE_LIMIT_WITH_OR_WITHOUT = '8',
    GWC_ORDER_TYPE_ON_BASIS = '9',
    GWC_ORDER_TYPE_ON_CLOSE = 'A',
    GWC_ORDER_TYPE_LIMIT_ON_CLOSE = 'B',
    GWC_ORDER_TYPE_FOREX = 'C',
    GWC_ORDER_TYPE_PREVIOUSLY_QUOTED = 'D',
    GWC_ORDER_TYPE_PREVIOUSLY_INDICATED = 'E',
    GWC_ORDER_TYPE_PEGGED = 'P',
} gwcOrderType;
```

### gwcTif

```cpp
typedef enum
{
    GWC_TIF_DAY = '0',
    GWC_TIF_GTC = '1',
    GWC_TIF_OPG = '2',
    GWC_TIF_IOC = '3',
    GWC_TIF_FOK = '4',
    GWC_TIF_GTX = '5',
    GWC_TIF_GTD = '6',
    GWC_TIF_ATC = '7',
    GWC_TIF_GTT = '8',
    GWC_TIF_CPX = '9',
    GWC_TIF_GFA = 'A',
    GWC_TIF_GFX = 'B',
    GWC_TIF_GFS = 'C',
} gwcTif;
```

## API walkthrough

The following outlines the high level steps required when using a venue from the FOSDK, please refer to  
[lse-cdr-example.cpp](./examples/lse-cdr-example.cpp)

First define your own delegates inherited from gwcSessionCallbacks and gwcMessageCallbacks these will define 
what your application wants to do on session events and exchange messages. Note that these classes are pure 
virtual so an implementation is needed for each pure virtual method, even if no action is performed in the methods.

```cpp
#include "gwcConnector.h"
#include "fields.h"

using namespace std;
using namespace neueda;

/* Session callbacks inherit from gwcSessionCallbacks */
class sessionCallbacks : public gwcSessionCallbacks
{
public:
    virtual void onConnected ()
    {
        mLog->info ("session logged on...");
    }

    virtual void onLoggingOn (cdr& msg)
    {

/* Message callbacks inherit from gwcMessageCallbacks */
class messageCallbacks : public gwcMessageCallbacks
{
public:
    virtual void onAdmin (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onAdmin msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onOrderAck (uint64_t seqno, const cdr& msg)
    {
```

In you main application create/config a logger object and create the configuration required, refer to 
configuration tables above.

```cpp
    properties p;
    p.setProperty ("lh.console.level", "debug");
    p.setProperty ("lh.console.color", "true");

    std::string errorMessage;
    bool ok = logService::get ().configure (p, errorMessage);
    if (not ok)
    {
        std::string e("failed to configure logger: " + errorMessage);
        errx (1, "%s", e.c_str ());
    }

    properties props (p, "gwc", "millennium", "sim");
    props.setProperty ("venue", "lse");
    props.setProperty ("real_time_host", "127.0.0.1:9899");
    props.setProperty ("recovery_host", "127.0.0.1:10000");

    logger* log = logService::getLogger ("MILLENIUM_TEST");
```

Create a connector using factory method gwcConnectorFactory::get(), this will automatically allocate and 
load the correct shared object.

```cpp
    gwcConnector* gwc = gwcConnectorFactory::get (log, "millennium", props);
```

Next call init() on the connector, passing the session and message callback objects and properties. This can 
fail is the props are incorrect so check return and look at error if applicable. 

```cpp
    if (!gwc->init (&sessionCbs, &messageCbs, props))
        errx (1, "failed to initialise connector...");
```

Calling start will being the session logic for the exchange, mostly this will start by making a TCP connection 
and then sending the initial logon request.

```cpp
    if (!gwc->start (false))
        errx (1, "failed to initialise connector...");
```

A blocking call waitForLogon () is provided to block the main thread until the connector finishes its logon 
process and is ready to accept orders to pass to the exchange.

```cpp
    gwc->waitForLogon ();
```

Note that during the start process the session callback onLoggingOn will be called, if the connector needs a 
username or password these can be added to the CDR at this point to be used to complete logon.

```cpp
     virtual void onLoggingOn (cdr& msg)
    {
        mLog->info ("session logging on...");

        /* set username and password */
        msg.setString (UserName, mUsername);
        msg.setString (Password, mPassword);
    }
```

Once the session is logged on any send method can be called and events and exchange messages will be 
dispatched to the user callbacks.

```cpp
    /* send order */
    gwcOrder order;
    order.setPrice (1234.45);
    order.setQty (1000);
    order.setTif (GWC_TIF_DAY);
    order.setSide (GWC_SIDE_BUY);
    order.setOrderType (GWC_ORDER_TYPE_LIMIT);
    order.setString (ClientOrderID, "myorder");
    order.setInteger (InstrumentID, 133215); // VOD.L
    order.setInteger (AutoCancel, 1);

    order.setString (TraderID, "TX1");
    order.setString (Account, "account");
    order.setInteger (ClearingAccount, 1);
    order.setInteger (FXMiFIDFlags, 0);
    order.setInteger (PartyRoleQualifiers, 0);
    order.setInteger (ExpireDateTime, 0);
    order.setInteger (DisplayQty, 0);
    order.setInteger (Capacity, 1);
    order.setInteger (OrderSubType, 0);
    order.setInteger (Anonymity, 0);
    order.setDouble (StopPrice, 0.0);
    order.setInteger (PassiveOnlyOrder, 0);
    order.setInteger (ClientID, 1234);
    order.setInteger (InvestmentDecisionMaker, 0);
    order.setInteger (MinimumQuantity, 0);
    order.setInteger (ExecutingTrader, 7676);

    if (!gwc->sendOrder (order))
        errx  (1, "failed to send order myorder...");
```

Finally stop () can be called to disconnect from the session.

```cpp
    gwc->stop ();
```

# Examples

## CDR example

Example Xetra connector using CDR interface for sending orders/modifies/cancels [xetra-cdr-example.cpp](./examples/xetra-cdr-example.cpp)

## Raw packets example

Eample Lse connector using Raw packets interface for sending orders/modifies/cancels [lse-raw-example.cpp](./examples/lse-raw-example.cpp)
