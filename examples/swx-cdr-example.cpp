/** Testbed for gwcConnector **/

#include "swxCodecConstants.h"
#include "gwcConnector.h"
#include "fields.h"

using namespace std;
using namespace neueda;

class sessionCallbacks : public gwcSessionCallbacks
{
public:
    virtual void onConnected ()
    {
        mLog->info ("session logged on...");
    }

    virtual void onLoggingOn (cdr& msg)
    {
        mLog->info ("session logging on...");

        /* set username and password */
        msg.setString (Username, mUsername);
        msg.setString (Password, mPassword);
    }

    virtual bool onError (const string& err)
    {
        mLog->err ("session err [%s]", err.c_str ());
        return true; /* try to reconnect */
    }

    virtual void onLoggedOn (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("session logged on...");

    }

    virtual void onLoggedOff (uint64_t seqno, const cdr& msg) 
    {
        mLog->info ("session logged off...");
    }

    virtual void onGap (uint64_t expected, uint64_t recieved)
    {
        mLog->warn ("gap detected expected [%llu] got [%llu] ",
                   (unsigned long long)expected,   
                   (unsigned long long)recieved);
    };

    gwcConnector* mGwc;
    logger* mLog;
    string mUsername;
    string mPassword;
};

/* Message callbacks */
class messageCallbacks : public gwcMessageCallbacks
{
public:
    virtual void onAdmin (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onAdmin msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onOrderAck (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onOrderAck msg...");
        mLog->info ("%s", msg.toString ().c_str ());

        /* send modify */
        cdr modify;
        
        modify.setInteger (ExistingOrderToken, 1);
        modify.setInteger (ReplacementOrderToken, 2);

        modify.setInteger (OrderQuantity, 500);
        modify.setInteger (OrderPrice, 1234);
        modify.setInteger (TimeInForce, SWX_TIMEINFORCE_IMMEDIATE);
        modify.setInteger (SecondaryQuantity, 500);
        modify.setInteger (AlgoID, 1);

        mLog->info ("sending modify");
        if (!mGwc->sendModify (modify))
            mLog->info ("failed to send modify...");
    }

    virtual void onOrderRejected (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onOrderRejected msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onOrderDone (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onOrderDone msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onOrderFill (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onOrderFill msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onModifyAck (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onModifyAck msg...");
        mLog->info ("%s", msg.toString ().c_str ());

        // cancel order
        cdr cancel;
        cancel.setInteger (OriginalOrderToken, 2);
        cancel.setInteger (OrderQuantity, 0); // not used via spec

        mLog->info ("sending cancel");
        if (!mGwc->sendCancel (cancel))
            mLog->info ("failed to send cancel ...");
    }

    virtual void onModifyRejected (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("oModifyRejected msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onCancelRejected (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onCacnelRejected msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onMsg (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onMsg msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    gwcConnector* mGwc;
    logger* mLog;
};

int main (int argc, char** argv)
{
    properties p;
    p.setProperty ("lh.console.level", "debug");
    p.setProperty ("lh.console.color", "true");

    std::string errorMessage;
    bool ok = logService::get ().configure (p, errorMessage);
    if (not ok)
    {
        std::string e("failed to configure logger: " + errorMessage);
        errx (1, "%s", e.c_str ());
    }

    properties props (p, "gwc", "soupbin", "sim");
    props.setProperty ("host", "127.0.0.1:8999");

    logger* log = logService::getLogger ("SWX_TEST");

    sessionCallbacks sessionCbs;
    sessionCbs.mUsername = "user";
    sessionCbs.mPassword = "pass";

    sessionCbs.mLog = log;
    messageCallbacks messageCbs;
    messageCbs.mLog = log;

    gwcConnector* gwc = gwcConnectorFactory::get (log, "swx", props);

    if (gwc == NULL)
        errx (1, "failed to get connector...");

    sessionCbs.mGwc = gwc;
    messageCbs.mGwc = gwc;

    log->info ("initialising connector...");
    if (!gwc->init (&sessionCbs, &messageCbs, props))
        errx (1, "failed to initialise connector...");

    log->info ("starting connector...");
    if (!gwc->start (false))
        errx (1, "failed to initialise connector...");
  
    gwc->waitForLogon ();

    /* send order */
    gwcOrder order;
    order.setPrice (1234);
    order.setQty (1000);
    order.setTif (GWC_TIF_DAY);
    order.setSide (GWC_SIDE_BUY);
    order.setOrderType (GWC_ORDER_TYPE_LIMIT);
    order.setInteger (OrderToken, 1);
    order.setString (BankInternalReference, "ABC");
    order.setInteger (OrderBook, 1147);
    order.setInteger (PrincipalId, 9999);
    order.setInteger (SecondaryQuantity, 0);
    order.setString (OrderPlacement, "C");
    order.setInteger (AlgoID, 1);

    if (!gwc->sendOrder (order))
        errx  (1, "failed to send order myorder...");

    sleep (15);
    gwc->stop ();

    sleep (2);
    log->info ("destroying connector");

    return 0;
}
