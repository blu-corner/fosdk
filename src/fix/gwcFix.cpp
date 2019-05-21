#include "gwcFix.h"
#include "sbfInterface.h"
#include "utils.h"
#include "fields.h"

#include <sstream>

const string gwcFix::FixHeartbeat = "0";
const string gwcFix::FixTestRequest = "1";
const string gwcFix::FixResendRequest = "2";
const string gwcFix::FixReject = "3";
const string gwcFix::FixSequenceReset = "4";
const string gwcFix::FixLogout = "5";
const string gwcFix::FixExecutionReport = "8";
const string gwcFix::FixOrderCancelReject = "9";
const string gwcFix::FixLogon = "A";
const string gwcFix::FixNewOrderSingle = "D";
const string gwcFix::FixOrderCancelRequest = "F";
const string gwcFix::FixOrderCancelReplaceRequest = "G";
const string gwcFix::FixBusinessMessageReject = "j";

gwcFixTcpConnectionDelegate::gwcFixTcpConnectionDelegate (gwcFix* gwc)
    : SbfTcpConnectionDelegate (),
      mGwc (gwc)
{ 
}

void
gwcFixTcpConnectionDelegate::onReady ()
{
    mGwc->onTcpConnectionReady ();
}

void
gwcFixTcpConnectionDelegate::onError ()
{
    mGwc->onTcpConnectionError ();
}

size_t
gwcFixTcpConnectionDelegate::onRead (void* data, size_t size)
{
    return mGwc->onTcpConnectionRead (data, size);
}

extern "C" gwcConnector*
getConnector (neueda::logger* log, const neueda::properties& props)
{
    return new gwcFix (log);
}

gwcFix::gwcFix (neueda::logger* log) :
    gwcConnector (log),
    mTcpConnection (NULL),
    mTcpConnectionDelegate (this),
    mCacheFile (NULL),
    mCacheItem (NULL),        
    mMw (NULL),
    mQueue (NULL),
    mBeginString ("FIX.4.2"),
    mSenderCompID (""),
    mTargetCompID (""),
    mDataDictionary (""),
    mHeartBtInt (30),
    mResetSeqNumFlag (false),
    mDispatching (false),
    mHb (NULL),
    mReconnectTimer (NULL),
    mSeenHb (false),
    mMissedHb (0)
{
    mSeqnums.mInbound = 1;
    mSeqnums.mOutbound = 1;
}

gwcFix::~gwcFix ()
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
gwcFix::cacheFileItemCb (sbfCacheFile file,
                           sbfCacheFileItem item,
                           void* itemData,
                           size_t itemSize,
                           void* closure)
{
    gwcFix* gwc = reinterpret_cast<gwcFix*>(closure);

    if (itemSize != sizeof (gwcFixSeqnums))
    {
        gwc->mLog->err ("mismatch of sizes in seqno cache file");
        return EINVAL;
    }

    gwc->mCacheItem = item;
    memcpy (&gwc->mSeqnums, itemData, itemSize);
    return 0;
}

void 
gwcFix::onTcpConnectionReady ()
{
    mState = GWC_CONNECTOR_CONNECTED;

    char space[1024];
    size_t used = 0;

    cdr d;
    d.setString (MsgType, FixLogon);
    d.setInteger (ResetSeqNumFlag, mResetSeqNumFlag ? 'Y' : 'N');
    setHeader (d);

    mSessionsCbs->onLoggingOn (d);

    if (mCodec.encode (d, space, sizeof space, used) != GW_CODEC_SUCCESS)
    {
        mLog->err ("failed to construct logon message [%s]",
                   mCodec.getLastError ().c_str ());
        return;
    }

    mTcpConnection->send (space, used);

    lock ();

    mSeqnums.mOutbound++;
    sbfCacheFile_write (mCacheItem, &mSeqnums);
    sbfCacheFile_flush (mCacheFile);

    unlock ();
}

void 
gwcFix::onTcpConnectionError ()
{
    error ("tcp dropped connection");
}

