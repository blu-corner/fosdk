#pragma once
/*
 * TCP SoupBin connector abstract base class
 */
#include "gwcConnector.h"

#include "SbfTcpConnection.hpp"
#include "sbfCacheFile.h"
#include "sbfMw.h"
#include "codec.h"

#include <ctime>
#include <map>

using namespace std;
using namespace neueda;

struct gwcSoupBinSeqNum
{
    uint32_t mSeqno;
    char mSession[10];
};

struct gwcSoupBinCacheItem
{
    sbfCacheFileItem mItem;
    gwcSoupBinSeqNum mData;
};

class gwcSoupBin;
class gwcSoupBinConnectionDelegate: public SbfTcpConnectionDelegate
{
    friend class gwcSoupBin;
    
public:
    gwcSoupBinConnectionDelegate (gwcSoupBin* gwc);

    virtual void onReady ();

    virtual void onError ();

    virtual size_t onRead (void* data, size_t size);

private:
    gwcSoupBin* mGwc;
};

class gwcSoupBin : public gwcConnector
{
    friend class gwcSoupBinConnectionDelegate;
    
public:    
    gwcSoupBin (neueda::logger* log);
    
    virtual ~gwcSoupBin ();

    virtual bool init (gwcSessionCallbacks* sessionCbs,
                       gwcMessageCallbacks* messageCbs, 
                       const neueda::properties& props);

    virtual bool start (bool reset);

    virtual bool stop ();

    virtual bool traderLogon (const cdr* msg) { return false; }
    
    virtual bool sendRaw (void* data, size_t len);
    
protected:
    SbfTcpConnection*            mConnection;
    gwcSoupBinConnectionDelegate mConnectionDelegate;

    sbfCacheFile          mCacheFile;
    sbfMw                 mMw;
    sbfQueue              mQueue;
    sbfThread             mThread;
    
    string                mSession;
    uint32_t              mSequenceNumber;
    
    struct gwcSoupBinCacheItem* mCacheItem;

    bool isSessionMessage (char type) const;

    // allows to override handling behaviour
    virtual void handleRealTimeMsg (cdr& msg);
    virtual void sendHeartBeat ();
    virtual void handleSessionMessge (cdr& msg);
    virtual void updateSeqno (string& session, uint32_t seqno);

    // veneue specific
    virtual neueda::codec& getCodec () = 0;

    // handle messages
    virtual void handleSequencedMessage (cdr& msg) = 0;
    virtual void handleUnsequencedMessage (cdr& msg) = 0;

private:
    void reset ();
    void resetHbTimer ();
    void error (const string& err);

    // handle state
    void onConnectionReady ();
    void onConnectionError ();
    size_t onConnectionRead (void* data, size_t size);

    static void* dispatchCb (void* closure);
    static sbfError cacheFileItemCb (sbfCacheFile file, 
                                     sbfCacheFileItem item, 
                                     void* itemData, 
                                     size_t itemSize, 
                                     void* closure);
    static void onHbTimeout (sbfTimer timer, void* closure);
    static void onReconnect (sbfTimer timer, void* closure);

    sbfTcpConnectionAddress mHost;

    bool        mDispatching;
    sbfTimer    mHb;
    sbfTimer    mReconnectTimer;
    bool        mSeenMessageWithinHbInterval;
};

