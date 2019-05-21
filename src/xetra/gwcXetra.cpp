#include "gwcXetra.h"
#include "sbfInterface.h"
#include "utils.h"
#include "fields.h"

#include <sstream>

gwcXetraTcpConnectionDelegate::gwcXetraTcpConnectionDelegate (gwcXetra* gwc)
    : SbfTcpConnectionDelegate (),
      mGwc (gwc)
{ 
}

void
gwcXetraTcpConnectionDelegate::onReady ()
{
    mGwc->onTcpConnectionReady ();
}

void
gwcXetraTcpConnectionDelegate::onError ()
{
    mGwc->onTcpConnectionError ();
}

size_t
gwcXetraTcpConnectionDelegate::onRead (void* data, size_t size)
{
    return mGwc->onTcpConnectionRead (data, size);
}

extern "C" gwcConnector*
getConnector (neueda::logger* log, const neueda::properties& props)
{
    return new gwcXetra (log);
}

gwcXetra::gwcXetra (neueda::logger* log) :
    gwcConnector (log),
    mTcpConnection (NULL),
    mTcpConnectionDelegate (this),
    mCacheFile (NULL),
    mCacheItem (NULL),        
    mMw (NULL),
    mQueue (NULL),
    mDispatching (false),
    mHb (NULL),
    mReconnectTimer (NULL),
    mSeenHb (false),
    mMissedHb (0),
    mOutboundSeqNo (1),
    mRecoveryMsgCnt (0)
{
    memset (mLastApplMsgId, 0x0, sizeof mLastApplMsgId);    
    memset (mCurrentRecoveryEnd, 0x0, sizeof mCurrentRecoveryEnd);
}

gwcXetra::~gwcXetra ()
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
gwcXetra::cacheFileItemCb (sbfCacheFile file,
                           sbfCacheFileItem item,
                           void* itemData,
                           size_t itemSize,
                           void* closure)
{
    gwcXetra* gwc = reinterpret_cast<gwcXetra*>(closure);

    if (itemSize != 16)
    {
        gwc->mLog->err ("mismatch of sizes in applMsgId cache file");
        return EINVAL;
    }

    gwc->mCacheItem = item;
    memcpy (gwc->mLastApplMsgId, itemData, itemSize);
    return 0;
}

void
gwcXetra::sendRetransRequest ()
{
    cdr out;
    char empty[16];
    memset (empty, 0x0, sizeof empty);
    string start (mLastApplMsgId, 16); 

    out.setInteger (TemplateID, 10026);
    out.setInteger (SubscriptionScope, 0); // XXX no value
    out.setInteger (PartitionID, mPartition);
    out.setInteger (RefApplID, 4); // session data
    if (mCacheItem != NULL)
        out.setString (ApplBegMsgID, start);
    // aways want to the end            
    sendMsg (out);
}

void
gwcXetra::updateApplMsgId (string& sMsgId)
{
    memcpy (mLastApplMsgId, sMsgId.c_str (), sizeof mLastApplMsgId);
    if (mCacheItem == NULL)
    {
        mCacheItem = sbfCacheFile_add (mCacheFile, mLastApplMsgId);
        sbfCacheFile_flush (mCacheFile);
        return;
    } 
    sbfCacheFile_write (mCacheItem, mLastApplMsgId);
    sbfCacheFile_flush (mCacheFile);
} 

void 
gwcXetra::onTcpConnectionReady ()
{
	mState = GWC_CONNECTOR_CONNECTED;

    cdr d;

	// session logon 
	d.setInteger (TemplateID, 10000);
	d.setInteger (MsgSeqNum, mOutboundSeqNo);
	d.setInteger (HeartBtInt, 10000);
	d.setString (DefaultCstmApplVerID, "7.1");
	d.setString (ApplUsageOrders, "A");
	d.setString (ApplUsageQuotes, "N");
	d.setString (OrderRoutingIndicator, "Y");
	d.setString (FIXEngineName, "Blucorner");
	d.setString (FIXEngineVersion, "1");
	d.setString (FIXEngineVendor, "Blucorner");

	// need ApplicationSystemName, ApplicationSystemVersion,
	// ApplicationSystemVendor,  PartyIDSessionID, Password
	
	mSessionsCbs->onLoggingOn (d);

    char space[1024];
    size_t used;
    if (mCodec.encode (d, space, sizeof space, used) != GW_CODEC_SUCCESS)
    {
        mLog->err ("failed to construct logon message [%s]",
                   mCodec.getLastError ().c_str ());
        return;
    }

    mTcpConnection->send (space, used);
}

