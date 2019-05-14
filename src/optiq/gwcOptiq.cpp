#include "gwcOptiq.h"
#include "optiqConstants.h"
#include "sbfInterface.h"
#include "utils.h"
#include "fields.h"

#include <sstream>

gwcOptiqTcpConnectionDelegate::gwcOptiqTcpConnectionDelegate (gwcOptiq* gwc)
    : SbfTcpConnectionDelegate (),
      mGwc (gwc)
{ 
}

void
gwcOptiqTcpConnectionDelegate::onReady ()
{
    mGwc->onTcpConnectionReady ();
}

void
gwcOptiqTcpConnectionDelegate::onError ()
{
    mGwc->onTcpConnectionError ();
}

size_t
gwcOptiqTcpConnectionDelegate::onRead (void* data, size_t size)
{
    return mGwc->onTcpConnectionRead (data, size);
}

extern "C" gwcConnector*
getConnector (neueda::logger* log, const neueda::properties& props)
{
    return new gwcOptiq (log);
}

gwcOptiq::gwcOptiq (neueda::logger* log) :
    gwcConnector (log),
    mTcpConnection (NULL),
    mTcpConnectionDelegate (this),
    mCacheFile (NULL),
    mCacheItem (NULL),        
    mMw (NULL),
    mQueue (NULL),
    mAccessId (-1),
    mPartition (-1),
    mDispatching (false),
    mHb (NULL),
    mReconnectTimer (NULL),
    mSeenHb (false),
    mMissedHb (0)
{
    mSeqnums.mInbound = 0;
    mSeqnums.mOutbound = 0;
}

gwcOptiq::~gwcOptiq ()
{
    if (mReconnectTimer)
        sbfTimer_destroy (mReconnectTimer);
    if (mHb)
        sbfTimer_destroy (mHb);
    if (mTcpConnection)
        delete mTcpConnection;
    if (mCacheFile)
        sbfCacheFile_close (mCacheFile);
    if (mQueue)
        sbfQueue_destroy (mQueue);
    if (mDispatching)
        sbfThread_join (mThread);
    if (mMw)
        sbfMw_destroy (mMw);
}

sbfError
gwcOptiq::cacheFileItemCb (sbfCacheFile file,
                           sbfCacheFileItem item,
                           void* itemData,
                           size_t itemSize,
                           void* closure)
{
    gwcOptiq* gwc = reinterpret_cast<gwcOptiq*>(closure);

    if (itemSize != sizeof (gwcOptiqSeqnums))
    {
        gwc->mLog->err ("mismatch of sizes in seqno cache file");
        return EINVAL;
    }

    gwc->mCacheItem = item;
    memcpy (&gwc->mSeqnums, itemData, itemSize);
    return 0;
}

void 
gwcOptiq::onTcpConnectionReady ()
{
    mState = GWC_CONNECTOR_CONNECTED;

    char space[1024];
    size_t used;

    /* sample logon message
        TemplateId - 100
          SchemaId - 0
           Version - 0
   LogicalAccessID - 1887
     OEPartitionID - 10
     LastMsgSeqNum - 0
  SoftwareProvider - BLUCNR
 QueueingIndicator - 1
    */

    cdr d;
    d.setInteger (TemplateId, OptiqLogonTemplateId);
    d.setInteger (SchemaId, 0);
    d.setInteger (Version, 0);
    d.setInteger (LogicalAccessID, mAccessId);
    d.setInteger (OEPartitionID, mPartition);

    /* if we don't want to validate or resend lost messages leave blank */
    if (mSeqnums.mInbound != -1)
        d.setInteger (LastMsgSeqNum, mSeqnums.mInbound);

    /* need to set SoftwareProvider and QueueingIndicator */
    mSessionsCbs->onLoggingOn (d);

    if (mCodec.encode (d, space, sizeof space, used) != GW_CODEC_SUCCESS)
    {
        mLog->err ("failed to construct logon message [%s]",
                   mCodec.getLastError ().c_str ());
        return;
    }

    mTcpConnection->send (space, used);
}

void 
gwcOptiq::onTcpConnectionError ()
{
    error ("tcp dropped connection");
}

