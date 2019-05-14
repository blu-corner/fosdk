// https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/soupbintcp.pdf

#include "sbfCommon.h"
#include "gwcSoupBin.h"

#include "sbfInterface.h"
#include "fields.h"
#include "utils.h"

#include <sstream>

#define GWC_SOUP_BIN_LOGIN_ACCEPTED_MESSAGE_TYPE 'A'
#define GWC_SOUP_BIN_LOGIN_REJECTED_MESSAGE_TYPE 'J'
#define GWC_SOUP_BIN_CLIENT_HEART_BEAT_MESSAGE_TYPE 'R'
#define GWC_SOUP_BIN_SERVER_HEART_BEAT_MESSAGE_TYPE 'H'
#define GWC_SOUP_BIN_END_OF_SESSION_MESSAGE_TYPE 'Z'
#define GWC_SOUP_BIN_LOGIN_REQUEST_MESSAGE_TYPE 'L'
#define GWC_SOUP_BIN_LOGOUT_REQUEST_MESSAGE_TYPE 'O'
#define GWC_SOUP_BIN_SEQUENCED_MESSAGE_TYPE 'S'
#define GWC_SOUP_BIN_UNSEQUENCED_MESSAGE_TYPE 'U'

static const string kDefaultCacheName = "soupbin.seqno.cache";
static const string kDefaultRawEnabled = "no";
static const double kHeartBeatTimeout = 15;
static const double kReconnectInterval = 5;


SBF_PACKED(struct gwcSoupBinHeader {
    uint16_t mMessageLength;
    char mType;
});

gwcSoupBinConnectionDelegate::gwcSoupBinConnectionDelegate (
    gwcSoupBin* gwc)
    : SbfTcpConnectionDelegate (),
      mGwc (gwc)
{ }

void
gwcSoupBinConnectionDelegate::onReady ()
{
    mGwc->onConnectionReady ();
}

void
gwcSoupBinConnectionDelegate::onError ()
{
    mGwc->onConnectionError ();
}

size_t
gwcSoupBinConnectionDelegate::onRead (void* data, size_t size)
{
    return mGwc->onConnectionRead (data, size);
}

gwcSoupBin::gwcSoupBin (neueda::logger* log) : 
    gwcConnector (log),
    mConnection (NULL),
    mConnectionDelegate (this),
    mCacheFile (NULL),
    mMw (NULL),
    mQueue (NULL),
    mSequenceNumber (0),
    mCacheItem (NULL),
    mDispatching (false),
    mHb (NULL),
    mReconnectTimer (NULL),
    mSeenMessageWithinHbInterval (false)
{

}

gwcSoupBin::~gwcSoupBin ()
{
    if (mReconnectTimer)
        sbfTimer_destroy (mReconnectTimer);
    if (mHb)
        sbfTimer_destroy (mHb);
    if (mConnection)
        delete mConnection;
    if (mCacheItem)
        delete mCacheItem;
    if (mCacheFile)
        sbfCacheFile_close (mCacheFile);
    if (mQueue)
        sbfQueue_destroy (mQueue);
    if (mDispatching)
        sbfThread_join (mThread);
    if (mMw)
        sbfMw_destroy (mMw);
}

void 
gwcSoupBin::onConnectionReady ()
{
    cdr logon;
    string empty;

    logon.setString (MessageType, "%c", GWC_SOUP_BIN_LOGIN_REQUEST_MESSAGE_TYPE);
    logon.setString (Username, empty);
    logon.setString (Password, empty);
    logon.setString (RequestedSession, empty);
    logon.setInteger (RequestedSequenceNumber, mSequenceNumber); // default is 0
    
    if (mCacheItem != NULL)
    {
        // update requested sequence number
        mSequenceNumber = mCacheItem->mData.mSeqno;
        mSession.assign (std::string (mCacheItem->mData.mSession));
        
        mLog->info ("found cached seqno [%u] for session [%s]", mSequenceNumber, mSession.c_str ());
        
        logon.setString (RequestedSession, "%s", mSession.c_str ());
        logon.setInteger (RequestedSequenceNumber, mSequenceNumber);
    }
    
    /* call session callback so that they can fill in the rest of the loggon */
    mSessionsCbs->onLoggingOn (logon);

    // ensure our state is correct to what the user wants
    logon.getString (RequestedSession, mSession);
    logon.getInteger (RequestedSequenceNumber, mSequenceNumber);

    char              space[1024];
    size_t            used;
    neueda::codec&    codec = getCodec ();

    bool ok = codec.encode (logon, space, sizeof space, used) == GW_CODEC_SUCCESS;
    if (!ok)
    {
        mLog->err ("%s", codec.getLastError ().c_str ());
        return;
    }
    mConnection->send (space, used);
}

