#pragma once
/*
 * Fix connector 
 */
#include "gwcConnector.h"
#include "fixCodec.h"

#include "SbfTcpConnection.hpp"
#include "sbfCacheFile.h"
#include "sbfMw.h"

#include "fixCodec.h"

#include <map>

using namespace std;
using namespace neueda;

class gwcFix;

class gwcFixTcpConnectionDelegate : public SbfTcpConnectionDelegate
{
    friend class gwcFix;
    
public:
    gwcFixTcpConnectionDelegate (gwcFix* gwc);

    virtual void onReady ();

    virtual void onError ();

    virtual size_t onRead (void* data, size_t size);

private:
    gwcFix* mGwc;
};

struct gwcFixSeqnums
{
    int64_t mInbound;
    int64_t mOutbound;
};

class gwcFix : public gwcConnector
{
    friend class gwcFixTcpConnectionDelegate;

    static const string FixHeartbeat;
    static const string FixTestRequest;
    static const string FixResendRequest;
    static const string FixReject;
    static const string FixSequenceReset;
    static const string FixLogout;
    static const string FixExecutionReport;
    static const string FixOrderCancelReject;
    static const string FixLogon;
    static const string FixNewOrderSingle;
    static const string FixBusinessMessageReject;
    static const string FixOrderCancelRequest;
    static const string FixOrderCancelReplaceRequest;
    
public:
    gwcFix (neueda::logger* log);
    virtual ~gwcFix ();

    virtual bool init (gwcSessionCallbacks* sessionCbs,
                       gwcMessageCallbacks* messageCbs, 
                       const neueda::properties& props);

    virtual bool start (bool reset);

    virtual bool stop ();

    virtual bool traderLogon (string& traderId, const cdr* msg = NULL);

    virtual bool sendOrder (gwcOrder& order);
    virtual bool sendOrder (cdr& order);    
    virtual bool sendCancel (cdr& cancel);
    virtual bool sendCancel (gwcOrder& cancel);
    virtual bool sendModify (cdr& modify);
    virtual bool sendModify (gwcOrder& modify);
    virtual bool sendMsg (cdr& msg);
    virtual bool sendRaw (void* data, size_t len);

protected:
    SbfTcpConnection*           mTcpConnection;
    gwcFixTcpConnectionDelegate mTcpConnectionDelegate;

    sbfCacheFile                mCacheFile;
    sbfCacheFileItem            mCacheItem;
    sbfMw                       mMw;
    sbfQueue                    mQueue;
    sbfThread                   mThread;

private:   

    // utility methods
    void reset ();
    void error (const string& err);
    void getTime (cdrDateTime& dt);
    void setHeader (cdr& d);
    bool mapOrderFields (gwcOrder& o);

    // handle state
    void onTcpConnectionReady ();
    void onTcpConnectionError ();
    size_t onTcpConnectionRead (void* data, size_t size);
    
    // handle messages
    void handleTcpMsg (cdr& msg);
    void handleLogoutMsg (int64_t seqno, cdr& msg);
    void handleTestRequestMsg (int64_t seqno, cdr& msg);
    void handleResendRequestMsg (int64_t seqno, cdr& msg);
    void handleSequenceResetMsg (int64_t seqno, cdr& msg);
    void handleExecutionReportMsg (int64_t seqno, cdr& msg); 
    void handleOrderCancelRejectMsg (int64_t seqno, cdr& msg); 
    void handleBusinessRejectMsg (int64_t seqno, cdr& msg);
    void handleRejectMsg (int64_t seqno, cdr& msg);

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
    string                  mBeginString;
    string                  mSenderCompID;
    string                  mTargetCompID;
    string                  mDataDictionary;
    int                     mHeartBtInt;
    bool                    mResetSeqNumFlag;
    bool                    mDispatching;
    sbfTimer                mHb;
    sbfTimer                mReconnectTimer;
    fixCodec                mCodec;
    bool                    mSeenHb;
    int                     mMissedHb;
    gwcFixSeqnums           mSeqnums;
};