size_t 
gwcFix::onTcpConnectionRead (void* data, size_t size)
{
    size_t left = size;
    cdr    msg;

    while (left > 0)
    {
        size_t used = 0;
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
gwcFix::onHbTimeout (sbfTimer timer, void* closure)
{
    gwcFix* gwc = reinterpret_cast<gwcFix*>(closure);

    cdr hb;
    hb.setString (MsgType, FixHeartbeat);

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
gwcFix::onReconnect (sbfTimer timer, void* closure)
{
    gwcFix* gwc = reinterpret_cast<gwcFix*>(closure);

    gwc->start (false);
}

void* 
gwcFix::dispatchCb (void* closure)
{
    gwcFix* gwc = reinterpret_cast<gwcFix*>(closure);
    sbfQueue_dispatch (gwc->mQueue);
    return NULL;
}

void
gwcFix::reset ()
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
gwcFix::error (const string& err)
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
                                           gwcFix::onReconnect,
                                           this,
                                           5.0); 
    }
}

void
gwcFix::getTime (cdrDateTime& dt)
{
    time_t t;
    struct tm* tmp;
    struct timeval tv;

    t = time(NULL);
    tmp = gmtime (&t);

    dt.mYear = tmp->tm_year;
    dt.mMonth = tmp->tm_mon;
    dt.mDay = tmp->tm_mday;

    dt.mHour = tmp->tm_hour;
    dt.mMinute = tmp->tm_min;
    dt.mSecond = tmp->tm_sec;

    if (gettimeofday (&tv, NULL) == 0)
        dt.mMillisecond = (int)tv.tv_usec / 1000;
}

void
gwcFix::setHeader (cdr& d)
{
    d.setString (BeginString, mBeginString);
    d.setString (SenderCompID, mSenderCompID);
    d.setString (TargetCompID, mTargetCompID);
    d.setInteger (MsgSeqNum, mSeqnums.mOutbound);
    d.setInteger (EncryptMethod, 0);
    d.setInteger (HeartBtInt, mHeartBtInt);

    cdrDateTime dt;
    getTime (dt);
    d.setDateTime (SendingTime, dt);
}