void 
gwcSoupBin::onConnectionError ()
{
    error ("tcp drop on real time connection");
}

size_t 
gwcSoupBin::onConnectionRead (void* data, size_t size)
{
    size_t left = size;
    cdr    msg;
    gwcSoupBinHeader* hdr = (gwcSoupBinHeader*)data;

    if (mRawEnabled)
    {
        size_t used = 0;
        for (;;)
        {
            if (left < sizeof *hdr)
                return used;

            // +2 since messagelength on packet is payload + header-type
            uint16_t messageLength = ntohs (hdr->mMessageLength) + 2;
            if (left < messageLength)
                return used;

            // seen messages from the server
            mSeenMessageWithinHbInterval = true;

            // if session
            if (isSessionMessage (hdr->mType))
            {
                size_t codecUsed = 0;
                switch (getCodec ().decode (msg, (void*)hdr, left, codecUsed))
                {
                case GW_CODEC_ERROR:
                    mLog->err ("failed to decode message codec error");
                    return codecUsed;

                case GW_CODEC_ABORT:
                    mLog->err ("failed to decode message");
                    return codecUsed;

                case GW_CODEC_SUCCESS:
                {
                    handleRealTimeMsg (msg);
                    left -= codecUsed;
                    used += codecUsed;
                    hdr = (gwcSoupBinHeader*)((char*)data + codecUsed);
                }
                default:
                    return used;
                }
            }
            
            mMessageCbs->onRawMsg (0, hdr, messageLength);
                
            hdr = (gwcSoupBinHeader*)((char*)data + messageLength);
            left -= messageLength;
            used += messageLength;
        }
        return used;
    }

    for (;;)
    {
        size_t used;
        switch (getCodec ().decode (msg, data, left, used))
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

    return size - left;
}

bool
gwcSoupBin::isSessionMessage (char type) const
{
    switch (type)
    {
    case GWC_SOUP_BIN_LOGIN_ACCEPTED_MESSAGE_TYPE:
    case GWC_SOUP_BIN_LOGIN_REJECTED_MESSAGE_TYPE:
    case GWC_SOUP_BIN_SERVER_HEART_BEAT_MESSAGE_TYPE:
    case GWC_SOUP_BIN_END_OF_SESSION_MESSAGE_TYPE:
        return true;

    default:
        return false;
    }
}

void
gwcSoupBin::handleRealTimeMsg (cdr& msg)
{
    // seen messages from the server
    mSeenMessageWithinHbInterval = true;
    
    string messageType;
    msg.getString (MessageType, messageType);

    char mType = messageType[0];
    if (isSessionMessage (mType))
    {
        handleSessionMessge (msg);
    }
    else
    {
        switch (mType)
        {
        case GWC_SOUP_BIN_UNSEQUENCED_MESSAGE_TYPE:
            handleUnsequencedMessage (msg);
            break;

        case GWC_SOUP_BIN_SEQUENCED_MESSAGE_TYPE:
            mSequenceNumber++;
            updateSeqno (mSession, mSequenceNumber);
            handleSequencedMessage (msg);
            break;

        default:
            mLog->err ("unhandled message type [%c]", mType);
            mLog->err ("%s", msg.toString ().c_str ());
            break;
        }
    }
}

void
gwcSoupBin::handleSessionMessge (cdr& msg)
{
    string mType;
    msg.getString (MessageType, mType);

    if (mType[0] == GWC_SOUP_BIN_LOGIN_ACCEPTED_MESSAGE_TYPE)
    {
        mMessageCbs->onAdmin (mSequenceNumber, msg);
        mSessionsCbs->onLoggedOn (0, msg);
        mState = GWC_CONNECTOR_READY;
        loggedOnEvent ();
        
        mLog->info ("logon complete for real time connection");

        // start heartbeats
        resetHbTimer ();
    }
    else if (mType[0] == GWC_SOUP_BIN_LOGIN_REJECTED_MESSAGE_TYPE)
    {
        mMessageCbs->onAdmin (mSequenceNumber, msg);

        // where we in a state to expect a logout
        if (mState != GWC_CONNECTOR_WAITING_LOGOFF)
        {
            error ("unsolicited logoff from exchnage");
            return;
        }
        reset ();
        mSessionsCbs->onLoggedOff (mSequenceNumber, msg);
        loggedOffEvent ();
    }
    else if (mType[0] == GWC_SOUP_BIN_SERVER_HEART_BEAT_MESSAGE_TYPE)
    {
        mMessageCbs->onAdmin (mSequenceNumber, msg);
    }
    else
    {
        mLog->err ("unhandled message type [%s]", mType.c_str ());
        mLog->err ("%s", msg.toString ().c_str ());
    }
}

void
gwcSoupBin::sendHeartBeat ()
{
    cdr hb;
    hb.setString (MessageType, "%c", GWC_SOUP_BIN_CLIENT_HEART_BEAT_MESSAGE_TYPE);

    sendMsg (hb);
}

void 
gwcSoupBin::onHbTimeout (sbfTimer timer, void* closure)
{
    gwcSoupBin* gwc = reinterpret_cast<gwcSoupBin*>(closure);
    if (!gwc->mSeenMessageWithinHbInterval)
    {
        gwc->error ("missed heartbeats");
        return;
    }

    gwc->sendHeartBeat ();
    
    // reset
    gwc->mSeenMessageWithinHbInterval = false;
}

void 
gwcSoupBin::onReconnect (sbfTimer timer, void* closure)
{
    gwcSoupBin* gwc = reinterpret_cast<gwcSoupBin*>(closure);
    gwc->start (false);
}

void* 
gwcSoupBin::dispatchCb (void* closure)
{
    gwcSoupBin* gwc = reinterpret_cast<gwcSoupBin*>(closure);
    sbfQueue_dispatch (gwc->mQueue);
    return NULL;
}

sbfError 
gwcSoupBin::cacheFileItemCb (sbfCacheFile file,
                             sbfCacheFileItem item,
                             void* itemData,
                             size_t itemSize,
                             void* closure)
{
    gwcSoupBin* gwc = reinterpret_cast<gwcSoupBin*> (closure);
    if (itemSize != sizeof (gwcSoupBinSeqNum))
    {
        gwc->mLog->err ("mismatch of sizes in seqno cache file");
        return EINVAL;
    }

    gwcSoupBinSeqNum* seqno = reinterpret_cast<gwcSoupBinSeqNum*>(itemData);
    if (gwc->mCacheItem == NULL)
        gwc->mCacheItem = new gwcSoupBinCacheItem ();
        
    gwc->mCacheItem->mItem = item;
    memcpy (&(gwc->mCacheItem->mData), seqno, itemSize);
    return 0;
}

void
gwcSoupBin::updateSeqno (string& session, uint32_t seqno)
{
    mLog->info ("update seqno for session [%s] to [%u]", session.c_str (), seqno);

    if (mCacheItem)
    {
        mCacheItem->mData.mSeqno = seqno;
        strncpy (mCacheItem->mData.mSession,
                 session.c_str (),
                 sizeof(mCacheItem->mData.mSession));
        
        sbfCacheFile_write (mCacheItem->mItem, &mCacheItem->mData);
        sbfCacheFile_flush (mCacheFile);

        return;
    }

    mCacheItem = new gwcSoupBinCacheItem ();
    mCacheItem->mData.mSeqno = seqno;
    strncpy (mCacheItem->mData.mSession,
             session.c_str (),
             sizeof(mCacheItem->mData.mSession));
    
    mCacheItem->mItem = sbfCacheFile_add (mCacheFile, &mCacheItem->mData);
    sbfCacheFile_flush (mCacheFile);
}

void
gwcSoupBin::reset ()
{
    if (mConnection)
        delete mConnection;
    mConnection = NULL;

    if (mHb)
        sbfTimer_destroy (mHb);
    mHb = NULL;

    if (mReconnectTimer)
        sbfTimer_destroy (mReconnectTimer);
    mReconnectTimer = NULL;

    gwcConnector::reset ();
}

void
gwcSoupBin::resetHbTimer ()
{
    if (mHb)
        sbfTimer_destroy (mHb);

    mHb = sbfTimer_create (sbfMw_getDefaultThread (mMw),
                           mQueue,
                           gwcSoupBin::onHbTimeout,
                           this,
                           kHeartBeatTimeout);
}

void 
gwcSoupBin::error (const string& err)
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
                                           gwcSoupBin::onReconnect,
                                           this,
                                           kReconnectInterval);
    }
}

