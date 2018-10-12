#pragma once
/*
 * Generic exchange connector 
 */

#include "gwcCommon.h"
#include "properties.h"
#include "logger.h"
#include "common.h"
#include "cdr.h"

#include <string>


namespace neueda {

/* Session level callbacks */
class gwcSessionCallbacks
{
public:
    /* dtor */
    virtual ~gwcSessionCallbacks() {};

    /* Tcp connection made to exchange */
    virtual void onConnected () {};

    /* Logon message is going to be send to exchange, can add extra fields to msg */
    virtual void onLoggingOn (cdr& msg) {};

    /* Error occurred on session, return true to try and reconnect or resolve, else false to kill session */
    virtual bool onError (const std::string& error) = 0;

    /* Logon completed, incoming seqno and logon message returned */
    virtual void onLoggedOn (uint64_t seqno, const cdr& msg) = 0;

    /* Trader logon completed */
    virtual void onTraderLogonOn (std::string traderId, const cdr& msg) {};

    /* Logoff completed, same to destroy session */
    virtual void onLoggedOff (uint64_t seqno, const cdr& msg) = 0;

    /* Gap has been detected */
    virtual void onGap (uint64_t expected, uint64_t recieved) {};
};

/* Message callbacks */
class gwcMessageCallbacks
{
public:
    /* dtor */
    virtual ~gwcMessageCallbacks() {};

    /* On admin message */
    virtual void onAdmin (uint64_t seqno, const cdr& msg) {};

    /* On order accepted */
    virtual void onOrderAck (uint64_t seqno, const cdr& msg) {};

    /* On order rejected */
    virtual void onOrderRejected (uint64_t seqno, const cdr& msg) {};

    /* On order is done, i.e. expiried, IOC or canceled by user */
    virtual void onOrderDone (uint64_t seqno, const cdr& msg) {};

    /* On order fill */
    virtual void onOrderFill (uint64_t seqno, const cdr& msg) {};

    /* On modify accepted */
    virtual void onModifyAck (uint64_t seqno, const cdr& msg) {};

    /* On modify rejecyed */
    virtual void onModifyRejected (uint64_t seqno, const cdr& msg) {};

    /* On Cancel rejected */
    virtual void onCancelRejected (uint64_t seqno, const cdr& msg) {};

    /* Generic message callback */
    virtual void onMsg (uint64_t seqno, const cdr& msg) {};

    /* Raw message from not encoded into a cdr */
    virtual void onRawMsg (uint64_t seqno, const void* ptr, size_t len) {};
};

/* Enum defining conector state */
typedef enum
{
    GWC_CONNECTOR_INIT,
    GWC_CONNECTOR_CONNECTED,
    GWC_CONNECTOR_WAITING_LOGON,
    GWC_CONNECTOR_READY,
    GWC_CONNECTOR_WAITING_LOGOFF
} gwcConnectorState;

/* Generic connector, create using factory */
class gwcConnector
{
public:
    typedef gwcConnector* (*getConnector) (neueda::logger* log, const neueda::properties& props);

    gwcConnector (neueda::logger* log) : 
        mLog (log), 
        mSessionsCbs (NULL),
        mMessageCbs (NULL),
        mState (GWC_CONNECTOR_INIT),
        mLoggedOn (0),
        mRawEnabled (false)
    {
        mSbfLog = sbfLog_create (NULL, "sbf"); // can't fail
        sbfLog_setHook (mSbfLog, SBF_LOG_INFO, sbfLogCb, this);
        sbfLog_setLevel (mSbfLog, SBF_LOG_INFO);
        sbfCondVar_init (&mLoggedOnCond);
        sbfMutex_init (&mLoggedOnMutex, 1);
        sbfMutex_init (&mLock, 1);
    }

    virtual ~gwcConnector () 
    {
        if (mSbfLog)
            sbfLog_destroy (mSbfLog);
        sbfCondVar_destroy (&mLoggedOnCond);
        sbfMutex_destroy (&mLoggedOnMutex);
        sbfMutex_destroy (&mLock);
    }

    /* Init connector, returns false on error */
    virtual bool init (gwcSessionCallbacks* sessionCbs,
                       gwcMessageCallbacks* messageCbs, 
                       const neueda::properties& props) = 0;

    /* Async start of session, will start with tcp connection then logon */
    virtual bool start (bool reset) = 0;

    /* Async stop of session, send logoff wait for logoff return and kill connection */
    virtual bool stop () = 0;

    /* Logon a trader if supported by exchnage */
    virtual bool traderLogon (std::string& traderId, const cdr* msg = NULL) = 0;

    /* Send a order */
    virtual bool sendOrder (cdr& order) = 0;
    virtual bool sendOrder (gwcOrder& order) = 0;

    /* Send a cancel */
    virtual bool sendCancel (cdr& cancel) = 0;

    /* Send a modify */
    virtual bool sendModify (cdr& modify) = 0;

    /* Send a message */
    virtual bool sendMsg (cdr& msg) = 0;

    /* Send a raw message */
    virtual bool sendRaw (void* data, size_t len) = 0;

    /* wait for logon event */
    void waitForLogon ()
    {
        while (mLoggedOn == 0)
        {
            sbfMutex_lock (&mLoggedOnMutex);
            sbfCondVar_wait (&mLoggedOnCond, &mLoggedOnMutex);
            sbfMutex_unlock (&mLoggedOnMutex);
        }     
    } 

protected:
    void reset ()
    {
        mState = GWC_CONNECTOR_INIT;
        mLoggedOn = 0;
    }
    
    void loggedOnEvent ()
    {
        mLoggedOn = 1;
        sbfMutex_lock (&mLoggedOnMutex);
        sbfCondVar_signal (&mLoggedOnCond);
        sbfMutex_unlock (&mLoggedOnMutex);
    }

    void lock ()
    {
        sbfMutex_lock (&mLock);
    }

    void unlock ()
    {
        sbfMutex_unlock (&mLock);
    }

    neueda::logger*      mLog;
    sbfMutex             mLock;
    sbfLog               mSbfLog;
    gwcSessionCallbacks* mSessionsCbs;
    gwcMessageCallbacks* mMessageCbs;
    gwcConnectorState    mState;
    u_int                mLoggedOn;
    bool                 mRawEnabled;

private:
    gwcConnector (const gwcConnector& obj);
    gwcConnector& operator= (const gwcConnector& obj);

    static int sbfLogCb (sbfLog log, sbfLogLevel level, const char* message, void* closure)
    {
        gwcConnector* gwc = reinterpret_cast<gwcConnector*>(closure);

        switch (level)
        {
        case SBF_LOG_DEBUG:
            gwc->mLog->debug ("%s", message);
        case SBF_LOG_INFO:
            gwc->mLog->info ("%s", message);
            break;
        case SBF_LOG_WARN:
            gwc->mLog->warn ("%s", message);
            break;
        case SBF_LOG_ERROR:
            gwc->mLog->err ("%s", message);
            break;
        default:
            break;
        }
        return 1; // don't let sbf log it as well
    }

    sbfCondVar mLoggedOnCond;
    sbfMutex   mLoggedOnMutex;
};

/* Factory to create correct connector */
class gwcConnectorFactory
{
public:
    /* Get a connector */
    static gwcConnector* get (neueda::logger* log, const std::string& type, const neueda::properties& props);
};

}