size_t 
gwcOptiq::onTcpConnectionRead (void* data, size_t size)
{
    size_t left = size;
    cdr    msg;

    while (left > 0)
    {
        size_t used;
        switch (mCodec.decode (msg, data, left, used))
        {
        case GW_CODEC_ERROR:
            mLog->err ("failed to decode message codec error");
            return size - left;

        case GW_CODEC_SHORT:
            return size - left;

        case GW_CODEC_ABORT:
            mLog->err ("failed to decode message");
            return size;

        case GW_CODEC_SUCCESS:
            handleTcpMsg (msg);
            left -= used;
            break;
        }
        data = (char*)data + used; 
    }

    return size - left;
}

void 
gwcOptiq::onHbTimeout (sbfTimer timer, void* closure)
{
    gwcOptiq* gwc = reinterpret_cast<gwcOptiq*>(closure);

    cdr hb;
    hb.setInteger (TemplateId, OptiqHeartbeatTemplateId);
    gwc->sendMsg (hb);
    if (gwc->mSeenHb)
    {
        gwc->mSeenHb = false;
        gwc->mMissedHb = 0;
        return;
    }
    
    gwc->mMissedHb++;
    if (gwc->mMissedHb > 3)
        gwc->error ("missed heartbeats");
}

void 
gwcOptiq::onReconnect (sbfTimer timer, void* closure)
{
    gwcOptiq* gwc = reinterpret_cast<gwcOptiq*>(closure);

    gwc->start (false);
}

void* 
gwcOptiq::dispatchCb (void* closure)
{
    gwcOptiq* gwc = reinterpret_cast<gwcOptiq*>(closure);
    sbfQueue_dispatch (gwc->mQueue);
    return NULL;
}

void
gwcOptiq::reset ()
{
    lock ();
    
    if (mTcpConnection)
        delete mTcpConnection;
    mTcpConnection = NULL;

    if (mHb)
        sbfTimer_destroy (mHb);
    mHb = NULL;
    mSeenHb = false;
    mMissedHb = 0;
    
    if (mReconnectTimer)
        sbfTimer_destroy (mReconnectTimer);
    mReconnectTimer = NULL;

    gwcConnector::reset ();

    unlock ();
}

void 
gwcOptiq::error (const string& err)
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
                                           gwcOptiq::onReconnect,
                                           this,
                                           5.0); 
    }
}

