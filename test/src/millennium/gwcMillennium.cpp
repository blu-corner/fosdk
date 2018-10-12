#include "gwcMillennium.h"

#include "TurquoisePackets.h"
#include "OsloPackets.h"
#include "LsePackets.h"

#include "sbfInterface.h"
#include "utils.h"
#include "fields.h"

#include <sstream>

static const string defaultCacheName = "millennium.seqno.cache";
static const string defaultRawEnabled = "no";


template <typename CodecT>
gwcMillenniumRealTimeConnectionDelegate<CodecT>::gwcMillenniumRealTimeConnectionDelegate(
    gwcMillennium<CodecT>* gwc)
: SbfTcpConnectionDelegate (),
  mGwc (gwc)
{ }

template <typename CodecT>
void
gwcMillenniumRealTimeConnectionDelegate<CodecT>::onReady ()
{
    mGwc->onRealTimeConnectionReady ();
}

template <typename CodecT>
void
gwcMillenniumRealTimeConnectionDelegate<CodecT>::onError ()
{
    mGwc->onRealTimeConnectionError ();
}

template <typename CodecT>
size_t
gwcMillenniumRealTimeConnectionDelegate<CodecT>::onRead (void* data, size_t size)
{
    return mGwc->onRealTimeConnectionRead (data, size);
}

template <typename CodecT>
gwcMillenniumRecoveryConnectionDelegate<CodecT>::gwcMillenniumRecoveryConnectionDelegate(
    gwcMillennium<CodecT>* gwc
    )
: SbfTcpConnectionDelegate (),
  mGwc (gwc)
{ }

template <typename CodecT>
void
gwcMillenniumRecoveryConnectionDelegate<CodecT>::onReady ()
{
    mGwc->onRecoveryConnectionReady ();
}

template <typename CodecT>
void
gwcMillenniumRecoveryConnectionDelegate<CodecT>::onError ()
{
    mGwc->onRecoveryConnectionError ();
}

template <typename CodecT>
size_t
gwcMillenniumRecoveryConnectionDelegate<CodecT>::onRead (void* data, size_t size)
{
    return mGwc->onRecoveryConnectionRead (data, size);
}

extern "C" gwcConnector*
getConnector (neueda::logger* log, const neueda::properties& props)
{
    string venue;
    if (!props.get ("venue", venue)) 
    {
        log->warn ("property venue must be specified");
        return NULL;
    }

    if (venue == "oslo")
        return new gwcMillennium<osloCodec> (log);
    if (venue == "lse")
        return new gwcMillennium<lseCodec> (log);
    if (venue == "turqoise")
        return new gwcMillennium<turquoiseCodec> (log);

    log->warn ("unknown venue must be olso/lse");
    return NULL;
}

template <typename CodecT>
gwcMillennium<CodecT>::gwcMillennium (neueda::logger* log) : 
    gwcConnector (log),
    mRealTimeConnection (NULL),
    mRealTimeConnectionDelegate (this),
    mRecoveryConnection (NULL),
    mRecoveryConnectionDelegate (this),
    mCacheFile (NULL),
    mMw (NULL),
    mQueue (NULL),
    mDispatching (false),
    mHb (NULL),
    mReconnectTimer (NULL),
    mSeenHb (false),
    mWaitingDownloads (0)
{
    
}

template <typename CodecT>
gwcMillennium<CodecT>::~gwcMillennium ()
{
    if (mReconnectTimer)
        sbfTimer_destroy (mReconnectTimer);
    if (mHb)
        sbfTimer_destroy (mHb);
    if (mRealTimeConnection)
        delete mRealTimeConnection;
    if (mRecoveryConnection)
        delete mRecoveryConnection;
    if (mCacheFile)
        sbfCacheFile_close (mCacheFile);
    if (mQueue)
        sbfQueue_destroy (mQueue);
    if (mDispatching)
        sbfThread_join (mThread);
    if (mMw)
        sbfMw_destroy (mMw);
}

template <typename CodecT>
void 
gwcMillennium<CodecT>::onRealTimeConnectionReady ()
{
    cdr            logon;
    string         empty;

    logon.setString (MessageType, GW_MILLENNIUM_LOGON);
    logon.setString (UserName, empty);
    logon.setString (Password, empty);
    logon.setString (NewPassword, empty);
    logon.setInteger (MessageVersion, 1);

    /* call session callback so that they can fill in the rest of the loggon */
    mSessionsCbs->onLoggingOn (logon);

    char space[1024];
    size_t used;
    
    if (mCodec.encode (logon, space, sizeof space, used) != GW_CODEC_SUCCESS)
    {
        mLog->err ("failed to construct logon message");
        return;
    }

    mRealTimeConnection->send (space, used);
}

template <typename CodecT>
void 
gwcMillennium<CodecT>::onRealTimeConnectionError ()
{
    error ("tcp drop on real time connection");
}

