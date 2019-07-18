#pragma once
/*
 * Eti connector 
 */
#include "gwcConnector.h"
#include "xetraCodec.h"
#include "eurexCodec.h"

#include "SbfTcpConnection.hpp"
#include "sbfCacheFile.h"
#include "sbfMw.h"

#include <map>

using namespace std;
using namespace neueda;

struct gwcXetraSeqNum
{
    uint64_t mParitionId;
    uint64_t mSeqno;    
};

struct gwcXetraCacheItem
{
    sbfCacheFileItem    mItem;
    gwcXetraSeqNum mData;
};

template <typename CodecT> class gwcEti;
template <typename CodecT>
class gwcEtiTcpConnectionDelegate : public SbfTcpConnectionDelegate
{
    friend class gwcEti<CodecT>;
    
public:
    gwcEtiTcpConnectionDelegate (gwcEti<CodecT>* gwc);

    virtual void onReady ();

    virtual void onError ();

    virtual size_t onRead (void* data, size_t size);

private:
    gwcEti<CodecT>* mGwc;
};

template <typename CodecT>
class gwcEti : public gwcConnector
{
    friend class gwcEtiTcpConnectionDelegate<CodecT>;
    
public:
    typedef map<uint64_t, gwcXetraCacheItem*> gwcXetraCacheMap;

    gwcEti (neueda::logger* log);
    virtual ~gwcEti ();

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
    gwcEtiTcpConnectionDelegate<CodecT> mTcpConnectionDelegate;

    sbfCacheFile                  mCacheFile;
    sbfCacheFileItem              mCacheItem;
    sbfMw                         mMw;
    sbfQueue                      mQueue;
    sbfThread                     mThread;

private:   
    // utility methods
    void updateSeqno (uint64_t partId, uint64_t seqno);
    void reset ();
    void error (const string& err);
    void sendRetransRequest ();
    void sendTraderLogonRequest ();
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
    void handleExchangeMsg (int, cdr& msg, const int); 
    void handleOrderCancelRejectMsg (cdr& msg);


    static void* dispatchCb (void* closure);
    static sbfError cacheFileItemCb (sbfCacheFile file, 
                                     sbfCacheFileItem item, 
                                     void* itemData, 
                                     size_t itemSize, 
                                     void* closure);
    static void onHbTimeout (sbfTimer timer, void* closure);
    static void onReconnect (sbfTimer timer, void* closure);

    gwcXetraCacheMap mCacheMap;
    // members 
    sbfTcpConnectionAddress mTcpHost;
    bool                    mDispatching;
    sbfTimer                mHb;
    sbfTimer                mReconnectTimer;
    CodecT                  mCodec;
    bool                    mSeenHb;
    int                     mMissedHb;
    uint64_t                mOutboundSeqNo;
    int64_t                 mPartition;
    char                    mLastApplMsgId[16];
    char                    mCurrentRecoveryEnd[16];
    int64_t                 mRecoveryMsgCnt;
};

