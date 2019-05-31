#pragma once
/*
 * Optiq connector 
 */
#include "gwcConnector.h"
#include "optiqCodec.h"

#include "SbfTcpConnection.hpp"
#include "sbfCacheFile.h"
#include "sbfMw.h"

#include "optiqCodec.h"

#include <map>

using namespace std;
using namespace neueda;

class gwcOptiq;

class gwcOptiqTcpConnectionDelegate : public SbfTcpConnectionDelegate
{
    friend class gwcOptiq;
    
public:
    gwcOptiqTcpConnectionDelegate (gwcOptiq* gwc);

    virtual void onReady ();

    virtual void onError ();

    virtual size_t onRead (void* data, size_t size);

private:
    gwcOptiq* mGwc;
};

struct gwcOptiqSeqnums
{
    int64_t mInbound;
    int64_t mOutbound;
};

class gwcOptiq : public gwcConnector
{
    friend class gwcOptiqTcpConnectionDelegate;
    
public:
    gwcOptiq (neueda::logger* log);
    virtual ~gwcOptiq ();

    virtual bool init (gwcSessionCallbacks* sessionCbs,
                       gwcMessageCallbacks* messageCbs, 
                       const neueda::properties& props);

    virtual bool start (bool reset);

    virtual bool stop ();

    virtual bool traderLogon (string& traderId, const cdr* msg = NULL)
    {
        /* no trader logon */
        return false;
    }

    virtual bool sendOrder (gwcOrder& order);
    virtual bool sendOrder (cdr& order);    

    virtual bool sendCancel (gwcOrder& cancel);
    virtual bool sendCancel (cdr& cancel);

    virtual bool sendModify (gwcOrder& modify);
    virtual bool sendModify (cdr& modify);

    virtual bool sendMsg (cdr& msg);
    virtual bool sendRaw (void* data, size_t len);

protected:
    SbfTcpConnection*             mTcpConnection;
    gwcOptiqTcpConnectionDelegate mTcpConnectionDelegate;

    sbfCacheFile                  mCacheFile;
    sbfCacheFileItem              mCacheItem;
    sbfMw                         mMw;
    sbfQueue                      mQueue;
    sbfThread                     mThread;

private:   
    // utility methods
    void reset ();
    void error (const string& err);
    bool mapOrderFields (gwcOrder& order);

    // handle state
    void onTcpConnectionReady ();
    void onTcpConnectionError ();
    size_t onTcpConnectionRead (void* data, size_t size);
    
    // handle messages
    void handleTcpMsg (cdr& msg);
    void handleLogoffResponse (cdr& msg);
    void handleTestRequestMsg (cdr& msg);
    void handleTechnicalRejectMsg (cdr& msg);
    void handleAckMsg (int64_t seqno, cdr& msg);
    void handleExecutionMsg (int64_t seqno, cdr& msg); 
    void handleKillMsg (int64_t seqno, cdr& msg);
    void handleRejectMsg (int64_t seqno, cdr& msg);

    static bool isSessionMessage (uint16_t templateId);

    static void* dispatchCb (void* closure);
    static sbfError cacheFileItemCb (sbfCacheFile file, 
                                     sbfCacheFileItem item, 
                                     void* itemData, 
                                     size_t itemSize, 
                                     void* closure);
    static void onHbTimeout (sbfTimer timer, void* closure);
    static void onReconnect (sbfTimer timer, void* closure);

    // members 
    sbfTcpConnectionAddress mGwHost;
    int64_t                 mAccessId;
    int64_t                 mPartition;
    bool                    mDispatching;
    sbfTimer                mHb;
    sbfTimer                mReconnectTimer;
    optiqCodec              mCodec;
    bool                    mSeenHb;
    int                     mMissedHb;
    gwcOptiqSeqnums         mSeqnums;
};