template <typename CodecT>
size_t 
gwcMillennium<CodecT>::onRealTimeConnectionRead (void* data, size_t size)
{
    size_t         left = size;
    size_t         used = 0;
    cdr            msg;
    LseHeader*     hdr = (LseHeader*)data;
    int32_t        seqno = 0;    

    if (mRawEnabled)
    {
        for (;;)
        {
            seqno = 0;
            if (left < sizeof *hdr)
                return used;

            if (left < hdr->mMessageLength + sizeof *hdr - 1)
                return used;

            if (isSessionMessage (hdr))
            {
                size_t codecUsed = 0;
                switch (mCodec.decode (msg, (void*)hdr, left, codecUsed))
                {
                case GW_CODEC_ERROR:
                    mLog->err ("failed to decode message codec error");
                    return size;

                case GW_CODEC_ABORT:
                    mLog->err ("failed to decode message");
                    return size;

                case GW_CODEC_SUCCESS:
                    handleRealTimeMsg (msg);
                    hdr = (LseHeader*)((char*)hdr + codecUsed);
                    left -= codecUsed;
                    used += codecUsed;
                    continue;
                    
                default:
                    return size - left;
                }
            }
            else
            {
                uint8_t partId;
                seqno = getSeqnum (hdr, partId);
                if (seqno != -1)
                   updateSeqno (partId, seqno);
            }

            mMessageCbs->onRawMsg (seqno, hdr, hdr->mMessageLength + sizeof *hdr - 1);
            
            size_t messageLength = (hdr->mMessageLength + sizeof *hdr - 1);
            left -= messageLength;
            used += messageLength;
            hdr = (LseHeader*)((char*)hdr + messageLength);
            data = (char*)data + used;
        }
        return used;
    }

    for (;;)
    {
        size_t used;
        switch (mCodec.decode (msg, data, left, used))
        {
        case GW_CODEC_ERROR:
            mLog->err ("failed to decode message codec error");
            return size;

        case GW_CODEC_SHORT:
            return size - left;

        case GW_CODEC_ABORT:
            mLog->err ("failed to decode message");
            return size;

        case GW_CODEC_SUCCESS:
            handleRealTimeMsg (msg);
            left -= used;
            break;
        }
        data = (char*)data + used;
    }

    return used;
}

template <typename CodecT>
void 
gwcMillennium<CodecT>::onRecoveryConnectionReady ()
{
    cdr            logon;
    string         empty;

    logon.setString (MessageType, GW_MILLENNIUM_LOGON);
    logon.setString (UserName, empty);
    logon.setString (Password, empty);
    logon.setString (NewPassword, empty);
    logon.setInteger (MessageVersion, 1);

    /* call session callback so that they can fill in the rest of the loggon */
    mSessionsCbs->onLoggingOn (logon);

    char space[1024];
    size_t used;
    
    if (mCodec.encode (logon, space, sizeof space, used) != GW_CODEC_SUCCESS)
    {
        mLog->err ("failed to construct logon message");
        return;
    }

    mRecoveryConnection->send (space, used);
}

template <typename CodecT>
void 
gwcMillennium<CodecT>::onRecoveryConnectionError ()
{
    error ("tcp drop recovery connection");
}

template <typename CodecT>
size_t 
gwcMillennium<CodecT>::onRecoveryConnectionRead (void* data, size_t size)
{
    size_t         left = size;
    size_t         used = 0;
    cdr            msg;
    LseHeader*     hdr = (LseHeader*)data;
    int32_t        seqno = 0;
    
    if (mRawEnabled)
    {
        for (;;)
        {
            seqno = 0;
            if (left < sizeof *hdr)
                return used;
            
            if (left < hdr->mMessageLength + sizeof *hdr - 1)
                return used;

            if (isSessionMessage (hdr))
            {
                size_t codecUsed = 0;
                switch (mCodec.decode (msg, (void*)hdr, left, codecUsed))
                {
                case GW_CODEC_ERROR:
                    mLog->err ("failed to decode recovery message codec error");
                    return left;

                case GW_CODEC_ABORT:
                    mLog->err ("failed to decode recovery message");
                    return left;

                case GW_CODEC_SUCCESS:
                    handleRecoveryMsg (msg);
                    hdr = (LseHeader*)((char*)hdr + codecUsed);
                    left -= codecUsed;
                    used += codecUsed;
                    continue;
                    
                default:
                    return size - left;
                }
            }
            else
            {
                uint8_t partId;
                seqno = getSeqnum (hdr, partId);
                if (seqno != -1)
                   updateSeqno (partId, seqno);
            }

            mMessageCbs->onRawMsg (seqno, hdr, hdr->mMessageLength + sizeof *hdr - 1);

            size_t messageLength = (hdr->mMessageLength + sizeof *hdr - 1);
            hdr = (LseHeader*)((char*)hdr + messageLength);
            left -= messageLength;
            used += messageLength;
        }
        return used;
    }

    for (;;)
    {
        size_t used;
        switch (mCodec.decode (msg, data, left, used))
        {
        case GW_CODEC_ERROR:
            mLog->err ("failed to decode recovery message codec error");
            return size;

        case GW_CODEC_SHORT:
            return size - left;

        case GW_CODEC_ABORT:
            mLog->err ("failed to decode recovery message");
            return size;

        case GW_CODEC_SUCCESS:
            handleRecoveryMsg (msg);
            left -= used;
            break;
        }
        data = (char*)data + used;        
    }

    return used;
}

