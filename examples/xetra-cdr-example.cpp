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
        //static bool response = false;
        mLog->info ("session logging on...");
        
        msg.setInteger (PartyIDSessionID, 123456789);
        msg.setString (Password, "password");

        msg.setString (ApplicationSystemName, "Test");
        msg.setString (ApplicationSystemVersion, "1.0");
        msg.setString (ApplicationSystemVendor, "Blucorner");
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

    virtual void onTraderLogonOn (std::string traderId, const cdr& msg)
    {
        mLog->info ("trader logged on...");
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

    properties props (p, "gwc", "xetra", "sim");
    props.setProperty ("host", "127.0.0.1:9999");
    props.setProperty ("partition", "58");

    logger* log = logService::getLogger ("XETRA_TEST");

    sessionCallbacks sessionCbs;

    sessionCbs.mLog = log;
    messageCallbacks messageCbs;
    messageCbs.mLog = log;

    gwcConnector* gwc = gwcConnectorFactory::get (log, "xetra", props);

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

    cdr traderLogon;
    traderLogon.setInteger (Username, 123456789);
    traderLogon.setString (Password, "password");
    string id ("123456789");
    if (gwc->traderLogon (id, &traderLogon) == false)
        errx (1, "failed to send trader logon");
#if 0
    /* uncomment to test multiple trader login, trader logout */
    if (gwc->traderLogon (id, &traderLogon) == false)
        errx (1, "failed to send trader logon");

    cdr traderLogout;
    traderLogout.setInteger (BodyLen, 0);
    traderLogout.setInteger (TemplateID, 10029);
    traderLogout.setInteger (Username, 123456789);
    gwc->sendMsg (traderLogout);
#endif

    /* send order */
    /* uncomment to test sending orders */
    int64_t clOrdId = time (NULL);
    gwcOrder border;
    border.setPrice (1234.45);
    border.setQty (50000);
    border.setTif (GWC_TIF_DAY);
    border.setSide (GWC_SIDE_BUY);
    border.setOrderType (GWC_ORDER_TYPE_LIMIT);

    border.setInteger (SenderSubID, 123456789);
    border.setInteger (ClOrdID, clOrdId);
    border.setInteger (SecurityID, 2504860),
    border.setInteger (PartyIDClientID, 0);
    border.setInteger (PartyIdInvestmentDecisionMaker, 2000000003);
    border.setInteger (ExecutingTrader, 2000005140);
    border.setInteger (MarketSegmentID, 52767);
    border.setInteger (ApplSeqIndicator, 0);
    border.setInteger (PriceValidityCheckType, 0);    
    border.setInteger (ValueCheckTypeValue, 0);
    border.setInteger (ValueCheckTypeQuantity, 0);
    border.setInteger (OrderAttributeLiquidityProvision, 0);
    border.setInteger (ExecInst, 2);
    border.setInteger (TradingCapacity, 5);
    border.setInteger (PartyIdInvestmentDecisionMakerQualifier, 24);
    border.setInteger (ExecutingTraderQualifier, 24);

    if (!gwc->sendOrder (border))
        errx  (1, "failed to send border myborder...");

    sleep (5);

#if 0
    gwcOrder sorder;
    sorder.setPrice (1234.45);
    sorder.setQty (10);
    sorder.setTif (GWC_TIF_IOC);
    sorder.setSide (GWC_SIDE_SELL);
    sorder.setOrderType (GWC_ORDER_TYPE_LIMIT);

    sorder.setInteger (SenderSubID, 123456789);
    sorder.setInteger (ClOrdID, time (NULL));
    sorder.setInteger (SecurityID, 2504860),
    sorder.setInteger (PartyIDClientID, 0);
    sorder.setInteger (PartyIdInvestmentDecisionMaker, 2000000003);
    sorder.setInteger (ExecutingTrader, 2000005140);
    sorder.setInteger (MarketSegmentID, 52767);
    sorder.setInteger (ApplSeqIndicator, 0);
    sorder.setInteger (PriceValidityCheckType, 0);    
    sorder.setInteger (ValueCheckTypeValue, 0);
    sorder.setInteger (ValueCheckTypeQuantity, 0);
    sorder.setInteger (OrderAttributeLiquidityProvision, 0);
    sorder.setInteger (ExecInst, 2);
    sorder.setInteger (TradingCapacity, 5);
    sorder.setInteger (PartyIdInvestmentDecisionMakerQualifier, 24);
    sorder.setInteger (ExecutingTraderQualifier, 24);

    if (!gwc->sendOrder (sorder))
        errx  (1, "failed to send sorder myborder...");
    sleep (5);
#endif

    /* send a cancel */
#if 0
    cdr cancel;
    cancel.setInteger (SenderSubID, 123456789);
    cancel.setInteger (ClOrdID, time (NULL));
    cancel.setInteger (OrigClOrdID, clOrdId);
    cancel.setInteger (SecurityID, 2504848);
    cancel.setInteger (PartyIdInvestmentDecisionMaker, 2000000003);
    cancel.setInteger (ExecutingTrader, 2000005140);
    cancel.setInteger (MarketSegmentID, 52755);
    cancel.setInteger (TargetPartyIDSessionID, 901201710);
    cancel.setInteger (PartyIdInvestmentDecisionMakerQualifier, 24);
    cancel.setInteger (ExecutingTraderQualifier, 24);
    if (!gwc->sendCancel (cancel))
        errx  (1, "failed to send cancel...");
    sleep (5);

#endif
    gwc->stop ();

    sleep (2);
    log->info ("destroying connector");

    return 0;
}