void 
gwcXetra::onTcpConnectionError ()
{
    error ("tcp dropped connection");
}

size_t 
gwcXetra::onTcpConnectionRead (void* data, size_t size)
{
    size_t left = size;
    cdr    msg;

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
            handleTcpMsg (msg);
            left -= used;
            break;
        }
        data = (char*)data + used; 
    }
}

void 
gwcXetra::onHbTimeout (sbfTimer timer, void* closure)
{
    gwcXetra* gwc = reinterpret_cast<gwcXetra*>(closure);

    cdr hb;
    hb.setInteger (TemplateID, 10011);
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
gwcXetra::onReconnect (sbfTimer timer, void* closure)
{
    gwcXetra* gwc = reinterpret_cast<gwcXetra*>(closure);

    gwc->start (false);
}

void* 
gwcXetra::dispatchCb (void* closure)
{
    gwcXetra* gwc = reinterpret_cast<gwcXetra*>(closure);
    sbfQueue_dispatch (gwc->mQueue);
    return NULL;
}

void
gwcXetra::reset ()
{
    lock ();
    mOutboundSeqNo = 1;

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
gwcXetra::error (const string& err)
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
                                           gwcXetra::onReconnect,
                                           this,
                                           5.0); 
    }
}

void 
gwcXetra::handleTcpMsg (cdr& msg)
{
    int64_t templateId = 0;
    msg.getInteger (TemplateID, templateId);

    mLog->info ("msg in..");
    mLog->info ("%s", msg.toString ().c_str ());

    /* any message counts as a hb */
    mSeenHb = true;

    if (templateId == 10023) // HB Notification
    {
        mMessageCbs->onAdmin (1, msg);
        return;
    }

    if (mState == GWC_CONNECTOR_CONNECTED)
    {
        /* can be reject or LogonResponse */
        if (templateId == 10001)
        {
            mState = GWC_CONNECTOR_READY;
            mLog->info ("session logon complete");
            mMessageCbs->onAdmin (1, msg);
            mSessionsCbs->onLoggedOn (1, msg);

            mHb = sbfTimer_create (sbfMw_getDefaultThread (mMw),
                                   mQueue,
                                   gwcXetra::onHbTimeout,
                                   this,
                                   10.0);

            /* send retrans request */
            sendRetransRequest ();

            loggedOnEvent ();
        }
        /* rejected logon */ 
        else if (templateId == 10010)
        {
            handleReject (msg);
        }
        return;
    }

    int64_t seqnum = 0;
    msg.getInteger (MsgSeqNum, seqnum);

    /* check if msg has ApplMsgID and ApplResendFlag */
    string applMsgId;
    // int64_t ApplResendFlag = 0;

    if (msg.getString (ApplMsgID, applMsgId))
    {
        updateApplMsgId (applMsgId);
        if (ApplResendFlag == 1)
        {
            mRecoveryMsgCnt--;
            if (memcmp (mCurrentRecoveryEnd, 
                        mLastApplMsgId, 
                        sizeof mLastApplMsgId) == 0)
            {
                if (mRecoveryMsgCnt != 0)
                {
                    /* need to send another recover to get rest of message */
                    sendRetransRequest ();
                }
                else
                {
                    /* if mRecoveryMsgCnt == 0 and have last message we have 
                       recovered */
                    mLog->info ("message recovery completed");
                }
            }
        }                
    }

    /* ready */    
    switch (templateId)
    {        
    case 10019:
        handleTraderLogon (msg);
        break;
    case 10003:
    case 10012: // forced logoff    
        handleLogoffResponse (msg);
        break;
    case 10027:
        handleRetransMeResponse (msg);
        break;
    default:
        mMessageCbs->onMsg (seqnum, msg);
    }
}

