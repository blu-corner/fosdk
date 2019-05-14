/** Testbed for gwcConnector **/

#include "gwcConnector.h"
#include "fields.h"

using namespace std;
using namespace neueda;

void
getTime (cdrDateTime& dt)
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

        msg.setString (DefaultApplVerID, "9");
        msg.setString (DefaultCstmApplVerID, "T4.0");
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

        string origcloid;
        msg.getString (ClOrdID, origcloid);

        /* send modify */
        gwcOrder modify;
        modify.setString (OrigClOrdID, origcloid);
        modify.setString (ClOrdID, "B12345678");
        modify.setInteger (AccountType, 1);
        modify.setString (Symbol, "CSCO");
        modify.setSide (GWC_SIDE_BUY);
        modify.setQty (50);
        modify.setOrderType (GWC_ORDER_TYPE_LIMIT);

        cdrDateTime tt;
        getTime (tt);
        modify.setDateTime (TransactTime, tt);

        if (!mGwc->sendModify (modify))
            mLog->info ("failed to send modify ...");
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

        string origcloid;
        msg.getString (ClOrdID, origcloid);

        gwcOrder cancel;
        cancel.setString (OrigClOrdID, origcloid);
        cancel.setString (ClOrdID, "C12345678");
        cancel.setString (Symbol, "CSCO");
        cancel.setSide (GWC_SIDE_BUY);

        cdrDateTime tt;
        getTime (tt);
        cancel.setDateTime (TransactTime, tt);

        if (!mGwc->sendCancel (cancel))
            mLog->info ("failed to send cancel ...");
    }

    virtual void onModifyRejected (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onModifyRejected msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onCancelRejected (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onCancelRejected msg...");
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

    properties props (p, "gwc", "millennium", "sim");
    props.setProperty ("host", "127.0.0.1:9899");
    props.setProperty ("begin_string", "FIXT.1.1");
    props.setProperty ("sender_comp_id", "DBL");
    props.setProperty ("target_comp_id", "BME");
    props.setProperty ("data_dictionary", "/home/colinp/dev/blu-corner/fosdk/examples/FIXT11.xml");
    props.setProperty ("seqno_cache", "/tmp/bme.cache");

    logger* log = logService::getLogger ("BME_TEST");

    sessionCallbacks sessionCbs;
    sessionCbs.mLog = log;

    messageCallbacks messageCbs;
    messageCbs.mLog = log;

    gwcConnector* gwc = gwcConnectorFactory::get (log, "fix", props);
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
    order.setString (ClOrdID, "A12345678");
    order.setInteger (AccountType, 1);
    order.setString (Symbol, "CSCO");
    order.setSide (GWC_SIDE_BUY);
    order.setQty (100);
    order.setOrderType (GWC_ORDER_TYPE_LIMIT);
    order.setString (ReceivedDeptID, "T");

    cdrDateTime tt;
    getTime (tt);
    order.setDateTime (TransactTime, tt);

    log->info ("sending new order");
    if (!gwc->sendOrder (order))
        log->info ("failed to send order ...");

    sleep (5);
    log->info ("destroying connector");
    gwc->stop ();

    log->info ("waiting for logoff...");
    gwc->waitForLogoff ();

    return 0;
}