template <typename CodecT>
bool
gwcMillennium<CodecT>::isSessionMessage (LseHeader* hdr)
{
    switch (hdr->mMessageType)
    {
    case GW_MILLENNIUM_LOGON_C:
    case GW_MILLENNIUM_LOGON_REPLY_C:
    case GW_MILLENNIUM_LOGOUT_C:
    case GW_MILLENNIUM_HEARTBEAT_C:
    case GW_MILLENNIUM_MISSED_MESSAGE_REQUEST_C:
    case GW_MILLENNIUM_MISSED_MESSAGE_REQUEST_ACK_C:
    case GW_MILLENNIUM_MISSED_MESSAGE_REPORT_C:
        return true;
    default:
        return false;
    }
}

template <typename CodecT>
int32_t
gwcMillennium<CodecT>::getSeqnum (LseHeader* hdr, uint8_t& appId)
{
    switch (hdr->mMessageType)
    {
    case GW_MILLENNIUM_EXECUTION_REPORT_C: {
        LseExecutionReport* exec = (LseExecutionReport*)hdr;
        appId = exec->mAppID;
        return exec->mSequenceNo;
    }
    case GW_MILLENNIUM_ORDER_CANCEL_REJECT_C: {
        LseOrderCancelReject* cr = (LseOrderCancelReject*)hdr;
        appId = cr->mAppID;
        return cr->mSequenceNo;

    }
    case GW_MILLENNIUM_BUSINESS_REJECT_C: {
        LseBusinessReject* bj = (LseBusinessReject*)hdr;
        appId = bj->mAppID;
        return bj->mSequenceNo;
    }
    default:
        return -1;
    } 
}

template <typename CodecT>
void 
gwcMillennium<CodecT>::onHbTimeout (sbfTimer timer, void* closure)
{
    gwcMillennium* gwc = reinterpret_cast<gwcMillennium*>(closure);

    if (gwc->mSeenHb)
    {
        gwc->mSeenHb = false;
        return;
    }
    
    gwc->error ("missed heartbeats");
}

template <typename CodecT>
void 
gwcMillennium<CodecT>::onReconnect (sbfTimer timer, void* closure)
{
    gwcMillennium* gwc = reinterpret_cast<gwcMillennium*>(closure);

    gwc->start (false);
}

template <typename CodecT>
void* 
gwcMillennium<CodecT>::dispatchCb (void* closure)
{
    gwcMillennium* gwc = reinterpret_cast<gwcMillennium*>(closure);
    sbfQueue_dispatch (gwc->mQueue);
    return NULL;
}

template <typename CodecT>
sbfError 
gwcMillennium<CodecT>::cacheFileItemCb (sbfCacheFile file,
                                        sbfCacheFileItem item,
                                        void* itemData,
                                        size_t itemSize,
                                        void* closure)
{
    gwcMillennium* gwc = reinterpret_cast<gwcMillennium*>(closure);
    
    if (itemSize != sizeof (gwcMillenniumSeqNum))
    {
        gwc->mLog->err ("mismatch of sizes in seqno cache file");
        return EINVAL;
    }

    gwcMillenniumSeqNum* seqno = reinterpret_cast<gwcMillenniumSeqNum*>(itemData);

    gwcMillenniumCacheItem* ci = new gwcMillenniumCacheItem ();
    ci->mItem = item;
    memcpy (&ci->mData, seqno, itemSize);
    
    // store in map for easy look up
    gwc->mCacheMap[seqno->mParitionId] = ci;

    return 0;
}

template <typename CodecT>
void
gwcMillennium<CodecT>::updateSeqno (uint64_t partId, uint64_t seqno)
{
    gwcMillenniumCacheMap::iterator itr = mCacheMap.find (partId);

    if (itr != mCacheMap.end ())
    {
        itr->second->mData.mSeqno = seqno;
        sbfCacheFile_write (itr->second->mItem, &itr->second->mData);
        sbfCacheFile_flush (mCacheFile);
        return;
    }

    // haven't seen this partition before so add it
    gwcMillenniumCacheItem* ci = new gwcMillenniumCacheItem ();
    ci->mData.mSeqno = seqno;
    ci->mData.mParitionId = partId;
    ci->mItem = sbfCacheFile_add (mCacheFile, &ci->mData);

    mCacheMap[partId] = ci;
    sbfCacheFile_flush (mCacheFile);
}