void 
gwcOptiq::handleTcpMsg (cdr& msg)
{
    int64_t templateId = 0;
    int64_t outbound;
    int64_t inbound;

    msg.getInteger (TemplateId, templateId);

    mLog->info ("msg in..");
    mLog->info ("%s", msg.toString ().c_str ());

    /* any message counts as a hb */
    mSeenHb = true;

    if (templateId == OptiqHeartbeatTemplateId) // HB Notification
    {
        mMessageCbs->onAdmin (0, msg);
        return;
    }

    if (mState == GWC_CONNECTOR_CONNECTED)
    {
        if (templateId != OptiqLogonAckTemplateId &&
            templateId != OptiqLogonRejectTemplateId) 
        {
            return error ("invalid template response after logon");
        }

        mMessageCbs->onAdmin (0, msg);
        if (templateId == OptiqLogonAckTemplateId)
        {
            mState = GWC_CONNECTOR_READY;
            mHb = sbfTimer_create (sbfMw_getDefaultThread (mMw),
                                   mQueue,
                                   gwcOptiq::onHbTimeout,
                                   this,
                                   1); /* Hb is 1sec for SBE */

            /* sample response 
                 TemplateId - 101
                   SchemaId - 0
                    Version - 102
                 ExchangeID - EURONEXT
            LastClMsgSeqNum - 0
            */

            /* patch up outbound seqnum */
            lock ();
            msg.getInteger (LastClMsgSeqNum, outbound);
            mSeqnums.mOutbound = outbound;

            sbfCacheFile_write (mCacheItem, &mSeqnums);
            sbfCacheFile_flush (mCacheFile);
            unlock ();

            mSessionsCbs->onLoggedOn (0, msg);
            loggedOnEvent ();         
        }
        else /* logon reject */
        {
            /* pacth up outbound and inbound seqno's so that the 
               reconnect will be ok */
            lock ();
            msg.getInteger (LastClMsgSeqNum, outbound);
            msg.getInteger (LastMsgSeqNum, inbound);

            mSeqnums.mOutbound = outbound;
            mSeqnums.mInbound = inbound;

            sbfCacheFile_write (mCacheItem, &mSeqnums);
            sbfCacheFile_flush (mCacheFile);
            unlock ();

            error ("logon rejected");
        }

        return;         
    }

    if (mState != GWC_CONNECTOR_READY && 
        mState != GWC_CONNECTOR_WAITING_LOGOFF)
    {
        return error ("invalid message during state READY or WAITING_LOGOFF");
    }

    /* check for seqnum and update cache */
    int64_t seqno = 0;
    if (msg.getInteger (MsgSeqNum, seqno))
    {
        lock ();
        /* update cache */
        mSeqnums.mInbound = seqno;
        sbfCacheFile_write (mCacheItem, &mSeqnums);
        sbfCacheFile_flush (mCacheFile);
        unlock ();
    }

    /* ready or waiting logoff */    
    switch (templateId)
    { 
    case OptiqLogoutTemplateId:
        handleLogoffResponse (msg);
        break;       
    case OptiqTestRequestTemplateId: 
        handleTestRequestMsg (msg);
        break;
    case OptiqTechnicalRejectTemplateId:
        handleTechnicalRejectMsg (msg);
        break;    
    case OptiqAckTemplateId:
        handleAckMsg (seqno, msg);
        break;
    case OptiqFillTemplateId:
        handleExecutionMsg (seqno, msg);
        break;
    case OptiqKillTemplateId:
        handleKillMsg (seqno, msg);
        break;
    case OptiqRejectTemplateId:
        handleRejectMsg (seqno, msg);
        break;
    default:
        mMessageCbs->onMsg (seqno, msg);
    }
}

void
gwcOptiq::handleLogoffResponse (cdr& msg)
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
    loggedOffEvent ();
}

void
gwcOptiq::handleTestRequestMsg (cdr& msg)
{    
    mMessageCbs->onAdmin (0, msg);

    /* send back heartbeat message */
    cdr hb;
    hb.setInteger (TemplateId, OptiqHeartbeatTemplateId);
    sendMsg (hb);
}

void
gwcOptiq::handleTechnicalRejectMsg (cdr& msg)
{    
    mMessageCbs->onAdmin (0, msg);
}

void
gwcOptiq::handleAckMsg (int64_t seqno, cdr& msg)
{
    /* look at AckType field 
       OPTIQ_ACKTYPE_NEW_ORDER_ACK
       OPTIQ_ACKTYPE_REPLACE_ACK
    */

    int64_t acktype;
    msg.getInteger (AckType, acktype);

    if (acktype == OPTIQ_ACKTYPE_NEW_ORDER_ACK)
        mMessageCbs->onOrderAck (seqno, msg);
    else if (acktype == OPTIQ_ACKTYPE_REPLACE_ACK)
        mMessageCbs->onModifyAck (seqno, msg);
}

void
gwcOptiq::handleExecutionMsg (int64_t seqno, cdr& msg)
{
    mMessageCbs->onOrderFill (seqno, msg);
}

void
gwcOptiq::handleKillMsg (int64_t seqno, cdr& msg)
{
    mMessageCbs->onOrderDone (seqno, msg);
}

void
gwcOptiq::handleRejectMsg (int64_t seqno, cdr& msg)
{
    /* look at field RejectedMessageID */

    int64_t rejMsgId;
    msg.getInteger (RejectedMessageID, rejMsgId);
    if (rejMsgId == OptiqNewOrderTemplateId)
        mMessageCbs->onOrderRejected (seqno, msg);
    else if (rejMsgId == OptiqCancelReplaceTemplateId)
        mMessageCbs->onModifyRejected (seqno, msg);
    else if (rejMsgId == OptiqCancelRequestTemplateId)
        mMessageCbs->onCancelRejected (seqno, msg);
}