void 
gwcFix::handleTcpMsg (cdr& msg)
{
    mLog->info ("msg in..");
    mLog->info ("%s", msg.toString ().c_str ());
    /* any message counts as a hb */
    mSeenHb = true;

    string msgType;
    int64_t seqnum;
    msg.getString (MsgType, msgType);
    msg.getInteger (MsgSeqNum, seqnum);

    if (mState == GWC_CONNECTOR_CONNECTED)
    {
        if (msgType != FixLogon  && 
            msgType != FixLogout &&
            msgType != FixResendRequest)
            return error ("invalid response after logon");

        if (msgType == FixLogon)
        {
            mState = GWC_CONNECTOR_READY;
            mHb = sbfTimer_create (sbfMw_getDefaultThread (mMw),
                                   mQueue,
                                   gwcFix::onHbTimeout,
                                   this,
                                   mHeartBtInt);

            bool reset = false;

            string resetseqno;
            if (msg.getString (ResetSeqNumFlag, resetseqno))
            {
                if (!utils_parseBool (resetseqno, reset))
                {
                    mLog->err ("failed to parse resetseqnumflag to bool");
                    return error ("invalid logon msg");
                }
            }

            if (reset)
            {
                lock ();

                mSeqnums.mInbound = seqnum;

                sbfCacheFile_write (mCacheItem, &mSeqnums);
                sbfCacheFile_flush (mCacheFile);

                unlock ();
            }

            mSessionsCbs->onLoggedOn (seqnum, msg);
            loggedOnEvent ();         
        }
        else if (msgType == FixLogout) /* logon reject */
        {
            error ("logon rejected");
            return;
        }
    }

    // skip seqnum checking on sequence reset
    if (msgType == FixSequenceReset)
    {
        handleSequenceResetMsg (seqnum, msg);
        return;
    }

    lock ();

    if (seqnum > mSeqnums.mInbound)
    {
        mLog->warn ("gap detected, got %ld expected: %ld",
                    seqnum, mSeqnums.mInbound);

        cdr resend;
        resend.setString (MsgType, FixResendRequest);
        resend.setInteger (BeginSeqNo, mSeqnums.mInbound);
        resend.setInteger (EndSeqNo, 0);

        unlock ();

        sendMsg (resend);
        return;
    }
    else if (seqnum < mSeqnums.mInbound)
    {
        stringstream err;
        err << "sequence number too low expecting: " << mSeqnums.mInbound;
        error (err.str ());

        unlock ();

        return;
    }

    mSeqnums.mInbound = seqnum + 1;

    sbfCacheFile_write (mCacheItem, &mSeqnums);
    sbfCacheFile_flush (mCacheFile);

    unlock ();

    if (msgType == FixHeartbeat)
        mMessageCbs->onAdmin (seqnum, msg);
    else if (msgType == FixTestRequest)
        handleTestRequestMsg (seqnum, msg);
    else if (msgType == FixResendRequest)
        handleResendRequestMsg (seqnum, msg);
    else if (msgType == FixReject)
        handleRejectMsg (seqnum, msg);
    else if (msgType == FixLogout)
        handleLogoutMsg (seqnum, msg);
    else if (msgType == FixExecutionReport)
        handleExecutionReportMsg (seqnum, msg);
    else if (msgType == FixOrderCancelReject)
        handleOrderCancelRejectMsg (seqnum, msg);
    else if (msgType == FixBusinessMessageReject)
        handleBusinessRejectMsg (seqnum, msg);
    else if (msgType == FixLogon  &&
             msgType == FixLogout &&
             msgType == FixResendRequest)
        mMessageCbs->onAdmin (seqnum, msg);
    else
        mMessageCbs->onMsg (seqnum, msg);
}

void
gwcFix::handleLogoutMsg (int64_t seqno, cdr& msg)
{
    mMessageCbs->onAdmin (seqno, msg);

    // where we in a state to expect a logout
    if (mState != GWC_CONNECTOR_WAITING_LOGOFF)
    {
        error ("unsolicited logoff from exchnage");
        return;
    }
    reset ();
    mSessionsCbs->onLoggedOff (seqno, msg);
    loggedOffEvent ();
}

void
gwcFix::handleTestRequestMsg (int64_t seqno, cdr& msg)
{    
    mMessageCbs->onAdmin (seqno, msg);

    string testreqid;
    if (!msg.getString (TestReqID, testreqid))
        return;

    /* send back heartbeat message */
    cdr tr;
    tr.setString (MsgType, FixTestRequest);
    tr.setString (TestReqID, testreqid);
    sendMsg (tr);
}

void
gwcFix::handleResendRequestMsg (int64_t seqno, cdr& msg)
{    
    mMessageCbs->onAdmin (seqno, msg);

    string testreqid;
    if (!msg.getString (TestReqID, testreqid))
        return;

    /* we won't replay old messages so lets reset seqno */
    cdr sr;
    sr.setString (MsgType, FixSequenceReset);
    sr.setInteger (NewSeqNo, mSeqnums.mOutbound + 1);
    sendMsg (sr);
}

void
gwcFix::handleSequenceResetMsg (int64_t seqno, cdr& msg)
{    
    mMessageCbs->onAdmin (seqno, msg);

    int64_t newseqno;
    if (!msg.getInteger (NewSeqNo, newseqno))
        return;

    lock ();

    mSeqnums.mInbound = newseqno;
    sbfCacheFile_write (mCacheItem, &mSeqnums);
    sbfCacheFile_flush (mCacheFile);

    unlock ();
}

void
gwcFix::handleRejectMsg (int64_t seqno, cdr& msg)
{    
    mMessageCbs->onAdmin (seqno, msg);
}