bool 
gwcSoupBin::init (gwcSessionCallbacks* sessionCbs, 
                  gwcMessageCallbacks* messageCbs,  
                  const neueda::properties& props)
{
    mSessionsCbs = sessionCbs;
    mMessageCbs = messageCbs;

    string cacheFileName;
    props.get ("seqno_cache", kDefaultCacheName, cacheFileName);

    string rtHost;
    bool ok = props.get ("host", rtHost);
    if (!ok)
    {
        mLog->err ("failed to find property [%s]", "real_time_host");
        return false;
    }
    
    if (sbfInterface_parseAddress (rtHost.c_str(), &mHost.sin) != 0)
    {
        mLog->err ("failed to parse host [%s]", rtHost.c_str());
        return false;
    }

    int created;
    mCacheFile = sbfCacheFile_open (cacheFileName.c_str (),
                                    sizeof (gwcSoupBinSeqNum),
                                    0, 
                                    &created,
                                    gwcSoupBin::cacheFileItemCb,
                                    this);
    if (mCacheFile == NULL)
    {
        mLog->err ("failed to create seqno cache file");
        return false;
    }
    if (created)
        mLog->info ("created seqno cachefile %s", cacheFileName.c_str ());  

    string enableRaw;
    props.get ("enable_raw_messages", kDefaultRawEnabled, enableRaw);
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
    if (sbfThread_create (&mThread, gwcSoupBin::dispatchCb, this) != 0)
    {
        mLog->err ("failed to start dispatch queue");
        return false;
    }

    mDispatching = true;
    return true;
}