template <typename CodecT>
void
gwcMillennium<CodecT>::reset ()
{
    if (mRealTimeConnection)
        delete mRealTimeConnection;
    mRealTimeConnection = NULL;

    if (mRecoveryConnection)
        delete mRecoveryConnection;
    mRecoveryConnection = NULL;

    if (mHb)
        sbfTimer_destroy (mHb);
    mHb = NULL;

    if (mReconnectTimer)
        sbfTimer_destroy (mReconnectTimer);
    mReconnectTimer = NULL;

    mSeenHb = false;
    mWaitingDownloads = 0;

    gwcConnector::reset ();
}

template <typename CodecT>
void 
gwcMillennium<CodecT>::error (const string& err)
{
    bool reconnect;

    mLog->err ("%s", err.c_str());
    reset ();

    reconnect = mSessionsCbs->onError (err);
    if (reconnect)
    {
        mLog->info ("reconnecting in 5 secsonds...");
        mReconnectTimer = sbfTimer_create (sbfMw_getDefaultThread (mMw),
                                           mQueue,
                                           gwcMillennium<CodecT>::onReconnect,
                                           this,
                                           5.0); 
    }
}

template <typename CodecT>
void 
gwcMillennium<CodecT>::handleRealTimeMsg (cdr& msg)
{
    string mType;
    msg.getString (MessageType, mType);
    mSeenHb = true;

    if (mType == GW_MILLENNIUM_LOGON_REPLY)
    {
        mMessageCbs->onAdmin (0, msg);
        uint64_t rejectCode;
        msg.getInteger (RejectCode, rejectCode);
        if (rejectCode != 0)
        {
            stringstream ss;
            ss << "real time logon failed code [" << rejectCode << "]";
            error (ss.str());
            return;
        }

        mLog->info ("logon complete for real time connection");

        // initiate connection to recovery server
        if (!mRecoveryConnection->connect ())
        {
            error ("failed to create tcp connection for recovery");
            return ;
        }

        // copy logon reply message for later 
        mLogonMsg = msg;

        // start HB timer 
        mHb = sbfTimer_create (sbfMw_getDefaultThread (mMw),
                               mQueue,
                               gwcMillennium<CodecT>::onHbTimeout,
                               this,
                               10.0);
    }

    else if (mType == GW_MILLENNIUM_LOGOUT)
    {
        mMessageCbs->onAdmin (0, msg);
        // where we in a state to expect a logout
        if (mState != GWC_CONNECTOR_WAITING_LOGOFF)
        {
            error ("unsolicited logoff from exchnage");
            return;
        }
        reset ();
        mSessionsCbs->onLoggedOff (0, msg);
    }
    else if (mType == GW_MILLENNIUM_HEARTBEAT)
    {
        mMessageCbs->onAdmin (0, msg);

        // send hb back 
        cdr hb;
        hb.setString (MessageType, GW_MILLENNIUM_HEARTBEAT);
        
        char space[1024];
        size_t used;
        mCodec.encode (hb, space, sizeof space, used);
        mRealTimeConnection->send (space, used);
    } 
    else if (mType == GW_MILLENNIUM_REJECT)
    {
        handleRejectMsg (msg);
    } 
    else if (mType == GW_MILLENNIUM_EXECUTION_REPORT)
    {
        handleExecutionMsg (msg);
    } 
    else if (mType == GW_MILLENNIUM_ORDER_CANCEL_REJECT)
    {
        handleOrderCancelRejectMsg (msg);
    } 
    else if (mType == GW_MILLENNIUM_BUSINESS_REJECT)
    {
        handleBusinessRejectMsg (msg);
    }
}

