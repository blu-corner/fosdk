#pragma once
/*
 * Xetra connector 
 */
#include "gwcConnector.h"
#include "xetraCodec.h"

#include "SbfTcpConnection.hpp"
#include "sbfCacheFile.h"
#include "sbfMw.h"

#include "xetraCodec.h"

#include <map>

using namespace std;
using namespace neueda;

class gwcXetra;

class gwcXetraTcpConnectionDelegate : public SbfTcpConnectionDelegate
{
    friend class gwcXetra;
    
public:
    gwcXetraTcpConnectionDelegate (gwcXetra* gwc);

    virtual void onReady ();

    virtual void onError ();

    virtual size_t onRead (void* data, size_t size);

private:
    gwcXetra* mGwc;
};

class gwcXetra : public gwcConnector
{
    friend class gwcXetraTcpConnectionDelegate;
    
public:
    gwcXetra (neueda::logger* log);
    virtual ~gwcXetra ();

    virtual bool init (gwcSessionCallbacks* sessionCbs,
                       gwcMessageCallbacks* messageCbs, 
                       const neueda::properties& props);

    virtual bool start (bool reset);

    virtual bool stop ();

    virtual bool traderLogon (string& traderId, const cdr* msg = NULL); 

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
    gwcXetraTcpConnectionDelegate mTcpConnectionDelegate;

    sbfCacheFile                  mCacheFile;
    sbfCacheFileItem              mCacheItem;
    sbfMw                         mMw;
    sbfQueue                      mQueue;
    sbfThread                     mThread;

private:   
    // utility methods
    void reset ();
    void error (const string& err);
    void sendRetransRequest ();
    void updateApplMsgId (string& sMsgId);
    bool mapOrderFields (gwcOrder& gwc);

    // handle state
    void onTcpConnectionReady ();
    void onTcpConnectionError ();
    size_t onTcpConnectionRead (void* data, size_t size);
    
    // handle messages
    void handleTcpMsg (cdr& msg);
    void handleMsg (cdr& msg);
    void handleReject (cdr& msg);
    void handleRetransMeResponse (cdr& msg);
    void handleTraderLogon (cdr& msg);
    void handleLogoffResponse (cdr& msg);
    void handleExecutionMsg (cdr& msg); 
    void handleOrderCancelRejectMsg (cdr& msg);


    static void* dispatchCb (void* closure);
    static sbfError cacheFileItemCb (sbfCacheFile file, 
                                     sbfCacheFileItem item, 
                                     void* itemData, 
                                     size_t itemSize, 
                                     void* closure);
    static void onHbTimeout (sbfTimer timer, void* closure);
    static void onReconnect (sbfTimer timer, void* closure);

    // members 
    sbfTcpConnectionAddress mTcpHost;
    bool                    mDispatching;
    sbfTimer                mHb;
    sbfTimer                mReconnectTimer;
    xetraCodec              mCodec;
    bool                    mSeenHb;
    int                     mMissedHb;
    uint64_t                mOutboundSeqNo;
    int64_t                 mPartition;
    char                    mLastApplMsgId[16];
    char                    mCurrentRecoveryEnd[16];
    int64_t                 mRecoveryMsgCnt;
};

