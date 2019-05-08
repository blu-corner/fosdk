/** Testbed for gwcConnector **/

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

        string orderId;
        msg.getString (OrderID, orderId);
        /* send modify */
        cdr modify;
        modify.setString (OriginalClientOrderID, "myorder");
        modify.setString (ClientOrderID, "myorder1");
        modify.setString (OrderID, orderId);
        modify.setInteger (InstrumentID, 133215); // VOD.L
        modify.setInteger (OrderQty, 2000);
        modify.setInteger (OrderType, 2);
        modify.setDouble (LimitPrice, 1234.56);
        modify.setInteger (Side, 1);

        modify.setString (Account, "account");
        modify.setInteger (ExpireDateTime, 0);
        modify.setInteger (DisplayQty, 0);
        modify.setDouble (StopPrice, 0.0);
        modify.setInteger (PassiveOnlyOrder, 0);
        modify.setInteger (ClientID, 1234);
        modify.setInteger (MinimumQuantity, 0);
        modify.setInteger (PassiveOnlyOrder, 0);
        modify.setInteger (ReservedField1, 0);
        modify.setInteger (ReservedField2, 0);
        modify.setInteger (ReservedField3, 0);
        modify.setInteger (ReservedField4, 0);
    
        if (!mGwc->sendModify (modify))
            mLog->info ("failed to send modify myorder1...");
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
        string orderId;
        msg.getString (OrderID, orderId);
        cdr cancel;
        cancel.setString (OriginalClientOrderID, "myorder1");
        cancel.setString (ClientOrderID, "myorder2");
        cancel.setString (OrderID, orderId);
        cancel.setInteger (InstrumentID, 133215); // VOD.L
        cancel.setInteger (Side, 1);
        cancel.setString (RfqID, "XXXX");

        cancel.setInteger (ReservedField1, 0);
        cancel.setInteger (ReservedField2, 0);
    
        if (!mGwc->sendCancel (cancel))
            mLog->info ("failed to send cancel myorder1...");
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
    // gwcOrder order;
    // order.setPrice (1234.45);
    // order.setQty (1000);
    // order.setTif (GWC_TIF_DAY);
    // order.setSide (GWC_SIDE_BUY);
    // order.setOrderType (GWC_ORDER_TYPE_LIMIT);
    // order.setString (ClientOrderID, "myorder");
    // order.setInteger (InstrumentID, 133215); // VOD.L
    // order.setInteger (AutoCancel, 1);
    //
    // order.setString (TraderID, "TX1");
    // order.setString (Account, "account");
    // order.setInteger (ClearingAccount, 1);
    // order.setInteger (FXMiFIDFlags, 0);
    // order.setInteger (PartyRoleQualifiers, 0);
    // order.setInteger (ExpireDateTime, 0);
    // order.setInteger (DisplayQty, 0);
    // order.setInteger (Capacity, 1);
    // order.setInteger (OrderSubType, 0);
    // order.setInteger (Anonymity, 0);
    // order.setDouble (StopPrice, 0.0);
    // order.setInteger (PassiveOnlyOrder, 0);
    // order.setInteger (ClientID, 1234);
    // order.setInteger (InvestmentDecisionMaker, 0);
    // order.setInteger (MinimumQuantity, 0);
    // order.setInteger (ExecutingTrader, 7676);
    //
    // if (!gwc->sendOrder (order))
    //     errx  (1, "failed to send order myorder...");
    //
    // sleep (5);
    // gwc->stop ();
    //
    sleep (10);
    log->info ("destroying connector");
    gwc->stop ();

    return 0;
}