void
gwcFix::handleBusinessRejectMsg (int64_t seqno, cdr& msg)
{    
    mMessageCbs->onAdmin (seqno, msg);
}

void
gwcFix::handleExecutionReportMsg (int64_t seqno, cdr& msg)
{
    string exectranstype;
    string exectype;

    if (msg.getString (ExecTransType, exectranstype))
    {
        if (exectranstype != "0")
        {
            // restatement
            mMessageCbs->onMsg (seqno, msg);
            return;
        }
    }

    if (!msg.getString (ExecType, exectype))
    {
        // invalid execution report received
        mMessageCbs->onMsg (seqno, msg);
        return;
    }   

    if (exectype == "0")
        mMessageCbs->onOrderAck (seqno, msg);
    else if (exectype == "1" || exectype == "2")
        mMessageCbs->onOrderFill (seqno, msg);
    else if (exectype == "3" || exectype == "4")
        mMessageCbs->onOrderDone (seqno, msg);
    else if (exectype == "5")
        mMessageCbs->onModifyAck (seqno, msg);
    else if (exectype == "8")
        mMessageCbs->onOrderRejected (seqno, msg);
    else
        mMessageCbs->onMsg (seqno, msg);
}

void
gwcFix::handleOrderCancelRejectMsg (int64_t seqno, cdr& msg)
{
    string cxlresp;
    if (!msg.getString (CxlRejResponseTo, cxlresp))
    {
        // invalid cancel reject msg
        mMessageCbs->onMsg (seqno, msg);
        return;
    }

    if (cxlresp == "1")
        mMessageCbs->onCancelRejected (seqno, msg);
    else if (cxlresp == "2")
        mMessageCbs->onModifyRejected (seqno, msg);
}