template <typename CodecT>
void 
gwcMillennium<CodecT>::handleRecoveryMsg (cdr& msg)
{
    string mType;
    msg.getString (MessageType, mType);

    if (mType == GW_MILLENNIUM_LOGON_REPLY)
    {
        mMessageCbs->onAdmin (0, msg);

        uint64_t rejectCode;
        msg.getInteger (RejectCode, rejectCode);
        if (rejectCode != 0)
        {
            stringstream ss;
            ss << "recovery logon failed code [" << rejectCode << "]";
            error (ss.str ());
            return;
        }

        mLog->info ("logon complete for recovery connection");
    
        cdr missedmsgs;
        missedmsgs.setString (MessageType, GW_MILLENNIUM_MISSED_MESSAGE_REQUEST);
        gwcMillenniumCacheMap::iterator itr = mCacheMap.begin ();
        for (; itr != mCacheMap.end(); ++itr)
        {
            missedmsgs.setInteger (AppID, itr->first);
            missedmsgs.setInteger (LastMsgSeqNum, itr->second->mData.mSeqno);

            char space[1024];
            size_t used;
            mCodec.encode (missedmsgs, space, sizeof space, used);
            mRecoveryConnection->send (space, used);
            mWaitingDownloads++;
        }
        
        mLog->info ("send %d recovery requests", mWaitingDownloads);
        if (mWaitingDownloads == 0)
        {
            mState = GWC_CONNECTOR_READY;
            delete mRecoveryConnection;
            mRecoveryConnection = NULL;
            mSessionsCbs->onLoggedOn (0, mLogonMsg);
            loggedOnEvent ();
        }
    }
    else if (mType == GW_MILLENNIUM_HEARTBEAT)
    {
        mMessageCbs->onAdmin (0, msg);

        cdr hb;
        hb.setString (MessageType, GW_MILLENNIUM_HEARTBEAT);
        
        char space[1024];
        size_t used;
        mCodec.encode (hb, space, sizeof space, used);
        mRecoveryConnection->send (space, used);
    }
    else if (mType == GW_MILLENNIUM_MISSED_MESSAGE_REQUEST_ACK)
    {
        mMessageCbs->onAdmin (0, msg);

        uint64_t rType;
        msg.getInteger (ResponseType, rType);
        if (rType != 0)
        {
            mLog->warn ("missed message ack response type (%lld) some messages might be missing", 
                        (signed long long)rType);
            mWaitingDownloads--; 
            if (mWaitingDownloads == 0)
            {
                mState = GWC_CONNECTOR_READY;
                delete mRecoveryConnection;
                mRecoveryConnection = NULL;
                mSessionsCbs->onLoggedOn (0, mLogonMsg);
                loggedOnEvent ();
            }
        }
    }
    else if (mType == GW_MILLENNIUM_MISSED_MESSAGE_REPORT)
    {
        mMessageCbs->onAdmin (0, msg);

        uint64_t rType;
        msg.getInteger (ResponseType, rType);

        if (rType != 0)
            mLog->warn ("missed message report response type (%lld) some messages might be missing", 
                        (signed long long)rType);

        mWaitingDownloads--; 
        if (mWaitingDownloads == 0)
        {
            mState = GWC_CONNECTOR_READY;
            delete mRecoveryConnection;
            mRecoveryConnection = NULL;
            mSessionsCbs->onLoggedOn (0, mLogonMsg);
            loggedOnEvent ();
        }
    } 
    else if (mType == GW_MILLENNIUM_REJECT)
    {
        handleRejectMsg (msg);
    } 
    else if (mType == GW_MILLENNIUM_EXECUTION_REPORT)
    {
        handleExecutionMsg (msg);
    } 
    else if (mType == GW_MILLENNIUM_ORDER_CANCEL_REJECT)
    {
        handleOrderCancelRejectMsg (msg);
    } 
    else if (mType == GW_MILLENNIUM_BUSINESS_REJECT)
    {
        handleBusinessRejectMsg (msg);
    }
}

template <typename CodecT>
void 
gwcMillennium<CodecT>::handleRejectMsg (cdr& msg)
{
    /* no seqno */
    mMessageCbs->onMsg (0, msg);
}

template <typename CodecT>
void
gwcMillennium<CodecT>::handleExecutionMsg (cdr& msg)
{
    uint64_t partId;
    uint64_t seqno;

    msg.getInteger (AppID, partId);
    msg.getInteger (SequenceNo, seqno);
    updateSeqno (partId, seqno);

    uint64_t execType;
    msg.getInteger (ExecType, execType);

    switch (execType)
    {
    case '0':
        mMessageCbs->onOrderAck (seqno, msg);
        break;
    case '4':
        mMessageCbs->onOrderDone (seqno, msg);
        break;
    case '5':
        mMessageCbs->onModifyAck (seqno, msg);
        break;
    case '8':
        mMessageCbs->onOrderRejected (seqno, msg);
        break;
    case 'C':
        mMessageCbs->onOrderDone (seqno, msg);
        break;
    case 'F':
        mMessageCbs->onOrderFill (seqno, msg);
        break;
    default:
        mMessageCbs->onMsg (seqno, msg);
    }
}

template <typename CodecT>
void 
gwcMillennium<CodecT>::handleOrderCancelRejectMsg (cdr& msg)
{
    uint64_t partId;
    uint64_t seqno;

    msg.getInteger (AppID, partId);
    msg.getInteger (SequenceNo, seqno);
    updateSeqno (partId, seqno);

    mMessageCbs->onCancelRejected (seqno, msg); 
}