void
gwcXetra::handleReject (cdr& msg)
{
    string  rejectText;
    int64_t sessionStatus;
    int64_t sessionRejectCode;
    int64_t seqnum;
    stringstream err;

    msg.getString (VarText, rejectText);
    msg.getInteger (SessionRejectReason, sessionRejectCode);
    msg.getInteger (SessionStatus, sessionStatus);
    msg.getInteger (MsgSeqNum, seqnum);

    /* session status tells you if this is a fatal error */
    if (sessionStatus == 4) // logout
    {
        err << "fatal reject [" << sessionStatus << "] " << rejectText;
        mMessageCbs->onMsg (seqnum, msg);
        return error (err.str ());
    }

    mMessageCbs->onMsg (seqnum, msg);
}

void 
gwcXetra::handleRetransMeResponse (cdr& msg)
{
    string end;
    msg.getInteger (ApplTotalMessageCount, mRecoveryMsgCnt);
    msg.getString (ApplEndMsgID, end);
    memcpy (mCurrentRecoveryEnd, end.c_str (), sizeof mCurrentRecoveryEnd);
}

void
gwcXetra::handleTraderLogon (cdr& msg)
{
    string traderId;

    mSessionsCbs->onTraderLogonOn (traderId, msg);
}

void
gwcXetra::handleLogoffResponse (cdr& msg)
{
    int64_t seqnum = 0;
    msg.getInteger (MsgSeqNum, seqnum);
    mMessageCbs->onAdmin (seqnum, msg);

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
gwcXetra::handleExecutionMsg (cdr& msg)
{
    uint64_t execType = 0;
    uint64_t seqno = 0;

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

void 
gwcXetra::handleOrderCancelRejectMsg (cdr& msg)
{
    uint64_t seqno = 0;
    mMessageCbs->onCancelRejected (seqno, msg); 
}

bool 
gwcXetra::init (gwcSessionCallbacks* sessionCbs, 
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
    if (sbfInterface_parseAddress (v.c_str(), &mTcpHost.sin) != 0)
    {
        mLog->err ("failed to parse recovery_host [%s]", v.c_str());
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

    string cacheFileName;
    props.get ("applMsgId_cache", "xetra.applMsgId.cache", cacheFileName);
    
    int created;
    mCacheFile = sbfCacheFile_open (cacheFileName.c_str (),
                                    16,
                                    0,
                                    &created,
                                    cacheFileItemCb,
                                    this);
    if (mCacheFile == NULL)
    {
        mLog->err ("failed to create applMsgId cache file");
        return false;
    }
    if (created)
        mLog->info ("created applMsgId cachefile %s", cacheFileName.c_str ());

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
    if (sbfThread_create (&mThread, gwcXetra::dispatchCb, this) != 0)
    {
        mLog->err ("failed to start dispatch queue");
        return false;
    }

    mDispatching = true;
    return true;
}

bool 
gwcXetra::start (bool reset)
{
	if (mTcpConnection != NULL)
		delete mTcpConnection;

    mTcpConnection = new SbfTcpConnection (mSbfLog,
                                           sbfMw_getDefaultThread (mMw),
                                           mQueue,
                                           &mTcpHost,
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
gwcXetra::stop ()
{
    lock ();
    if (mState != GWC_CONNECTOR_READY) // not logged in
    {
        reset ();
        cdr logoffResponse;
        logoffResponse.setInteger (TemplateID, 10002);
        mSessionsCbs->onLoggedOff (0, logoffResponse);
        loggedOffEvent ();
        unlock ();
        return true;
    }
    unlock ();

    cdr logoff;
    logoff.setInteger (TemplateID, 10002);

    if (!sendMsg (logoff))
        return false;
    mState = GWC_CONNECTOR_WAITING_LOGOFF;
    return true;
}

bool
gwcXetra::mapOrderFields (gwcOrder& order)
{
    if (order.mPriceSet)
        order.setDouble (Price, order.mPrice);

    if (order.mQtySet)
        order.setDouble (OrderQty, (double)order.mQty / 10000.0);

    if (order.mOrderTypeSet)
    {
        switch (order.mOrderType)
        {
        case GWC_ORDER_TYPE_MARKET:
            order.setInteger (OrdType, 1);
            break;
        case GWC_ORDER_TYPE_LIMIT:
            order.setInteger (OrdType, 2);
            break;
        case GWC_ORDER_TYPE_STOP:
            order.setInteger (OrdType, 3);
            break;
        case GWC_ORDER_TYPE_STOP_LIMIT:
            order.setInteger (OrdType, 4);
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
            order.setInteger (TimeInForce, 0);
            break;
        case GWC_TIF_IOC:
            order.setInteger (TimeInForce, 3);
            break;
        case GWC_TIF_FOK:
            order.setInteger (TimeInForce, 4);
            break;
        case GWC_TIF_GTD:
            order.setInteger (TimeInForce, 6);
            break;
        default:
            break;    
        }
    }

    return true;
}

bool
gwcXetra::sendOrder (gwcOrder& order)
{
    if (!mapOrderFields (order))
        return false;

    return sendOrder ((cdr&)order);
}

bool 
gwcXetra::sendOrder (cdr& order)
{
    order.setInteger (TemplateID, 10100);
    return sendMsg (order);
}

bool
gwcXetra::sendCancel (gwcOrder& cancel)
{
    if (!mapOrderFields (cancel))
        return false;

    return sendCancel ((cdr&)cancel);
}

bool 
gwcXetra::sendCancel (cdr& cancel)
{
    cancel.setInteger (TemplateID, 10109);
    return sendMsg (cancel);
}

bool
gwcXetra::sendModify (gwcOrder& modify)
{
    if (!mapOrderFields (modify))
        return false;

    return sendModify ((cdr&)modify);
}

bool 
gwcXetra::sendModify (cdr& modify)
{
    //TODO need to set TemplateID
    //modify.setString (MessageType, GW_XETRA_ORDER_CANCEL_REPLACE_REQUEST);
    return sendMsg (modify);
}

bool 
gwcXetra::sendMsg (cdr& msg)
{
    char space[1024];
    size_t used;
    bool hb = false;
    int64_t templateId;

    // use a codec from the stack gets around threading issues
    xetraCodec codec;

    lock ();
    if (mState != GWC_CONNECTOR_READY)
    {
        mLog->warn ("gwc not ready to send messages");
        unlock ();
        return false;
    }

    msg.getInteger (TemplateID, templateId);
    hb = templateId == 10011 ? true : false;

    if (!hb)
        mOutboundSeqNo++;
    msg.setInteger (MsgSeqNum, mOutboundSeqNo);
    if (codec.encode (msg, space, sizeof space, used) != GW_CODEC_SUCCESS)
    {
        mLog->err ("failed to construct message [%s]", 
                   codec.getLastError ().c_str ());
        if (!hb)
            mOutboundSeqNo--;
        unlock ();
        return false;
    }    
    mTcpConnection->send (space, used);
    unlock ();
    return true;
}

bool
gwcXetra::sendRaw (void* data, size_t len)
{
    if (!mRawEnabled)
    {
        mLog->warn ("raw send interface not enabled");
        return false;
    }    

    lock ();
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

bool 
gwcXetra::traderLogon (string& traderId, const cdr* msg)
{
    if (msg == NULL)
    {
        mLog->warn ("need to define cdr for tradeLogon");
        return false;
    }
    
    int64_t usr;
    string pass;
    /* need cdr since we need username and passwrd */
    if (!msg->getInteger (Username, &usr) || !msg->getString (Password, pass))
    {
        mLog->warn ("need to define username and password for tradeLogon");
        return false;
    }

    cdr tlogon;
    tlogon.setInteger (TemplateID, 10018);
    tlogon.setInteger (Username, usr);
    tlogon.setString (Password, pass);

    return sendMsg (tlogon);
}