bool 
gwcFix::init (gwcSessionCallbacks* sessionCbs, 
                gwcMessageCallbacks* messageCbs,  
                const neueda::properties& props)
{
    mSessionsCbs = sessionCbs;
    mMessageCbs = messageCbs;

    string v;
    if (!props.get ("host", v))
    {
        mLog->err ("missing property host");
        return false;
    }
    if (sbfInterface_parseAddress (v.c_str(), &mGwHost.sin) != 0)
    {
        mLog->err ("failed to parse host [%s]", v.c_str());
        return false;
    }

    props.get ("begin_string", mBeginString);

    if (!props.get ("sender_comp_id", mSenderCompID))
    {
        mLog->err ("missing property sender_comp_id");
        return false;
    }

    string hbint;
    if (props.get ("heartbeat_interval", hbint))
    {
        if (!utils_parseNumber (hbint, mHeartBtInt))
        {
            mLog->err ("failed to parse heartbeat_interval to integer");
            return false;
        }
    }

    string resetseqno;
    if (props.get ("reset_sequence_number", resetseqno))
    {
        if (!utils_parseBool (resetseqno, mResetSeqNumFlag))
        {
            mLog->err ("failed to parse reset_sequence_number to bool");
            return false;
        }
    }

    if (!props.get ("target_comp_id", mTargetCompID))
    {
        mLog->err ("missing property target_comp_id");
        return false;
    }

    if (!props.get ("data_dictionary", mDataDictionary))
    {
        mLog->err ("missing property data_dictionary");
        return false;
    }

    string err;
    if (!mCodec.loadDataDictionary (mDataDictionary.c_str (), err))
    {
        mLog->err ("failed to load data_dictionary %s", err.c_str ());
        return false;
    }   

    string cacheFileName;
    props.get ("seqno_cache", "fix.seqno.cache", cacheFileName);
    
    int created;
    mCacheFile = sbfCacheFile_open (cacheFileName.c_str (),
                                    sizeof (gwcFixSeqnums),
                                    0,
                                    &created,
                                    cacheFileItemCb,
                                    this);
    if (mCacheFile == NULL)
    {
        mLog->err ("failed to create fix seqno cache file");
        return false;
    }

    if (created)
    {
        mLog->info ("created seqno cachefile %s", cacheFileName.c_str ());
        mCacheItem = sbfCacheFile_add (mCacheFile, &mSeqnums);
        sbfCacheFile_flush (mCacheFile);
    }

    string enableRaw;
    props.get ("enable_raw_messages", "false", enableRaw);
    if (!utils_parseBool (enableRaw, mRawEnabled))
    {
        mLog->err ("failed to parse enable_raw_messages as bool");
        return false;
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
    if (sbfThread_create (&mThread, gwcFix::dispatchCb, this) != 0)
    {
        mLog->err ("failed to start dispatch queue");
        return false;
    }

    mDispatching = true;
    return true;
}

bool 
gwcFix::start (bool reset)
{
    if (mTcpConnection != NULL)
        delete mTcpConnection;

    if (reset)
    {
        mSeqnums.mInbound = 1;
        mSeqnums.mOutbound = 1;
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
gwcFix::stop ()
{
    lock ();
    if (mState != GWC_CONNECTOR_READY) // not logged in
    {
        reset ();
        cdr logoffResponse;
        mSessionsCbs->onLoggedOff (0, logoffResponse);
        loggedOffEvent ();
        unlock ();
        return true;
    }
    unlock ();

    cdr logoff;
    logoff.setString (MsgType, FixLogout);

    if (!sendMsg (logoff))
        return false;

    mState = GWC_CONNECTOR_WAITING_LOGOFF;
    return true;
}

bool
gwcFix::mapOrderFields (gwcOrder& order)
{
    if (order.mPriceSet)
        order.setInteger (Price, order.mPrice);

    if (order.mQtySet)
        order.setInteger (OrderQty, order.mQty);

    if (order.mOrderTypeSet)
    {
        string ordType;
        switch (order.mOrderType)
        {
        case GWC_ORDER_TYPE_MARKET:
            ordType = "1";
        case GWC_ORDER_TYPE_LIMIT:
            ordType = "2";
        case GWC_ORDER_TYPE_STOP:
            ordType = "3";
        case GWC_ORDER_TYPE_STOP_LIMIT:
            ordType = "4";
        case GWC_ORDER_TYPE_MARKET_ON_CLOSE:
            ordType = "5";
        case GWC_ORDER_TYPE_WITH_OR_WITHOUT:
            ordType = "6";
        case GWC_ORDER_TYPE_LIMIT_OR_BETTER:
            ordType = "7";
        case GWC_ORDER_TYPE_LIMIT_WITH_OR_WITHOUT:
            ordType = "8";
        case GWC_ORDER_TYPE_ON_BASIS:
            ordType = "9";
        case GWC_ORDER_TYPE_ON_CLOSE:
            ordType = "A";
        case GWC_ORDER_TYPE_LIMIT_ON_CLOSE:
            ordType = "B";
        case GWC_ORDER_TYPE_FOREX:
            ordType = "C";
        case GWC_ORDER_TYPE_PREVIOUSLY_QUOTED:
            ordType = "D";
        case GWC_ORDER_TYPE_PREVIOUSLY_INDICATED:
            ordType = "E";
        case GWC_ORDER_TYPE_PEGGED:
            ordType = "P";
            break;
        default:
            break;
        }

        if(!ordType.empty())
            order.setString (OrdType, "%c", *ordType.c_str());
    }

    if (order.mSideSet)
    {
        string side;
        switch (order.mSide)
        {
        case GWC_SIDE_BUY:
            side = "1";
        case GWC_SIDE_SELL:
            side = "2";
        case GWC_SIDE_BUY_MINUS:
            side = "3";
        case GWC_SIDE_SELL_PLUS:
            side = "4";
        case GWC_SIDE_SELL_SHORT:
            side = "5";
        case GWC_SIDE_SELL_SHORT_EXEMPT:
            side = "6";
        case GWC_SIDE_UNDISCLOSED:
            side = "7";
        case GWC_SIDE_CROSS:
            side = "8";
        case GWC_SIDE_CROSS_SHORT:
            side = "9";
        case GWC_SIDE_CROSS_SHORT_EXEMPT:
            side = "A";
        case GWC_SIDE_AS_DEFINED:
            side = "B";
        case GWC_SIDE_OPPOSITE:
            side = "C";
        case GWC_SIDE_SUBSCRIBE:
            side = "D";
        case GWC_SIDE_REDEEM:
            side = "E";
        case GWC_SIDE_LEND:
            side = "F";
        case GWC_SIDE_BORROW:
            side = "G";
        case GWC_SIDE_SELL_UNDISCLOSED:
            side = "H";
            break;
        default:
            break;
        }

        if(!side.empty())
            order.setString(Side, "%c", *side.c_str());
    }

    if (order.mTifSet)
    {
        string tif;
        switch (order.mTif)
        {
        case GWC_TIF_DAY:
            tif = "0";
        case GWC_TIF_GTC:
            tif = "1";
        case GWC_TIF_OPG:
            tif = "2";
        case GWC_TIF_IOC:
            tif = "3";
        case GWC_TIF_FOK:
            tif = "4";
        case GWC_TIF_GTX:
            tif = "5";
        case GWC_TIF_GTD:
            tif = "6";
        case GWC_TIF_ATC:
            tif = "7";
            break;
        default:
            break;
        }

        if(!tif.empty()) 
            order.setString (TimeInForce, "%c", *tif.c_str());
    }

    return true;
}

bool
gwcFix::traderLogon (string& traderId, const cdr* msg)
{
    // TODO traderLogon needs looked at
    return true;
}

bool
gwcFix::sendOrder (gwcOrder& order)
{
    if (!mapOrderFields (order))
        return false;

    return sendOrder ((cdr&)order);
}

bool 
gwcFix::sendOrder (cdr& order)
{
    order.setString (MsgType, FixNewOrderSingle);

    if (!order.contains (TransactTime))
    {
        cdrDateTime dt;
        getTime (dt);
        order.setDateTime (TransactTime, dt);
    }

    return sendMsg (order);
}

bool
gwcFix::sendCancel (gwcOrder& cancel)
{
    if (!mapOrderFields (cancel))
        return false;

    return sendCancel ((cdr&)cancel);
}

bool 
gwcFix::sendCancel (cdr& cancel)
{
    cancel.setString (MsgType, FixOrderCancelRequest);

    if (!cancel.contains (TransactTime))
    {
        cdrDateTime dt;
        getTime (dt);
        cancel.setDateTime (TransactTime, dt);
    }

    return sendMsg (cancel);
}

bool
gwcFix::sendModify (gwcOrder& modify)
{
    if (!mapOrderFields (modify))
        return false;

    return sendModify ((cdr&)modify);
}

bool 
gwcFix::sendModify (cdr& modify)
{
    modify.setString (MsgType, FixOrderCancelReplaceRequest);

    if (!modify.contains (TransactTime))
    {
        cdrDateTime dt;
        getTime (dt);
        modify.setDateTime (TransactTime, dt);
    }

    return sendMsg (modify);
}

bool 
gwcFix::sendMsg (cdr& msg)
{
    char space[1024];
    size_t used = 0;


    lock ();
    if (mState != GWC_CONNECTOR_READY)
    {
        mLog->warn ("gwc not ready to send messages");
        unlock ();
        return false;
    }

    setHeader (msg);

    if (mCodec.encode (msg, space, sizeof space, used) != GW_CODEC_SUCCESS)
    {
        mLog->err ("failed to construct logon message [%s]",
                   mCodec.getLastError ().c_str ());

        unlock ();
        return false;
    }

    mLog->info ("msg out..");
    mLog->info ("%s", msg.toString ().c_str ());

    mTcpConnection->send (space, used);

    mSeqnums.mOutbound++;
    sbfCacheFile_write (mCacheItem, &mSeqnums);
    sbfCacheFile_flush (mCacheFile);

    unlock ();
    return true;
}

bool
gwcFix::sendRaw (void* data, size_t len)
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