template <typename CodecT>
void 
gwcMillennium<CodecT>::handleBusinessRejectMsg (cdr& msg)
{
    uint64_t partId;
    uint64_t seqno;

    msg.getInteger (AppID, partId);
    msg.getInteger (SequenceNo, seqno);
    updateSeqno (partId, seqno);

    mMessageCbs->onMsg (seqno, msg);
}

template <typename CodecT>
bool 
gwcMillennium<CodecT>::init (gwcSessionCallbacks* sessionCbs, 
                             gwcMessageCallbacks* messageCbs,  
                             const neueda::properties& props)
{
    mSessionsCbs = sessionCbs;
    mMessageCbs = messageCbs;

    /* get props
       - seqno_cache default millennium.seqno.cache
       - real_time_host
       - recovery_host
    */

    string cacheFileName;
    props.get ("seqno_cache", defaultCacheName, cacheFileName);

    string rtHost;
    bool ok = props.get ("real_time_host", rtHost);
    if (not ok)
    {
        mLog->err ("failed to find property [%s]", "real_time_host");
        return false;
    }
    
    if (sbfInterface_parseAddress (rtHost.c_str(), &mRealTimeHost.sin) != 0)
    {
        mLog->err ("failed to parse real_time_host [%s]", rtHost.c_str());
        return false;
    }

    string rcHost;
    ok = props.get ("recovery_host", rcHost);
    if (not ok)
    {
        mLog->err ("missing propertry recovery_host");
        return false;
    }
    if (sbfInterface_parseAddress (rcHost.c_str(), &mRecoveryHost.sin) != 0)
    {
        mLog->err ("failed to parse recovery_host [%s]", rcHost.c_str());
        return false;
    }

    int created;
    mCacheFile = sbfCacheFile_open (cacheFileName.c_str (),
                                    sizeof (gwcMillenniumSeqNum),
                                    0, 
                                    &created,
                                    gwcMillennium<CodecT>::cacheFileItemCb,
                                    this);
    if (mCacheFile == NULL)
    {
        mLog->err ("failed to create seqno cache file");
        return false;
    }
    if (created)
        mLog->info ("created seqno cachefile %s", cacheFileName.c_str ());  


    string enableRaw;
    props.get ("enable_raw_messages", defaultRawEnabled, enableRaw);
    if (enableRaw == "Y"    || 
        enableRaw == "y"    ||
        enableRaw == "Yes"  ||
        enableRaw == "yes"  ||
        enableRaw == "True" ||
        enableRaw == "true" ||
        enableRaw == "1")
    {
        mRawEnabled = true;
    }

    sbfKeyValue kv = sbfKeyValue_create ();
    mMw = sbfMw_create (mSbfLog, kv);
    sbfKeyValue_destroy (kv);
    if (mMw == NULL)
    {
        mLog->err ("failed to create mw");
        return false;
    }

    // could add a prop to make queue spin for max performance
    mQueue = sbfQueue_create (mMw, "default");
    if (mQueue == NULL)
    {
        mLog->err ("failed to create queue");
        return false;
    }

    // start to dispatch 
    if (sbfThread_create (&mThread, gwcMillennium<CodecT>::dispatchCb, this) != 0)
    {
        mLog->err ("failed to start dispatch queue");
        return false;
    }

    mDispatching = true;
    return true;
}

template <typename CodecT>
bool 
gwcMillennium<CodecT>::start (bool reset)
{
    /* make connection to realtime host then logon to it then logon to recovery host once
       logged on then request recovery of messages for each partition once download
       complete call logged on callback, we are full recovered at that point. 
       If there is a tcp conenction error or a logoff from exchange at any time, break
       all tcp connections and call errorCb */

    // reset doesn't mean anything here since there are no outbound seqnums
    mRealTimeConnection = new SbfTcpConnection (mSbfLog,
                                                sbfMw_getDefaultThread (mMw),
                                                mQueue,
                                                &mRealTimeHost,
                                                false,
                                                true, // disable-nagles
                                                &mRealTimeConnectionDelegate);
    mRecoveryConnection = new SbfTcpConnection (mSbfLog,
                                                sbfMw_getDefaultThread (mMw),
                                                mQueue,
                                                &mRecoveryHost,
                                                false,
                                                true, // disable-nagles
                                                &mRecoveryConnectionDelegate);
    if (!mRealTimeConnection->connect ())
    {
        mLog->err ("failed to create connection to real time host");
        return false;
    }

    return true;
}

template <typename CodecT>
bool 
gwcMillennium<CodecT>::stop ()
{
    if (mState != GWC_CONNECTOR_READY) // not logged in
    {
        reset ();
        cdr dlogoff;
        dlogoff.setString (MessageType, GW_MILLENNIUM_LOGOUT);
        mSessionsCbs->onLoggedOff (0, dlogoff);
        return true;
    }
 
    mState = GWC_CONNECTOR_WAITING_LOGOFF;
    cdr logoff;
    logoff.setString (MessageType, GW_MILLENNIUM_LOGOUT);
    logoff.setString (Reason, "logoff");

    CodecT            codec;
    char              space[1024];
    size_t            used;

    codec.encode (logoff, space, sizeof space, used);
    mRealTimeConnection->send (space, used);

    return true;
}

