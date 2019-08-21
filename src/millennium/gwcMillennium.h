#pragma once
/*
 * Millennium connector 
 */
#include "gwcConnector.h"

#include "SbfTcpConnection.hpp"
#include "sbfCacheFile.h"
#include "sbfMw.h"

#include "lseCodec.h"
#include "osloCodec.h"
#include "turquoiseCodec.h"
#include "jseCodec.h"
#include "borsaitalianaCodec.h"

#include <map>

using namespace std;
using namespace neueda;

#define GW_MILLENNIUM_LOGON "A"
#define GW_MILLENNIUM_LOGON_REPLY "B"
#define GW_MILLENNIUM_LOGOUT "5"
#define GW_MILLENNIUM_HEARTBEAT "0"
#define GW_MILLENNIUM_MISSED_MESSAGE_REQUEST "M"
#define GW_MILLENNIUM_MISSED_MESSAGE_REQUEST_ACK "N"
#define GW_MILLENNIUM_MISSED_MESSAGE_REPORT "P"
#define GW_MILLENNIUM_REJECT "3"
#define GW_MILLENNIUM_NEW_ORDER "D"
#define GW_MILLENNIUM_ORDER_CANCEL_REQUEST "F"
#define GW_MILLENNIUM_ORDER_CANCEL_REPLACE_REQUEST "G"
#define GW_MILLENNIUM_EXECUTION_REPORT "8"
#define GW_MILLENNIUM_ORDER_CANCEL_REJECT "9"
#define GW_MILLENNIUM_BUSINESS_REJECT "j"

#define GW_MILLENNIUM_LOGON_C 'A'
#define GW_MILLENNIUM_LOGON_REPLY_C 'B'
#define GW_MILLENNIUM_LOGOUT_C '5'
#define GW_MILLENNIUM_HEARTBEAT_C '0'
#define GW_MILLENNIUM_MISSED_MESSAGE_REQUEST_C 'M'
#define GW_MILLENNIUM_MISSED_MESSAGE_REQUEST_ACK_C 'N'
#define GW_MILLENNIUM_MISSED_MESSAGE_REPORT_C 'P'
#define GW_MILLENNIUM_REJECT_C '3'
#define GW_MILLENNIUM_NEW_ORDER_C 'D'
#define GW_MILLENNIUM_ORDER_CANCEL_REQUEST_C 'F'
#define GW_MILLENNIUM_ORDER_CANCEL_REPLACE_REQUEST_C 'G'
#define GW_MILLENNIUM_EXECUTION_REPORT_C '8'
#define GW_MILLENNIUM_ORDER_CANCEL_REJECT_C '9'
#define GW_MILLENNIUM_BUSINESS_REJECT_C 'j'

struct gwcMillenniumSeqNum
{
    uint64_t mParitionId;
    uint64_t mSeqno;    
};

struct gwcMillenniumCacheItem
{
    sbfCacheFileItem    mItem;
    gwcMillenniumSeqNum mData;
};

template <typename CodecT> class gwcMillennium;
template <typename CodecT>
class gwcMillenniumRealTimeConnectionDelegate: public SbfTcpConnectionDelegate
{
    friend class gwcMillennium<CodecT>;
    
public:
    gwcMillenniumRealTimeConnectionDelegate (gwcMillennium<CodecT>* gwc);

    virtual void onReady ();

    virtual void onError ();

    virtual size_t onRead (void* data, size_t size);

private:
    gwcMillennium<CodecT>* mGwc;
};

template <typename CodecT>
class gwcMillenniumRecoveryConnectionDelegate: public SbfTcpConnectionDelegate
{
    friend class gwcMillennium<CodecT>;
    
public:
    gwcMillenniumRecoveryConnectionDelegate (gwcMillennium<CodecT>* gwc);

    virtual void onReady ();

    virtual void onError ();

    virtual size_t onRead (void* data, size_t size);

private:
    gwcMillennium<CodecT>* mGwc;
};

template <typename CodecT>
class gwcMillennium : public gwcConnector
{
    friend class gwcMillenniumRealTimeConnectionDelegate<CodecT>;
    friend class gwcMillenniumRecoveryConnectionDelegate<CodecT>;
    
public:
    typedef map<uint64_t, gwcMillenniumCacheItem*> gwcMillenniumCacheMap;

    gwcMillennium (neueda::logger* log);
    virtual ~gwcMillennium ();

    virtual bool init (gwcSessionCallbacks* sessionCbs,
                       gwcMessageCallbacks* messageCbs, 
                       const neueda::properties& props);

    virtual bool start (bool reset);

    virtual bool stop ();

    virtual bool traderLogon (const cdr* msg) {return false;}

    virtual bool sendOrder (gwcOrder& order);
    virtual bool sendOrder (cdr& order);    

    virtual bool sendCancel (gwcOrder& cancel);
    virtual bool sendCancel (cdr& cancel);

    virtual bool sendModify (gwcOrder& modify);
    virtual bool sendModify (cdr& modify);

    virtual bool sendMsg (cdr& msg);

    virtual bool sendRaw (void* data, size_t len);

protected:
    SbfTcpConnection*         mRealTimeConnection;
    gwcMillenniumRealTimeConnectionDelegate<CodecT>  mRealTimeConnectionDelegate;
    
    SbfTcpConnection*         mRecoveryConnection;
    gwcMillenniumRecoveryConnectionDelegate<CodecT>  mRecoveryConnectionDelegate;
    
    sbfCacheFile          mCacheFile;
    sbfMw                 mMw;
    sbfQueue              mQueue;
    sbfThread             mThread;

private:
    // utility methods
    void updateSeqno (uint64_t partId, uint64_t seqno);
    void reset ();
    void error (const string& err);
    bool isSessionMessage (LseHeader* hdr);
    int getSeqnum (LseHeader* hdr, uint8_t& appId);
    bool mapOrderFields (gwcOrder& order);

    // handle state
    void onRealTimeConnectionReady ();
    void onRealTimeConnectionError ();
    size_t onRealTimeConnectionRead (void* data, size_t size);
    
    void onRecoveryConnectionReady ();
    void onRecoveryConnectionError ();
    size_t onRecoveryConnectionRead (void* data, size_t size);
    
    // handle messages 
    void handleRealTimeMsg (cdr& msg);
    void handleRecoveryMsg (cdr& msg);
    void handleRejectMsg (cdr& msg);
    void handleExecutionMsg (cdr& msg); 
    void handleOrderCancelRejectMsg (cdr& msg);
    void handleBusinessRejectMsg (cdr& msg);

    static void* dispatchCb (void* closure);
    static sbfError cacheFileItemCb (sbfCacheFile file, 
                                     sbfCacheFileItem item, 
                                     void* itemData, 
                                     size_t itemSize, 
                                     void* closure);
    static void onHbTimeout (sbfTimer timer, void* closure);
    static void onReconnect (sbfTimer timer, void* closure);

    gwcMillenniumCacheMap mCacheMap;
    sbfTcpConnectionAddress mRealTimeHost;
    sbfTcpConnectionAddress mRecoveryHost;

    bool                  mDispatching;
    sbfTimer              mHb;
    sbfTimer              mReconnectTimer;

    CodecT                mCodec;

    bool                  mSeenHb;
    int                   mWaitingDownloads;

    cdr                   mLogonMsg;
};