bool 
gwcOptiq::init (gwcSessionCallbacks* sessionCbs, 
                gwcMessageCallbacks* messageCbs,  
                const neueda::properties& props)
{
    mSessionsCbs = sessionCbs;
    mMessageCbs = messageCbs;

    string v;
    if (!props.get ("host", v))
    {
        mLog->err ("missing propertry host");
        return false;
    }
    if (sbfInterface_parseAddress (v.c_str(), &mGwHost.sin) != 0)
    {
        mLog->err ("failed to parse host [%s]", v.c_str());
        return false;
    }

    if (!props.get ("partition", v))
    {
        mLog->err ("missing propertry partition");
        return false;
    }
    if (!utils_parseNumber (v, mPartition))
    {
        mLog->err ("invalid prorpertry partition");
        return false;
    }

    if (!props.get ("accessId", v))
    {
        mLog->err ("missing propertry accessId");
        return false;
    }
    if (!utils_parseNumber (v, mAccessId))
    {
        mLog->err ("invalid prorpertry accessId");
        return false;
    }

    string cacheFileName;
    props.get ("seqno_cache", "optiq.seqno.cache", cacheFileName);
    
    int created;
    mCacheFile = sbfCacheFile_open (cacheFileName.c_str (),
                                    sizeof (gwcOptiqSeqnums),
                                    0,
                                    &created,
                                    cacheFileItemCb,
                                    this);
    if (mCacheFile == NULL)
    {
        mLog->err ("failed to create optiq seqno cache file");
        return false;
    }
    if (created)
    {
        mLog->info ("created seqno cachefile %s", cacheFileName.c_str ());
        mCacheItem = sbfCacheFile_add (mCacheFile, &mSeqnums);
        sbfCacheFile_flush (mCacheFile);
    }

    string enableRaw;
    props.get ("enable_raw_messages", "no", enableRaw);
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
    if (sbfThread_create (&mThread, gwcOptiq::dispatchCb, this) != 0)
    {
        mLog->err ("failed to start dispatch queue");
        return false;
    }

    mDispatching = true;
    return true;
}

bool 
gwcOptiq::start (bool reset)
{
    if (mTcpConnection != NULL)
        delete mTcpConnection;

    if (reset)
    {
        mSeqnums.mInbound = -1;
        mSeqnums.mOutbound = -1;
    }

    mTcpConnection = new SbfTcpConnection (mSbfLog,
                                           sbfMw_getDefaultThread (mMw),
                                           mQueue,
                                           &mGwHost,
                                           false,
                                           true, // disable-nagles
                                           &mTcpConnectionDelegate);
    if (!mTcpConnection->connect ())
    {
        mLog->err ("failed to create connection to gateway response server");
        return false;
    }

    return true;
}

bool 
gwcOptiq::stop ()
{
    lock ();
    if (mState != GWC_CONNECTOR_READY) // not logged in
    {
        reset ();
        cdr logoffResponse;
        logoffResponse.setInteger (TemplateId, OptiqLogoutTemplateId);
        mSessionsCbs->onLoggedOff (0, logoffResponse);
        loggedOffEvent ();
        unlock ();
        return true;
    }
    unlock ();

    cdr logoff;
    logoff.setInteger (TemplateId, OptiqLogoutTemplateId);
    logoff.setInteger (LogOutReasonCode, OPTIQ_LOGOUTREASONCODE_REGULAR_LOGOUT);
    if (!sendMsg (logoff))
        return false;
    mState = GWC_CONNECTOR_WAITING_LOGOFF;
    return true;
}