template <typename CodecT>
bool
gwcMillennium<CodecT>::sendOrder (gwcOrder& order)
{
    if (order.mPriceSet)
        order.setDouble (LimitPrice, order.mPrice);

    if (order.mQtySet)
        order.setInteger (OrderQty, order.mQty);

    if (order.mOrderTypeSet)
    {
        switch (order.mOrderType)
        {
        case GWC_ORDER_TYPE_MARKET:
            order.setInteger (OrderType, 1);
            break;
        case GWC_ORDER_TYPE_LIMIT:
            order.setInteger (OrderType, 2);
            break;
        case GWC_ORDER_TYPE_STOP:
            order.setInteger (OrderType, 3);
            break;
        case GWC_ORDER_TYPE_STOP_LIMIT:
            order.setInteger (OrderType, 4);
            break;
        default:
            mLog->err ("invalid order type");
            return false;
        }
    }

    if (order.mSideSet)
    {
        switch (order.mSide)
        {
        case GWC_SIDE_BUY:
            order.setInteger (Side, 1);
            break;
        case GWC_SIDE_SELL:
            order.setInteger (Side, 2);
            break;
        default:
            order.setInteger (Side, 2); 
            break;
        }
    }

    if (order.mTifSet)
    {
        switch (order.mTif)
        {
        case GWC_TIF_DAY:
            order.setInteger (TIF, 0);
            break;
        case GWC_TIF_IOC:
            order.setInteger (TIF, 3);
            break;
        case GWC_TIF_FOK:
            order.setInteger (TIF, 4);
            break;
        case GWC_TIF_OPG:
            order.setInteger (TIF, 5);
            break;
        case GWC_TIF_GTD:
            order.setInteger (TIF, 6);
            break;
        case GWC_TIF_GTT:
            order.setInteger (TIF, 8);
            break;
        case GWC_TIF_ATC:
            order.setInteger (TIF, 10);
            break;
        case GWC_TIF_CPX:
            order.setInteger (TIF, 12);
            break;
        case GWC_TIF_GFA:
            order.setInteger (TIF, 50);
            break;
        case GWC_TIF_GFX:
            order.setInteger (TIF, 51);
            break;
        case GWC_TIF_GFS:
            order.setInteger (TIF, 52);
            break;
        }
    }

    // downgrade to cdr so compiler picks correct method
    cdr& o = order;
    return sendOrder (o);
}

template <>
bool
gwcMillennium<osloCodec>::sendOrder (gwcOrder& order)
{
    if (order.mPriceSet)
        order.setDouble (LimitPrice, order.mPrice);

    if (order.mQtySet)
        order.setInteger (OrderQty, order.mQty);

    if (order.mOrderTypeSet)
    {
        switch (order.mOrderType)
        {
        case GWC_ORDER_TYPE_MARKET:
            order.setInteger (OrderType, 1);
            break;
        case GWC_ORDER_TYPE_LIMIT:
            order.setInteger (OrderType, 2);
            break;
        default:
            mLog->err ("invalid order type");
            return false;
        }
    }

    if (order.mSideSet)
    {
        switch (order.mSide)
        {
        case GWC_SIDE_BUY:
            order.setInteger (Side, 1);
            break;
        case GWC_SIDE_SELL:
            order.setInteger (Side, 2);
            break;
        default:
            order.setInteger (Side, 2); 
            break;
        }
    }

    if (order.mTifSet)
    {
        switch (order.mTif)
        {
        case GWC_TIF_DAY:
            order.setInteger (TIF, 0);
            break;
        case GWC_TIF_IOC:
            order.setInteger (TIF, 3);
            break;
        case GWC_TIF_FOK:
            order.setInteger (TIF, 4);
            break;
        case GWC_TIF_OPG:
            order.setInteger (TIF, 5);
            break;
        case GWC_TIF_GTD:
            order.setInteger (TIF, 6);
            break;
        case GWC_TIF_GTT:
            order.setInteger (TIF, 8);
            break;
        case GWC_TIF_GFA:
            order.setInteger (TIF, 9);
            break;
        case GWC_TIF_ATC:
            order.setInteger (TIF, 10);
            break;
        case GWC_TIF_GFS:
            order.setInteger (TIF, 52);
            break;
        }
    }

    // downgrade to cdr so compiler picks correct method
    cdr& o = order;
    return sendOrder (o);
}