bool 
gwcSoupBin::start (bool reset)
{
    mConnection = new SbfTcpConnection (mSbfLog,
                                        sbfMw_getDefaultThread (mMw), 
                                        mQueue,
                                        &mHost,
                                        false,
                                        true, // disable-nagles
                                        &mConnectionDelegate);
    if (!mConnection->connect ())
    {
        mLog->err ("failed to create connection to real time host");
        return false;
    }

    return true;
}

bool 
gwcSoupBin::stop ()
{
    cdr logoff;
    logoff.setString (MessageType, "%c", GWC_SOUP_BIN_LOGOUT_REQUEST_MESSAGE_TYPE);
    
    if (mState != GWC_CONNECTOR_READY) // not logged in
    {
        reset ();
        mSessionsCbs->onLoggedOff (0, logoff);
        loggedOffEvent ();
        return true;
    }
 
    mState = GWC_CONNECTOR_WAITING_LOGOFF;

    char              space[1024];
    size_t            used;
    neueda::codec&    codec = getCodec ();
    
    bool ok = codec.encode (logoff, space, sizeof space, used) == GW_CODEC_SUCCESS;
    if (!ok)
    {
        mLog->err ("%s", codec.getLastError ().c_str ());
        return false;
    }
    mConnection->send (space, used);

    return true;
}

bool
gwcSoupBin::sendRaw (void* data, size_t len)
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
 
    mConnection->send (data, len);
    return true;
}