bool
gwcOptiq::sendOrder (gwcOrder& order)
{
    if (order.mPriceSet)
        order.setInteger (OrderPx, order.mPrice);

    if (order.mQtySet)
        order.setInteger (OrderQty, order.mQty);

    if (order.mOrderTypeSet)
    {
        switch (order.mOrderType)
        {
        case GWC_ORDER_TYPE_MARKET:
            order.setInteger (OrderType, OPTIQ_ORDERTYPE_MARKET_PEG);
            break;
        case GWC_ORDER_TYPE_LIMIT:
            order.setInteger (OrderType, OPTIQ_ORDERTYPE_LIMIT);
            break;
        case GWC_ORDER_TYPE_STOP:
            order.setInteger (
                           OrderType, 
                           OPTIQ_ORDERTYPE_STOP_MARKET_OR_STOP_MARKET_ON_QUOTE);
            break;
        case GWC_ORDER_TYPE_STOP_LIMIT:
            order.setInteger (
                           OrderType,
                           OPTIQ_ORDERTYPE_STOP_LIMIT_OR_STOP_LIMIT_ON_QUOTE);
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
            order.setInteger (OrderSide, OPTIQ_SIDE_BUY);
            break;
        case GWC_SIDE_SELL:
            order.setInteger (OrderSide, OPTIQ_SIDE_SELL);
            break;
        default:
            order.setInteger (OrderSide, OPTIQ_SIDE_SELL); 
            break;
        }
    }

    if (order.mTifSet)
    {
        switch (order.mTif)
        {
        case GWC_TIF_DAY:
            order.setInteger (TimeInForce, OPTIQ_TIMEINFORCE_DAY);
            break;
        case GWC_TIF_IOC:
            order.setInteger (TimeInForce, 
                              OPTIQ_TIMEINFORCE_IMMEDIATE_OR_CANCEL);
            break;
        case GWC_TIF_FOK:
            order.setInteger (TimeInForce, OPTIQ_TIMEINFORCE_FILL_OR_KILL);
            break;
        case GWC_TIF_GTD:
            order.setInteger (TimeInForce, OPTIQ_TIMEINFORCE_GOOD_TILL_DATE);
            break;
        default:
            break;    
        }
    }

    // downgrade to cdr so compiler picks correct method
    cdr& o = order;
    return sendOrder (o);
}

bool 
gwcOptiq::sendOrder (cdr& order)
{
    order.setInteger (TemplateId, OptiqNewOrderTemplateId);
    return sendMsg (order);
}

bool 
gwcOptiq::sendCancel (cdr& cancel)
{
    cancel.setInteger (TemplateId, OptiqCancelRequestTemplateId);
    return sendMsg (cancel);
}

bool 
gwcOptiq::sendModify (cdr& modify)
{
    modify.setInteger (TemplateId, OptiqCancelReplaceTemplateId);
    return sendMsg (modify);
}

bool 
gwcOptiq::sendMsg (cdr& msg)
{
    char space[1024];
    size_t used;
    bool admin = false;
    int64_t templateId;

    // use a codec from the stack gets around threading issues
    optiqCodec codec;

    lock ();
    if (mState != GWC_CONNECTOR_READY)
    {
        mLog->warn ("gwc not ready to send messages");
        unlock ();
        return false;
    }

    msg.getInteger (TemplateId, templateId);
    switch (templateId)
    {
    case OptiqLogonTemplateId:
    case OptiqLogonAckTemplateId:
    case OptiqLogonRejectTemplateId:
    case OptiqLogoutTemplateId:
    case OptiqHeartbeatTemplateId:
    case OptiqTestRequestTemplateId:
    case OptiqTechnicalRejectTemplateId:
        admin = true;
        break;
    default:
        admin = false;
        break;
    }

    if (!admin)
    {
        /* update seqnum cache */
        mSeqnums.mOutbound++;
        msg.setInteger (ClMsgSeqNum, mSeqnums.mOutbound);
        sbfCacheFile_write (mCacheItem, &mSeqnums);
        sbfCacheFile_flush (mCacheFile);
    }

    if (codec.encode (msg, space, sizeof space, used) != GW_CODEC_SUCCESS)
    {
        mLog->err ("failed to construct message [%s]", 
                   codec.getLastError ().c_str ());
        unlock ();
        return false;
    }   

    mTcpConnection->send (space, used);

    unlock ();
    return true;
}

bool
gwcOptiq::sendRaw (void* data, size_t len)
{
    if (!mRawEnabled)
    {
        mLog->warn ("raw send interface not enabled");
        return false;
    }    

    lock ();

    /* XXX need to setup seqnum */
    if (mState != GWC_CONNECTOR_READY)
    {
        mLog->warn ("gwc not ready to send messages");
        unlock ();
        return false;
    }
 
    mTcpConnection->send (data, len);
    unlock ();
    return true;
}