template <>
bool
gwcMillennium<lseCodec>::sendOrder (gwcOrder& order)
{
    if (order.mPriceSet)
        order.setDouble (LimitPrice, order.mPrice);

    if (order.mQtySet)
        order.setInteger (OrderQty, order.mQty);

    if (order.mOrderTypeSet)
    {
        switch (order.mOrderType)
        {
        case GWC_ORDER_TYPE_MARKET:
            order.setInteger (OrderType, 1);
            break;
        case GWC_ORDER_TYPE_LIMIT:
            order.setInteger (OrderType, 2);
            break;
        case GWC_ORDER_TYPE_STOP:
            order.setInteger (OrderType, 3);
            break;
        case GWC_ORDER_TYPE_STOP_LIMIT:
            order.setInteger (OrderType, 4);
            break;
        default:
            mLog->err ("invalid order type");
            return false;
        }
    }

    if (order.mSideSet)
    {
        switch (order.mSide)
        {
        case GWC_SIDE_BUY:
            order.setInteger (Side, 1);
            break;
        case GWC_SIDE_SELL:
            order.setInteger (Side, 2);
            break;
        default:
            order.setInteger (Side, 2); 
            break;
        }
    }

    if (order.mTifSet)
    {
        switch (order.mTif)
        {
        case GWC_TIF_DAY:
            order.setInteger (TIF, 0);
            break;
        case GWC_TIF_IOC:
            order.setInteger (TIF, 3);
            break;
        case GWC_TIF_FOK:
            order.setInteger (TIF, 4);
            break;
        case GWC_TIF_OPG:
            order.setInteger (TIF, 5);
            break;
        case GWC_TIF_GTD:
            order.setInteger (TIF, 6);
            break;
        case GWC_TIF_GTT:
            order.setInteger (TIF, 8);
            break;
        case GWC_TIF_ATC:
            order.setInteger (TIF, 10);
            break;
        case GWC_TIF_CPX:
            order.setInteger (TIF, 12);
            break;
        case GWC_TIF_GFA:
            order.setInteger (TIF, 50);
            break;
        case GWC_TIF_GFX:
            order.setInteger (TIF, 51);
            break;
        case GWC_TIF_GFS:
            order.setInteger (TIF, 52);
            break;
        }
    }

    // downgrade to cdr so compiler picks correct method
    cdr& o = order;
    return sendOrder (o);
}

template <typename CodecT>
bool 
gwcMillennium<CodecT>::sendOrder (cdr& order)
{
    order.setString (MessageType, GW_MILLENNIUM_NEW_ORDER);
    return sendMsg (order);
}

template <typename CodecT>
bool 
gwcMillennium<CodecT>::sendCancel (cdr& cancel)
{
    cancel.setString (MessageType, GW_MILLENNIUM_ORDER_CANCEL_REQUEST);
    return sendMsg (cancel);
}

template <typename CodecT>
bool 
gwcMillennium<CodecT>::sendModify (cdr& modify)
{
    modify.setString (MessageType, GW_MILLENNIUM_ORDER_CANCEL_REPLACE_REQUEST);
    return sendMsg (modify);
}

template <typename CodecT>
bool 
gwcMillennium<CodecT>::sendMsg (cdr& msg)
{
    char space[1024];
    size_t used;
    
    // use a codec from the stack gets around threading issues
    CodecT codec;

    if (mState != GWC_CONNECTOR_READY)
    {
        mLog->warn ("gwc not ready to send messages");
        return false;
    }
    
    if (codec.encode (msg, space, sizeof space, used) != GW_CODEC_SUCCESS)
    {
        mLog->err ("failed to construct message [%s]", codec.getLastError ().c_str ());
        return false;
    }

    mRealTimeConnection->send (space, used);
    return true;
}

template <typename CodecT>
bool
gwcMillennium<CodecT>::sendRaw (void* data, size_t len)
{
    if (!mRawEnabled)
    {
        mLog->warn ("raw send interface not enabled");
        return false;
    }    

    if (mState != GWC_CONNECTOR_READY)
    {
        mLog->warn ("gwc not ready to send messages");
        return false;
    }
 
    mRealTimeConnection->send (data, len);
    return true;
}

// get concretes into object for unit-testing and swig bindings

// lse
template class gwcMillennium<neueda::lseCodec>;
template class gwcMillenniumRealTimeConnectionDelegate<neueda::lseCodec>;
template class gwcMillenniumRecoveryConnectionDelegate<neueda::lseCodec>;

// oslo
template class gwcMillennium<neueda::osloCodec>;
template class gwcMillenniumRealTimeConnectionDelegate<neueda::osloCodec>;
template class gwcMillenniumRecoveryConnectionDelegate<neueda::osloCodec>;

// turquoise
template class gwcMillennium<neueda::turquoiseCodec>;
template class gwcMillenniumRealTimeConnectionDelegate<neueda::turquoiseCodec>;
template class gwcMillenniumRecoveryConnectionDelegate<neueda::turquoiseCodec>;
